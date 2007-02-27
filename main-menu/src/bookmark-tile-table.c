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

#include "bookmark-tile-table.h"

#include "tile.h"
#include "libslab-utils.h"

G_DEFINE_TYPE (BookmarkTileTable, bookmark_tile_table, TILE_TABLE_TYPE)

typedef struct {
	GnomeVFSMonitorHandle *store_monitor;
	gchar                 *store_path;
} BookmarkTileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BOOKMARK_TILE_TABLE_TYPE, BookmarkTileTablePrivate))

static void bookmark_tile_table_finalize (GObject *);

static void bookmark_tile_table_update    (TileTable *, TileTableUpdateEvent   *);
static void bookmark_tile_table_uri_added (TileTable *, TileTableURIAddedEvent *);

static void reload_tiles   (TileTable *);
static void update_monitor (BookmarkTileTable *);

static void store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                              GnomeVFSMonitorEventType, gpointer);

static void
bookmark_tile_table_class_init (BookmarkTileTableClass *this_class)
{
	GObjectClass   *g_obj_class      = G_OBJECT_CLASS   (this_class);
	TileTableClass *tile_table_class = TILE_TABLE_CLASS (this_class);

	g_obj_class->finalize = bookmark_tile_table_finalize;

	tile_table_class->reload    = reload_tiles;
	tile_table_class->update    = bookmark_tile_table_update;
	tile_table_class->uri_added = bookmark_tile_table_uri_added;

	g_type_class_add_private (this_class, sizeof (BookmarkTileTablePrivate));
}

static void
bookmark_tile_table_init (BookmarkTileTable *this)
{
	BookmarkTileTablePrivate *priv = PRIVATE (this);

	priv->store_monitor = NULL;
	priv->store_path    = NULL;
}

static void
bookmark_tile_table_finalize (GObject *g_obj)
{
	BookmarkTileTablePrivate *priv = PRIVATE (g_obj);

	if (priv->store_monitor)
		gnome_vfs_monitor_cancel (priv->store_monitor);

	g_free (priv->store_path);

	G_OBJECT_CLASS (bookmark_tile_table_parent_class)->finalize (g_obj);
}

static void
bookmark_tile_table_update (TileTable *this, TileTableUpdateEvent *event)
{
	gchar *path_old;
	gchar *path_new;

	LibSlabBookmarkFile *bm_file_old;
	LibSlabBookmarkFile *bm_file_new;

	GError *error = NULL;

	GList *node;


	path_old = BOOKMARK_TILE_TABLE_GET_CLASS (this)->get_store_path (FALSE);
	path_new = BOOKMARK_TILE_TABLE_GET_CLASS (this)->get_store_path (TRUE);

	if (! path_new)
		goto exit;

	bm_file_old = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file_old, path_old, & error);

	bm_file_new = libslab_bookmark_file_new ();

	for (node = event->tiles; node; node = node->next)
		BOOKMARK_TILE_TABLE_GET_CLASS (this)->update_bookmark_store (
			bm_file_old, bm_file_new, TILE (node->data)->uri);

	if (error) {
		g_error_free (error);
		error = NULL;
	}

	libslab_bookmark_file_to_file (bm_file_new, path_new, & error);

	if (error)
		libslab_handle_g_error (
			& error,
			"%s: couldn't save bookmark file [%s]",
			G_STRFUNC, path_new);

	libslab_bookmark_file_free (bm_file_old);
	libslab_bookmark_file_free (bm_file_new);

	g_free (path_new);

exit:

	g_free (path_old);
}

static void
bookmark_tile_table_uri_added (TileTable *this, TileTableURIAddedEvent *event)
{
	gchar *path_old;
	gchar *path_new;

	LibSlabBookmarkFile *bm_file;

	GError *error = NULL;


	path_old = BOOKMARK_TILE_TABLE_GET_CLASS (this)->get_store_path (FALSE);
	path_new = BOOKMARK_TILE_TABLE_GET_CLASS (this)->get_store_path (TRUE);

	if (! path_new)
		goto exit;

	bm_file = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path_old, & error);

	if (! (error || libslab_bookmark_file_has_item (bm_file, event->uri))) {
		BOOKMARK_TILE_TABLE_GET_CLASS (this)->update_bookmark_store (
			NULL, bm_file, event->uri);

		libslab_bookmark_file_to_file (bm_file, path_new, & error);

		if (error)
			libslab_handle_g_error (
				& error,
				"%s: couldn't save bookmark file [%s]",
				G_STRFUNC, path_new);
	}
	else if (error)
		libslab_handle_g_error (
			& error,
			"%s: couldn't open bookmark file [%s]",
			G_STRFUNC, path_old);
	else
		/* do nothing */ ;

	libslab_bookmark_file_free (bm_file);

	g_free (path_new);

exit:

	g_free (path_old);
}

static void
reload_tiles (TileTable *this)
{
	gchar *path;
	LibSlabBookmarkFile *bm_file;
	gchar **uris = NULL;

	GtkWidget *tile;

	GList *tiles = NULL;

	GError *error = NULL;

	gint i;


	path = BOOKMARK_TILE_TABLE_GET_CLASS (this)->get_store_path (FALSE);

	bm_file = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path, & error);

	if (! error) {
		uris = libslab_bookmark_file_get_uris (bm_file, NULL);

		for (i = 0; uris && uris [i]; ++i) {
			tile = BOOKMARK_TILE_TABLE_GET_CLASS (this)->get_tile (bm_file, uris [i]);

			if (tile)
				tiles = g_list_append (tiles, tile);
		}

		g_object_set (G_OBJECT (this), TILE_TABLE_TILES_PROP, tiles, NULL);

		g_strfreev (uris);
	}
	else
		libslab_handle_g_error (
			& error,
			"%s: couldn't load bookmark file [%s]",
			G_STRFUNC, path);

	libslab_bookmark_file_free (bm_file);
	g_list_free (tiles);

	update_monitor (BOOKMARK_TILE_TABLE (this));
}

static void
update_monitor (BookmarkTileTable *this)
{
	BookmarkTileTablePrivate *priv = PRIVATE (this);

	gchar *path;
	gchar *uri;


	path = BOOKMARK_TILE_TABLE_GET_CLASS (this)->get_store_path (FALSE);

	if (libslab_strcmp (path, priv->store_path)) {
		g_free (priv->store_path);
		priv->store_path = path;

		uri = g_filename_to_uri (path, NULL, NULL);

		if (priv->store_monitor)
			gnome_vfs_monitor_cancel (priv->store_monitor);

		gnome_vfs_monitor_add (
			& priv->store_monitor, uri,
			GNOME_VFS_MONITOR_FILE, store_monitor_cb, this);

		g_free (uri);
	}
}

static void
store_monitor_cb (
	GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	reload_tiles (TILE_TABLE (user_data));
}
