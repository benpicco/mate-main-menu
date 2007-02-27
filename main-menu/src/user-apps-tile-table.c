/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006, 2007 Novell, Inc.
 *
 * The Main Menu is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Main Menu is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Main Menu; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "user-apps-tile-table.h"

#include <string.h>

#include "application-tile.h"
#include "libslab-utils.h"

#define DISABLE_TERMINAL_GCONF_KEY "/desktop/gnome/lockdown/disable_command_line"

G_DEFINE_TYPE (UserAppsTileTable, user_apps_tile_table, BOOKMARK_TILE_TABLE_TYPE)

static void update_store (LibSlabBookmarkFile *, LibSlabBookmarkFile *, const gchar *);

static GtkWidget *get_application_tile (LibSlabBookmarkFile *, const gchar *);

GtkWidget *
user_apps_tile_table_new ()
{
	GObject *this = g_object_new (
		USER_APPS_TILE_TABLE_TYPE,
		"n-columns",             2,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_PUSH_PULL,
		NULL);

	tile_table_reload (TILE_TABLE (this));

	return GTK_WIDGET (this);
}

static void
user_apps_tile_table_class_init (UserAppsTileTableClass *this_class)
{
	BookmarkTileTableClass *bookmark_tile_table_class = BOOKMARK_TILE_TABLE_CLASS (this_class);

	bookmark_tile_table_class->get_store_path        = libslab_get_user_apps_store_path;
	bookmark_tile_table_class->update_bookmark_store = update_store;
	bookmark_tile_table_class->get_tile              = get_application_tile;
}

static void
user_apps_tile_table_init (UserAppsTileTable *this)
{
	/* nothing to init */
}

static void
update_store (LibSlabBookmarkFile *bm_file_old, LibSlabBookmarkFile *bm_file_new,
              const gchar *uri)
{
	gint uri_len;
	
	if (! uri)
		return;

	uri_len = strlen (uri);

	if (! strcmp (& uri [uri_len - 8], ".desktop")) {
		libslab_bookmark_file_set_mime_type   (bm_file_new, uri, "application/x-desktop");
		libslab_bookmark_file_add_application (bm_file_new, uri, NULL, NULL);
	}
}

static GtkWidget *
get_application_tile (LibSlabBookmarkFile *bm_file, const gchar *uri)
{
	gint uri_len = strlen (uri);

	gboolean disable_term;
	gboolean blacklisted;


	disable_term = GPOINTER_TO_INT (libslab_get_gconf_value (DISABLE_TERMINAL_GCONF_KEY));
	blacklisted  = disable_term && libslab_desktop_item_is_a_terminal (uri);

	if (! blacklisted && ! strcmp (& uri [uri_len - 8], ".desktop"))
		return application_tile_new (uri);

	return NULL;
}
