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

#include "system-tile-table.h"

#include <string.h>

#include "system-tile.h"
#include "libslab-utils.h"

G_DEFINE_TYPE (SystemTileTable, system_tile_table, BOOKMARK_TILE_TABLE_TYPE)

static void update_store (LibSlabBookmarkFile *, LibSlabBookmarkFile *, const gchar *);

static GtkWidget *get_system_tile (LibSlabBookmarkFile *, const gchar *);

GtkWidget *
system_tile_table_new ()
{
	GObject *this = g_object_new (
		SYSTEM_TILE_TABLE_TYPE,
		"n-columns",             1,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_PUSH_PULL,
		NULL);

	tile_table_reload (TILE_TABLE (this));

	return GTK_WIDGET (this);
}

static void
system_tile_table_class_init (SystemTileTableClass *this_class)
{
	BookmarkTileTableClass *bookmark_tile_table_class = BOOKMARK_TILE_TABLE_CLASS (this_class);

	bookmark_tile_table_class->get_store_path        = libslab_get_system_item_store_path;
	bookmark_tile_table_class->update_bookmark_store = update_store;
	bookmark_tile_table_class->get_tile              = get_system_tile;
}

static void
system_tile_table_init (SystemTileTable *this)
{
	/* nothing to init */
}

static void
update_store (LibSlabBookmarkFile *bm_file_old, LibSlabBookmarkFile *bm_file_new,
              const gchar *uri)
{
	gchar *title = NULL;
	gint uri_len;


	if (! uri)
		return;

	uri_len = strlen (uri);

	if (strcmp (& uri [uri_len - 8], ".desktop"))
		return;

	if (bm_file_old && libslab_bookmark_file_has_item (bm_file_old, uri))
		title = libslab_bookmark_file_get_title (bm_file_old, uri, NULL);

	libslab_bookmark_file_set_mime_type (bm_file_new, uri, "application/x-desktop");
	libslab_bookmark_file_add_application (bm_file_new, uri, NULL, NULL);

	if (title)
		libslab_bookmark_file_set_title (bm_file_new, uri, title);

	g_free (title);
}

static GtkWidget *
get_system_tile (LibSlabBookmarkFile *bm_file, const gchar *uri)
{
	gchar *title = NULL;
	gint uri_len;

	GtkWidget *tile;


	if (! uri)
		return NULL;

	uri_len = strlen (uri);

	if (strcmp (& uri [uri_len - 8], ".desktop"))
		return NULL;

	if (libslab_bookmark_file_has_item (bm_file, uri))
		title = libslab_bookmark_file_get_title (bm_file, uri, NULL);

	tile = system_tile_new (uri, title);

	g_free (title);

	return tile;
}
