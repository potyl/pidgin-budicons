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

#include "module-json.h"

#include "module-user.h"
#include <json-glib/json-glib.h>


static BudiconsUser*
budicons_json_get_user (JsonNode *node) {

	if (node == NULL) {
		g_print("%s called with a NULL node\n", __FUNCTION__);
		return NULL;
	}
	else if (JSON_NODE_TYPE(node) != JSON_NODE_OBJECT) {
		g_print("%s called with a non object node\n", __FUNCTION__);
		return NULL;
	}

	JsonObject *object = json_node_get_object(node);

	BudiconsUser *user = budicons_user_new();

	node = json_object_get_member(object, "id");
	if (node) {
		user->id = g_strdup(json_node_get_string(node));
	}

	node = json_object_get_member(object, "name");
	if (node) {
		user->name = g_strdup(json_node_get_string(node));
	}

	node = json_object_get_member(object, "image");
	if (node) {
		user->image = g_strdup(json_node_get_string(node));
	}

	return user;
}


GHashTable*
budicons_json_parse_users (const gchar *data, gssize length, GError **error) {

	GHashTable* users = NULL;

	// Parse the file
	JsonParser *parser = json_parser_new();
	gboolean ok = json_parser_load_from_data(parser, data, length, error);
	if (! ok) {
		goto cleanup;
	}

	// Get the root element (expecting an object)
	JsonNode *root = json_parser_get_root(parser);
	if (JSON_NODE_TYPE(root) != JSON_NODE_ARRAY) {
		GQuark quark = g_quark_from_string("pidgin-icons-plugin");
		*error = g_error_new_literal(quark, 1, "JSON root node is not an array");
		goto cleanup;
	}

	users = g_hash_table_new_full(
 		g_str_hash,
		g_str_equal,
		NULL, // The hash key will be part of the value
		budicons_user_free
	);

	// Parse the JSON users export
	JsonArray *root_array = json_node_get_array(root);
	GList *list = json_array_get_elements(root_array);
	for (GList *iter = list; iter != NULL; iter = iter->next) {
		JsonNode *node = (JsonNode*) iter->data;
		BudiconsUser *user = budicons_json_get_user(node);
		if (user && user->id) {
			g_hash_table_insert(users, user->id, user);			
		}
		else {
			budicons_user_free(user);
		}
	}
	g_list_free(list);

cleanup:
	if (parser) g_object_unref(parser);
	return users;
}

