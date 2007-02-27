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

#include "user-dirs-tile-table.h"

#include <string.h>

#include "directory-tile.h"
#include "libslab-utils.h"

#define GTK_BOOKMARKS_FILE ".gtk-bookmarks"

G_DEFINE_TYPE (UserDirsTileTable, user_dirs_tile_table, BOOKMARK_TILE_TABLE_TYPE)

typedef struct {
	GnomeVFSMonitorHandle *gtk_bookmarks_store_monitor;
	gchar *gtk_bookmarks_store_path;
} UserDirsTileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), USER_DIRS_TILE_TABLE_TYPE, UserDirsTileTablePrivate))

static void user_dirs_tile_table_finalize (GObject *);

static void update_store (LibSlabBookmarkFile *, LibSlabBookmarkFile *, const gchar *);

static GtkWidget *get_directory_tile (LibSlabBookmarkFile *, const gchar *);

static void sync_gtk_bookmarks_to_store (UserDirsTileTable *);

static void gtk_bookmarks_store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                                            GnomeVFSMonitorEventType, gpointer);

GtkWidget *
user_dirs_tile_table_new ()
{
	UserDirsTileTable *this;
	UserDirsTileTablePrivate *priv;

	gchar *store_uri;

	
	this = g_object_new (
		USER_DIRS_TILE_TABLE_TYPE,
		"n-columns",             2,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_NONE,
		NULL);

	priv = PRIVATE (this);

	priv->gtk_bookmarks_store_path = g_build_filename (g_get_home_dir (), GTK_BOOKMARKS_FILE, NULL);
	store_uri = g_filename_to_uri (priv->gtk_bookmarks_store_path, NULL, NULL);

	gnome_vfs_monitor_add (
		& priv->gtk_bookmarks_store_monitor, store_uri,
		GNOME_VFS_MONITOR_FILE, gtk_bookmarks_store_monitor_cb, this);

	g_free (store_uri);

	sync_gtk_bookmarks_to_store (this);
	tile_table_reload (TILE_TABLE (this));

	return GTK_WIDGET (this);
}

static void
user_dirs_tile_table_class_init (UserDirsTileTableClass *this_class)
{
	GObjectClass           *g_obj_class               = G_OBJECT_CLASS            (this_class);
	BookmarkTileTableClass *bookmark_tile_table_class = BOOKMARK_TILE_TABLE_CLASS (this_class);


	g_obj_class->finalize = user_dirs_tile_table_finalize;

	bookmark_tile_table_class->get_store_path        = libslab_get_user_dirs_store_path;
	bookmark_tile_table_class->update_bookmark_store = update_store;
	bookmark_tile_table_class->get_tile              = get_directory_tile;

	g_type_class_add_private (this_class, sizeof (UserDirsTileTablePrivate));
}

static void
user_dirs_tile_table_init (UserDirsTileTable *this)
{
	UserDirsTileTablePrivate *priv = PRIVATE (this);

	priv->gtk_bookmarks_store_monitor = NULL;
	priv->gtk_bookmarks_store_path    = NULL;
}

static void
user_dirs_tile_table_finalize (GObject *g_obj)
{
	UserDirsTileTablePrivate *priv = PRIVATE (g_obj);

	if (priv->gtk_bookmarks_store_monitor)
		gnome_vfs_monitor_cancel (priv->gtk_bookmarks_store_monitor);

	g_free (priv->gtk_bookmarks_store_path);

	G_OBJECT_CLASS (user_dirs_tile_table_parent_class)->finalize (g_obj);
}

static void
update_store (LibSlabBookmarkFile *bm_file_old, LibSlabBookmarkFile *bm_file_new,
              const gchar *uri)
{
	gchar    *title;
	gchar    *icon;
	gboolean  found;


	libslab_bookmark_file_set_mime_type   (bm_file_new, uri, "inode/directory");
	libslab_bookmark_file_add_application (bm_file_new, uri, "nautilus", "nautilus --browser %u");

	if (bm_file_old) {
		title = libslab_bookmark_file_get_title (bm_file_old, uri, NULL);
		libslab_bookmark_file_set_title (bm_file_new, uri, title);

		found = libslab_bookmark_file_get_icon (bm_file_old, uri, & icon, NULL, NULL);
		libslab_bookmark_file_set_icon (bm_file_new, uri, icon, NULL);

		g_free (title);
		g_free (icon);
	}
}

