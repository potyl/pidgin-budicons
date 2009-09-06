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

#include "module-prefs.h"
#include "module-id.h"

#include "pidgin.h"
#include "gtkutils.h"

// Plugin preferences
#define PLUGIN_PREFS_BASE       "/plugins/" PLUGIN_ID_TYPE "/" PLUGIN_ID_NAME
#define PLUGIN_PREFS(path)      PLUGIN_PREFS_BASE "/" path
#define PLUGIN_PREFS_URL_JSON   PLUGIN_PREFS("url_json")
#define PLUGIN_PREFS_WORKERS    PLUGIN_PREFS("workers")

#define EQ(str1, str2) (g_strcmp0((str1), (str2)) == 0)




//
// Called to initialize the plugin's preferences.
//
void
budicons_prefs_init () {

	// Create the section
	purple_prefs_add_none(PLUGIN_PREFS_BASE);

	purple_prefs_add_string(PLUGIN_PREFS_URL_JSON, CONF_URL_JSON);
	purple_prefs_add_int(PLUGIN_PREFS_WORKERS, CONF_WORKERS);
}


//
// Returns the number of workers to use. The value is taken from the Pidgin
// configuration.
//
guint
budicons_prefs_get_workers () {
	guint workers = (guint) purple_prefs_get_int(PLUGIN_PREFS_WORKERS);
	return workers > 0 ? workers : 1;
}


//
// Returns the URL of the JSON export. The value is taken from the Pidgin
// configuration.
//
const gchar*
budicons_prefs_get_url_json () {
	const gchar *url = purple_prefs_get_string(PLUGIN_PREFS_URL_JSON);
	if (url == NULL || EQ(url, "")) {
		return NULL;
	}
	return url;
}


//
// Create a preference row in the configuration dialog.
//
static void
budicons_pref_row (GtkWidget *table, const char* text, GtkWidget *widget, guint top, guint bottom) {

	// Row with the number of workers
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);

	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, top, bottom);
	gtk_table_attach_defaults(GTK_TABLE(table), widget, 1, 2, top, bottom);
}


//
// This callback gets called when the number of workgers gets changed in the
// configuration window.
//
static void
budicons_pref_workers_changed_callback (GtkSpinButton *spinbutton, gpointer user_data) {
	gint workers = gtk_spin_button_get_value_as_int(spinbutton);
	purple_prefs_set_int(PLUGIN_PREFS_WORKERS, workers);
}


//
// This callback gets called when the URL of the JSON file gets changed in the
// configuration window.
//
static void
budicons_pref_json_url_changed_callback (GtkEditable *editable, gpointer user_data) {
	const gchar *url = gtk_entry_get_text(GTK_ENTRY(editable));
	purple_prefs_set_string(PLUGIN_PREFS_URL_JSON, url);
}


//
// GUI for the plugin preferences.
//
static GtkWidget*
budicons_pref_frame (PurplePlugin *plugin) {

	GtkWidget *ui = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ui), 12);

	GtkWidget *frame = pidgin_make_frame(ui, "Buddy icons download");

	GtkWidget *table = gtk_table_new(2, 2, TRUE);
	gtk_container_add(GTK_CONTAINER(frame), table);

	guint top = 0, bottom = 1;


	// Row with the JSON file
	const gchar *url = budicons_prefs_get_url_json();
	if (url == NULL) {
		url = "";
	}
	GtkWidget *url_ui = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(url_ui), url);
	budicons_pref_row(
		table,
		"URL of the JSON file",
		url_ui,
		top++, bottom++
	);
	g_signal_connect(
		G_OBJECT(url_ui),
		"changed",
		G_CALLBACK(budicons_pref_json_url_changed_callback),
		NULL
	);


	// Row with the number of workers
	GtkObject *adjustment = gtk_adjustment_new(5, 1, 16, 1, 0, 0);
	GtkWidget *worker_ui = gtk_spin_button_new(GTK_ADJUSTMENT(adjustment), 0, 0);
	guint workers = budicons_prefs_get_workers();
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(worker_ui), workers);
	budicons_pref_row(
		table,
		"Number of simultaneous downloads",
		worker_ui,
		top++, bottom++
	);
	g_signal_connect(
		G_OBJECT(worker_ui),
		"value-changed",
		G_CALLBACK(budicons_pref_workers_changed_callback),
		NULL
	);

	gtk_widget_show_all(ui);
	return ui;
}


//
// Purple plugin preferences dialog
//
PidginPluginUiInfo budicons_prefs_info = {
	budicons_pref_frame,
	0,    // Reserverd for page_num

	NULL, // Reserved 1
	NULL, // Reserved 2
	NULL, // Reserved 3
	NULL, // Reserved 4
};
