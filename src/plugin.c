/*
 * Copyright (C) 2009 Emmanuel Rodriguez <emmanuel.rodriguez@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define PURPLE_PLUGINS

#include "config.h"

#include "module-json.h"
#include "module-user.h"
#include "module-prefs.h"
#include "module-id.h"
#include "module-utils.h"

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <libsoup/soup.h>

#include "account.h"
#include "plugin.h"
#include "version.h"
#include "buddyicon.h"
#include "pidgin.h"
#include "gtkutils.h"
#include "gtkplugin.h"


//
// Plugin's data
//
typedef struct _BudiconsPlugin {
	PurplePlugin *purple;
	SoupSession  *session;
	GHashTable   *users;
	GSList       *buddies;
	GSList       *buddy_iter; // Do not free as this is an iterator
} BudiconsPlugin;


//
// Data structure passed to each worker. Workers are responsible of processing
// users. Each worker operates one user at a time and the taks are done
// asynchronously. Multiple workes can work concurrently, although each one
// operates on a different user.
//
typedef struct _BudiconsWorker {
	BudiconsPlugin  *plugin;
	PurpleBuddy     *buddy; // Pidgin's user
	guint            id;
} BudiconsWorker;


// Prototypes
gboolean
purple_init_plugin (PurplePlugin *plugin);

static void
budicons_got_json_response (SoupSession *session, SoupMessage *message, gpointer data);

static void
budicons_worker_got_image_response (SoupSession *session, SoupMessage *message, gpointer data);

static void
budicons_worker_iter (BudiconsWorker *worker);

static SoupMessage*
budicons_buddy_update (BudiconsPlugin *plugin, PurpleBuddy *buddy);


//
// Callback that's invoked each time that an user image has been downloaded.
//
// This function sets the buddy's icon based on the downloaded image, that is
// assuming that the picture was successfully downloaded.
//
// This function always invokes a next iteration within the current worker.
//
static void
budicons_got_image_response (SoupSession *session, SoupMessage *message, gpointer data) {

	PurpleBuddy *buddy = (PurpleBuddy *) data;

	char *url = soup_uri_to_string(soup_message_get_uri(message), FALSE);
	g_print("Soup: [%-3d] %s\n", message->status_code, url);
	g_free(url);

	if (! SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
		return;
	}


	const char *mime = soup_message_headers_get_content_type(message->response_headers, NULL);
	if (g_ascii_strncasecmp(mime, "image/", strlen("image/"))) {
		// Wrong mime-type, this isn't an image
		g_print("Soup: content-type '%s' doesn't correspond to an image\n", mime);
		return;
	}


	// Set the icon of the buddy
	const char *content = message->response_body->data;
	gsize length = (gsize) message->response_body->length;
	char *icon = g_memdup(content, length);
	purple_buddy_icons_set_for_user(
		buddy->account,
		buddy->name,
		icon,
		length,
		NULL
	);
}



//
// Callback that's invoked each time that a buddy is added in Pidgin. This is
// usually when the end user manually adds a new buddy.
//
// This callback will try to set the buddy name and icon.
//
//
static void
budicons_buddy_added_callback (PurpleBuddy *buddy, gpointer *data) {
	BudiconsPlugin *plugin = (BudiconsPlugin *) data;

	SoupMessage *message = budicons_buddy_update(plugin, buddy);
	if (message != NULL) {
		soup_session_queue_message(
			plugin->session, message,
			budicons_got_image_response,
			buddy
		);
	}
}


//
// Callback that's invoked each time that an user image has been downloaded.
//
// This function sets the buddy's icon based on the downloaded image, that is
// assuming that the picture was successfully downloaded.
//
// This function always invokes a next iteration within the current worker.
//
static void
budicons_worker_got_image_response (SoupSession *session, SoupMessage *message, gpointer data) {

	BudiconsWorker *worker = (BudiconsWorker *) data;
	budicons_got_image_response(session, message, worker->buddy);

	// Start working on the next user
	budicons_worker_iter(worker);
}


//
// This function updates a given buddy. It first does a lookup the the buddy in
// the internal lookup table of buddies that where defined in the JSON file. If
// the buddy can't be found then no processing is done. Otherwise the buddy's
// name is changed unless if the buddy has already a name. Finally the buddy's
// icon is downloaded unless if the buddy has already an icon. The buddy icon is
// downloaded asynchronously.
//
// Returns a SoupMessage* if the buddy icon has been scheduled for a download.
//
static SoupMessage*
budicons_buddy_update (BudiconsPlugin *plugin, PurpleBuddy *buddy) {
	// Take the next buddy/user to process
	g_print("Buddy %s\n", buddy->name);

	BudiconsUser *user = g_hash_table_lookup(plugin->users, buddy->name);
	if (user == NULL) {return NULL;}

	// Set the buddy's name (alias) if it's still unset
	if (buddy->alias == NULL || EQ(buddy->name, buddy->alias)) {
		g_print("Rename %s to %s\n", buddy->alias, user->name);
		purple_blist_alias_buddy(buddy, user->name);
	}

	// Check if the buddy has already an image
	if (user->image == NULL) {return NULL;}
	PurpleBuddyIcon *icon = purple_buddy_icons_find(buddy->account, buddy->name);
	if (icon != NULL) {
		// This buddy has already an icon
		purple_buddy_icon_unref(icon);
		return NULL;
	}

	// Download the buddy's image since it doesn't have one (asynchronous)
	SoupMessage *message = soup_message_new(SOUP_METHOD_GET, user->image);
	if (message == NULL) {
		g_print("Invalid URL %s for buddy %s\n", user->image, buddy->name);
		return NULL;
	}
	g_print("Download of %s\n", user->image);
	return message;
}


//
// This function processes the next pidgin buddies in the global queue until one
// of them requires an image download. Buddies will be renamed as they are
// pulled from the queue.
//
// If a buddy requires its icon to be downloaded then the worker will stop
// iterating and will schedule the image to be downloaded asynchronously. Once
// the image is downloaded the worker will continue its iteration resuming where
// the last worker left the global queue.
//
// The worker will only update the alias or image of a buddy only if it wasn't
// changed already.
//
static void
budicons_worker_iter (BudiconsWorker *worker) {
	BudiconsPlugin *plugin = (BudiconsPlugin *) worker->plugin;

	while (plugin->buddy_iter) {
		// Take the next buddy/user to process
		worker->buddy = plugin->buddy_iter->data;
		plugin->buddy_iter = plugin->buddy_iter->next;

		SoupMessage *message = budicons_buddy_update(plugin, worker->buddy);
		if (message == NULL) {continue;}

		soup_session_queue_message(plugin->session, message, budicons_worker_got_image_response, worker);
		return;
	}

	// End of this worker as there are no more users to process
	g_print("[%d] End of worker\n", worker->id);
	g_free(worker);
	return;
}


//
// Callback that's invoked once the main JSON file is downloaded.
//
// If the SoupMessage returns a successful response then this callback will
// trigger the processing of the users asynchronously. It will also register a
// a callback that will process all new buddies that are added while Pidgin is
// running.
//
static void
budicons_got_json_response (SoupSession *session, SoupMessage *message, gpointer data) {
	BudiconsPlugin *plugin = (BudiconsPlugin *) data;

	{
		char *url = soup_uri_to_string(soup_message_get_uri(message), FALSE);
		g_print("Downloaded URL %s: %d\n", url, message->status_code);
		g_free(url);
	}

	if (! SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
		g_print("SOUP Message was not successful (%d)\n", message->status_code);
		return;
	}


	const char *buffer = message->response_body->data;
	gsize length = (gsize) message->response_body->length;

	const char *mime = soup_message_headers_get_content_type(message->response_headers, NULL);
	if (! (EQ(mime, "application/json") || EQ(mime, "text/plain"))) {
		g_print("Got the wrong mime type (%s) for the JSON file\n", mime);
		return;
	}


	// Parse the JSON file
	GError *error = NULL;
	plugin->users = budicons_json_parse_users(buffer, length, &error);
	if (plugin->users == NULL) {
		char *url = soup_uri_to_string(soup_message_get_uri(message), FALSE);
		g_print("Failed to parse URL %s: %s\n", url, error->message);
		g_free(url);
		g_error_free(error);
		return;
	}


	// Register a callback for every new buddy added
	purple_signal_connect(
		purple_blist_get_handle(),
		"buddy-added",
		plugin->purple,
		PURPLE_CALLBACK(budicons_buddy_added_callback),
		plugin
	);


	// Collect the buddies to process
	PurpleBuddyList *list = purple_get_blist();
	if (list == NULL) {return;}

	plugin->buddies = NULL;
	for (PurpleBlistNode *group = list->root; group; group = group->next) {
		if (! PURPLE_BLIST_NODE_IS_GROUP(group)) {continue;}

		for (PurpleBlistNode *contact = group->child; contact; contact = contact->next) {
			if (! PURPLE_BLIST_NODE_IS_CONTACT(contact)) {continue;}

			for (PurpleBlistNode *blist = contact->child; blist; blist = blist->next) {
				if (! PURPLE_BLIST_NODE_IS_BUDDY(blist)) {continue;}

				PurpleBuddy *buddy = (PurpleBuddy *) blist;
				plugin->buddies = g_slist_prepend(plugin->buddies, buddy);
			}
		}
	}


	// Start a few workers that will process the buddies registered so far
	plugin->buddy_iter = plugin->buddies;
	guint workers = budicons_prefs_get_workers();
	for (guint i = 0; i < workers; ++i) {

		// No more buddies to process
		if (plugin->buddy_iter == NULL) {break;}

		// Create a new worker
		BudiconsWorker *worker = g_new0(BudiconsWorker, 1);
		worker->plugin = plugin;
		worker->id = i;
		g_print("[%d] Started a new worker\n", worker->id);

		budicons_worker_iter(worker);
	}
}


//
// Called when the plugin is first loaded
//
static gboolean
budicons_plugin_load (PurplePlugin *purple) {
	g_print("Plugin loaded\n");

	// Create the plugin's global data
	BudiconsPlugin *plugin = g_new0(BudiconsPlugin, 1);
	purple->extra = plugin;
	if (plugin == NULL) {
		g_print("Failed to create the plugin's internal data structure\n");
		return FALSE;
	}
	plugin->session = soup_session_async_new();
	plugin->purple = purple;


	// Download the JSON file (asynchronously)
	const gchar *url = budicons_prefs_get_url_json();
	if (url == NULL) {
		g_print("No CONF_URL_JSON was provided\n");
		return TRUE;
	}
	g_print("Download of JSON %s\n", url);
	SoupMessage *message = soup_message_new(SOUP_METHOD_GET, url);
	if (message == NULL) {
		g_print("URL %s can't be parsed\n", url);
		return TRUE;
	}
	soup_session_queue_message(plugin->session, message, budicons_got_json_response, plugin);

	return TRUE;
}



//
// Called when the plugin is unloaded
//
static gboolean
budicons_plugin_unload (PurplePlugin *purple) {

	BudiconsPlugin *plugin = (BudiconsPlugin *) purple->extra;
	purple->extra = NULL;

	if (plugin) {
		purple_signal_disconnect(
			purple_blist_get_handle(),
			"buddy-added",
			plugin->purple,
			PURPLE_CALLBACK(budicons_buddy_added_callback)
		);
		if (plugin->session)  g_object_unref(plugin->session);
		if (plugin->buddies)  g_slist_free(plugin->buddies);
		if (plugin->users)    g_hash_table_destroy(plugin->users);
		g_free(plugin);
	}

	return TRUE;
}



//
// Called when the plugin is initialized
//
static void
budicons_plugin_init (PurplePlugin *purple) {
	budicons_prefs_init();
}


//
// Purple plugin definition
//
static PurplePluginInfo budicons_info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,

	PURPLE_PLUGIN_STANDARD,
	(char *) PIDGIN_PLUGIN_TYPE, // ui_requirements
	0,                           // flags
	NULL,                        // dependencies
	PURPLE_PRIORITY_DEFAULT,

	// id, name, version
	(char *) PLUGIN_ID,
	(char *) PLUGIN_NAME,
	(char *) PLUGIN_VERSION,

	// summary, description, author, homepage
	(char *) "Downloads buddy icons and sets their names",
	(char *) "Sets the icons of the buddies by using an external configuration file.",
	(char *) "Emmanuel Rodriguez <emmanuel.rodriguez@gmail.com>",
	(char *) "http://debian.potyl.com/",

	// load, unload, destroy
	budicons_plugin_load,
	budicons_plugin_unload,
	NULL,

	&budicons_prefs_info, // ui_info
	NULL,                 // extra_info
	NULL,                 // prefs_info
	NULL,                 // actions callback

	NULL, // reserved 1
	NULL, // reserved 2
	NULL, // reserved 3
	NULL, // reserved 4
};


//
// Register the plugin
//
PURPLE_INIT_PLUGIN(budicons, budicons_plugin_init, budicons_info)