static GtkWidget *
get_directory_tile (LibSlabBookmarkFile *bm_file, const gchar *uri)
{
	gchar *title;
	gchar *path;
	gchar *icon = NULL;
	gchar *uri_new = NULL;

	GtkWidget *tile;


	if (! strcmp (uri, "HOME")) {
		uri_new = g_filename_to_uri (g_get_home_dir (), NULL, NULL);
		icon = "gnome-fs-home";
	}
	else if (! strcmp (uri, "DOCUMENTS")) {
		path = g_build_filename (g_get_home_dir (), "Documents", NULL);

		if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			uri_new = g_filename_to_uri (path, NULL, NULL);

		g_free (path);
	}
	else if (! strcmp (uri, "DESKTOP")) {
		path = g_build_filename (g_get_home_dir (), "Desktop", NULL);

		if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			uri_new = g_filename_to_uri (path, NULL, NULL);
			icon = "gnome-fs-desktop";
		}

		g_free (path);
	}
	else
		uri_new = g_strdup (uri);

	if (! strcmp (uri_new, "file:///"))
		icon = "drive-harddisk";
	else if (! strcmp (uri_new, "network:"))
		icon = "network-workgroup";
	else
		/* do nothing */ ;

	if (bm_file)
		title = libslab_bookmark_file_get_title (bm_file, uri, NULL);

	tile = directory_tile_new (uri_new, title, icon);

	g_free (title);
	g_free (uri_new);

	return tile;
}

static void
sync_gtk_bookmarks_to_store (UserDirsTileTable *this)
{
	UserDirsTileTablePrivate *priv = PRIVATE (this);

	gchar *path;

	LibSlabBookmarkFile *bm_file;

	gchar **uris;
	gchar **groups;

	gchar  *buf;
	gchar **folders;

	GError *error = NULL;

	gint i, j;


	path = libslab_get_user_dirs_store_path (FALSE);

	bm_file = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path, NULL);

	uris = libslab_bookmark_file_get_uris (bm_file, NULL);

	for (i = 0; uris && uris [i]; ++i) {
		groups = libslab_bookmark_file_get_groups (bm_file, uris [i], NULL, NULL);

		for (j = 0; groups && groups [j]; ++j) {
			if (! strcmp (groups [j], "gtk-bookmarks")) {
				libslab_bookmark_file_remove_item (bm_file, uris [i], NULL);

				break;
			}
		}

		g_strfreev (groups);
	}

	g_strfreev (uris);

	g_file_get_contents (priv->gtk_bookmarks_store_path, & buf, NULL, NULL);

	folders = g_strsplit (buf, "\n", -1);

	for (i = 0; folders && folders [i]; ++i) {
		if (strlen (folders [i]) > 0) {
			libslab_bookmark_file_set_mime_type (
				bm_file, folders [i], "inode/directory");
			libslab_bookmark_file_add_application (
				bm_file, folders [i], "nautilus", "nautilus --browser %u");
			libslab_bookmark_file_add_group (
				bm_file, folders [i], "gtk-bookmarks");
		}
	}

	g_free (path);

	path = libslab_get_user_dirs_store_path (TRUE);
	libslab_bookmark_file_to_file (bm_file, path, & error);

	if (error)
		libslab_handle_g_error (
			& error,
			"%s: couldn't save bookmark file [%s]",
			G_STRFUNC, path);

	g_strfreev (folders);
	g_free (buf);

	libslab_bookmark_file_free (bm_file);
}

static void
gtk_bookmarks_store_monitor_cb (GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
                                const gchar *info_uri, GnomeVFSMonitorEventType type,
                                gpointer user_data)
{
	sync_gtk_bookmarks_to_store (USER_DIRS_TILE_TABLE (user_data));
}
