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

#include "module-user.h"


BudiconsUser*
budicons_user_new () {
	return g_new0(BudiconsUser, 1);
}


void
budicons_user_free (gpointer data) {
	if (data == NULL) {return;}

	BudiconsUser *user = (BudiconsUser *) data;
	g_free(user->id);
	g_free(user->name);
	g_free(user->image);
	g_free(data);
}


void
budicons_user_print (const BudiconsUser *user) {
	if (user == NULL) {
		return;
	}
	g_print("user.id     = %s\n", user->id);
	g_print("user.name   = %s\n", user->name);
	g_print("user.image  = %s\n", user->image);
	g_print("\n");
}
