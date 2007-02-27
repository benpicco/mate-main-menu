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

#include "recent-docs-tile-table.h"

#include "recent-files.h"
#include "document-tile.h"
#include "libslab-utils.h"

G_DEFINE_TYPE (RecentDocsTileTable, recent_docs_tile_table, TILE_TABLE_TYPE)

typedef struct {
	MainMenuRecentMonitor *recent_monitor;
} RecentDocsTileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), RECENT_DOCS_TILE_TABLE_TYPE, RecentDocsTileTablePrivate))

static void recent_docs_tile_table_finalize (GObject *);

static void reload_tiles (TileTable *);

static void recent_monitor_changed_cb (MainMenuRecentMonitor *, gpointer);

GtkWidget *
recent_docs_tile_table_new ()
{
	RecentDocsTileTable        *this;
	RecentDocsTileTablePrivate *priv;
	
	
	this = g_object_new (
		RECENT_DOCS_TILE_TABLE_TYPE,
		"n-columns",             2,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_NONE,
		NULL);

	priv = PRIVATE (this);

	priv->recent_monitor = main_menu_recent_monitor_new ();

	g_signal_connect (
		G_OBJECT (priv->recent_monitor), "changed",
		G_CALLBACK (recent_monitor_changed_cb), this);

	reload_tiles (TILE_TABLE (this));

	return GTK_WIDGET (this);
}

static void
recent_docs_tile_table_class_init (RecentDocsTileTableClass *this_class)
{
	GObjectClass   *g_obj_class      = G_OBJECT_CLASS   (this_class);
	TileTableClass *tile_table_class = TILE_TABLE_CLASS (this_class);

	g_obj_class->finalize = recent_docs_tile_table_finalize;

	tile_table_class->reload = reload_tiles;

	g_type_class_add_private (this_class, sizeof (RecentDocsTileTablePrivate));
}

static void
recent_docs_tile_table_init (RecentDocsTileTable *this)
{
	PRIVATE (this)->recent_monitor = NULL;
}

static void
recent_docs_tile_table_finalize (GObject *g_obj)
{
	g_object_unref (PRIVATE (g_obj)->recent_monitor);

	G_OBJECT_CLASS (recent_docs_tile_table_parent_class)->finalize (g_obj);
}

static void
reload_tiles (TileTable *this)
{
	RecentDocsTileTablePrivate *priv = PRIVATE (this);

	GList              *docs;
	MainMenuRecentFile *doc;

	const gchar *uri;
	const gchar *mime_type;
	time_t       modified;

	gboolean blacklisted = FALSE;

	GList     *tiles = NULL;
	GtkWidget *tile;

	GList *node;


	docs = main_menu_get_recent_files (priv->recent_monitor);

	for (node = docs; node; node = node->next) {
		doc = (MainMenuRecentFile *) node->data;

		uri       = main_menu_recent_file_get_uri       (doc);
		mime_type = main_menu_recent_file_get_mime_type (doc);
		modified  = main_menu_recent_file_get_modified  (doc);

		blacklisted = libslab_user_docs_store_has_uri (uri);

		if (! blacklisted) {
			tile = document_tile_new (uri, mime_type, modified);

			if (tile)
				tiles = g_list_append (tiles, tile);
		}

		g_object_unref (doc);
	}

	g_object_set (G_OBJECT (this), TILE_TABLE_TILES_PROP, tiles, NULL);

	g_list_free (docs);
	g_list_free (tiles);
}

static void
recent_monitor_changed_cb (MainMenuRecentMonitor *monitor, gpointer user_data)
{
	reload_tiles (TILE_TABLE (user_data));
}
