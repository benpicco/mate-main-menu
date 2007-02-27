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

#include "user-docs-tile-table.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "document-tile.h"
#include "libslab-utils.h"

G_DEFINE_TYPE (UserDocsTileTable, user_docs_tile_table, BOOKMARK_TILE_TABLE_TYPE)

static void update_store (LibSlabBookmarkFile *, LibSlabBookmarkFile *, const gchar *);

static GtkWidget *get_document_tile (LibSlabBookmarkFile *, const gchar *);

GtkWidget *
user_docs_tile_table_new ()
{
	GObject *this = g_object_new (
		USER_DOCS_TILE_TABLE_TYPE,
		"n-columns",             2,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_PUSH_PULL,
		NULL);

	tile_table_reload (TILE_TABLE (this));

	return GTK_WIDGET (this);
}

static void
user_docs_tile_table_class_init (UserDocsTileTableClass *this_class)
{
	BookmarkTileTableClass *bookmark_tile_table_class = BOOKMARK_TILE_TABLE_CLASS (this_class);

	bookmark_tile_table_class->get_store_path        = libslab_get_user_docs_store_path;
	bookmark_tile_table_class->update_bookmark_store = update_store;
	bookmark_tile_table_class->get_tile              = get_document_tile;
}

static void
user_docs_tile_table_init (UserDocsTileTable *this)
{
	/* nothing to init */
}

static void
update_store (LibSlabBookmarkFile *bm_file_old, LibSlabBookmarkFile *bm_file_new,
              const gchar *uri)
{
	gchar   *mime_type = NULL;
	time_t   modified  = 0;
	gchar  **apps      = NULL;
	gchar  **exec      = NULL;
	gsize    n_apps    = 0;

	GnomeVFSFileInfo        *info;
	GnomeVFSMimeApplication *default_app;

	gint i;


	if (bm_file_old) {
		mime_type = libslab_bookmark_file_get_mime_type    (bm_file_old, uri, NULL);
		modified  = libslab_bookmark_file_get_modified     (bm_file_old, uri, NULL);
		apps      = libslab_bookmark_file_get_applications (bm_file_old, uri, & n_apps, NULL);

		exec = g_new0 (gchar *, n_apps + 1);

		for (i = 0; apps && apps [i]; ++i)
			libslab_bookmark_file_get_app_info (
				bm_file_old, uri, apps [i], & exec [i], NULL, NULL, NULL);

		exec [n_apps] = NULL;
	}

	if (! (mime_type && apps && apps [0])) {
		info = gnome_vfs_file_info_new ();

		gnome_vfs_get_file_info (
			uri, info, 
			GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);

		mime_type = g_strdup (info->mime_type);
		modified  = info->mtime;

		if (mime_type) {
			default_app = gnome_vfs_mime_get_default_application (mime_type);

			g_strfreev (apps);

			apps = g_new0 (gchar *, 2);
			exec = g_new0 (gchar *, 2);

			apps [0] = g_strdup (gnome_vfs_mime_application_get_name (default_app));
			exec [0] = g_strdup (gnome_vfs_mime_application_get_exec (default_app));
			apps [1] = NULL;
			exec [1] = NULL;

			gnome_vfs_mime_application_free (default_app);
		}

		gnome_vfs_file_info_unref (info);
	}

	if (! (mime_type && apps && apps [0]))
		g_warning ("Cannot save info for user doc [%s]\n", uri);
	else {
		libslab_bookmark_file_set_mime_type (bm_file_new, uri, mime_type);
		libslab_bookmark_file_set_modified  (bm_file_new, uri, modified);

		for (i = 0; apps [i]; ++i)
			libslab_bookmark_file_add_application (bm_file_new, uri, apps [i], exec [i]);
	}

	g_free (mime_type);
	g_strfreev (apps);
	g_strfreev (exec);
}

static GtkWidget *
get_document_tile (LibSlabBookmarkFile *bm_file, const gchar *uri)
{
	gchar  *mime_type = NULL;
	time_t  modified  = 0;

	gchar *dir;
	gchar *file;
	gchar *path;
	gchar *uri_new;

	GnomeVFSFileInfo *info;

	GtkWidget *tile;


	if (! strcmp (uri, "BLANK_SPREADSHEET") || ! strcmp (uri, "BLANK_DOCUMENT")) {
		dir = g_build_filename (g_get_home_dir (), "Documents", NULL);
		g_mkdir_with_parents (dir, 0700);

		if (! strcmp (uri, "BLANK_SPREADSHEET"))
			file = g_strconcat (_("New Spreadsheet"), ".ods", NULL);
		else
			file = g_strconcat (_("New Document"), ".odt", NULL);

		path = g_build_filename (dir, file, NULL);

		if (! g_file_test (path, G_FILE_TEST_EXISTS))
			fclose (g_fopen (path, "w"));

		uri_new = g_filename_to_uri (path, NULL, NULL);

		g_free (dir);
		g_free (file);
		g_free (path);
	}
	else
		uri_new = g_strdup (uri);

	if (bm_file) {
		mime_type = libslab_bookmark_file_get_mime_type (bm_file, uri_new, NULL);
		modified  = libslab_bookmark_file_get_modified  (bm_file, uri_new, NULL);
	}

	if (! mime_type) {
		info = gnome_vfs_file_info_new ();
		gnome_vfs_get_file_info (
			uri_new, info, 
			GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);

		mime_type = g_strdup (info->mime_type);
		modified  = info->mtime;

		gnome_vfs_file_info_clear (info);
	}

	if (! mime_type) {
		g_warning ("Cannot make document tile [%s]\n", uri_new);

		tile = NULL;
	}
	else
		tile = document_tile_new (uri_new, mime_type, modified);

	g_free (mime_type);
	g_free (uri_new);

	return tile;
}
