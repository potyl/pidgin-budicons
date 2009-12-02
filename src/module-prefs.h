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

#ifndef _MODULE_PREFS_H
#define _MODULE_PREFS_H

#include <glib.h>
#include <plugin.h>
#include "gtkplugin.h"

void
budicons_prefs_init (void);

guint
budicons_prefs_get_workers (void);

const gchar*
budicons_prefs_get_url_json (void);

gboolean
budicons_prefs_get_force_icon_download (void);

gboolean
budicons_prefs_get_force_name_update (void);

PidginPluginUiInfo budicons_prefs_info;

#endif
