/*
 * This file is part of libslab.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * Libslab is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libslab is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "slab-gnome-util.h"

#include <gconf/gconf-client.h>
#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <string.h>

gboolean
get_slab_gconf_bool (const gchar * key)
{
	GConfClient *gconf_client;
	GError *error;

	gboolean value;

	gconf_client = gconf_client_get_default ();
	error = NULL;

	value = gconf_client_get_bool (gconf_client, key, &error);

	if (error)
		g_warning ("error accessing %s [%s]\n", key, error->message);

	return value;
}

gint
get_slab_gconf_int (const gchar * key)
{
	GConfClient *gconf_client;
	GError *error;

	gint value;

	gconf_client = gconf_client_get_default ();
	error = NULL;

	value = gconf_client_get_int (gconf_client, key, &error);

	if (error)
		g_warning ("error accessing %s [%s]\n", key, error->message);

	return value;
}

gchar *
get_slab_gconf_string (const gchar * key)
{
	GConfClient *gconf_client;
	GError *error;

	gchar *value;

	gconf_client = gconf_client_get_default ();
	error = NULL;

	value = gconf_client_get_string (gconf_client, key, &error);

	if (error)
		g_warning ("error accessing %s [%s]\n", key, error->message);

	return value;
}

void
free_slab_gconf_slist_of_strings (GSList * string_list)
{
	g_assert (string_list != NULL);
	GSList * temp;

	for(temp = string_list; temp; temp = temp->next)
		g_free (temp->data);
	g_slist_free (string_list);
}

GSList *
get_slab_gconf_slist (const gchar * key)
{
	GConfClient *gconf_client;
	GError *error;

	GSList *value;

	gconf_client = gconf_client_get_default ();
	error = NULL;

	value = gconf_client_get_list (gconf_client, key, GCONF_VALUE_STRING, &error);

	if (error)
	{
		g_warning ("error accessing %s [%s]\n", key, error->message);

		g_error_free (error);
	}

	return value;
}

GnomeDesktopItem *
load_desktop_item_from_gconf_key (const gchar * key)
{
	GnomeDesktopItem *item;
	gchar *id = get_slab_gconf_string (key);

	if (!id)
		return NULL;

	item = load_desktop_item_from_unknown (id);
	g_free (id);
	return item;
}

GnomeDesktopItem *
load_desktop_item_from_unknown (const gchar * id)
{
	GnomeDesktopItem *item;
	GError *error;

	error = NULL;

	item = gnome_desktop_item_new_from_uri (id, 0, &error);

	if (!error)
		return item;
	else
	{
		g_error_free (error);
		error = NULL;
	}

	item = gnome_desktop_item_new_from_file (id, 0, &error);

	if (!error)
		return item;
	else
	{
		g_error_free (error);
		error = NULL;
	}

	item = gnome_desktop_item_new_from_basename (id, 0, &error);

	if (!error)
		return item;
	else
	{
		g_error_free (error);
		error = NULL;
	}

	return NULL;
}

gchar *
get_package_name_from_desktop_item (GnomeDesktopItem * desktop_item)
{
	gchar **argv;
	gchar *package_name;
	gint retval;
	GError *error;

	argv = g_new (gchar *, 6);
	argv[0] = "rpm";
	argv[1] = "-qf";
	argv[2] = "--qf";
	argv[3] = "%{NAME}";
	argv[4] = g_filename_from_uri (gnome_desktop_item_get_location (desktop_item), NULL, NULL);
	argv[5] = NULL;

	error = NULL;

	if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &package_name, NULL,
			&retval, &error))
	{
		g_warning ("error: [%s]\n", error->message);

		return NULL;
	}

	g_free (argv[4]);
	g_free (argv);

	if (error)
		g_error_free (error);

	if (!retval)
		return package_name;
	else
		return NULL;
}

gboolean
open_desktop_item_exec (GnomeDesktopItem * desktop_item)
{
	GError *error = NULL;

	if (!desktop_item)
		return FALSE;

	gnome_desktop_item_launch (desktop_item, NULL, GNOME_DESKTOP_ITEM_LAUNCH_ONLY_ONE, &error);

	if (error)
	{
		g_warning ("error launching %s [%s]\n",
			gnome_desktop_item_get_location (desktop_item), error->message);

		return FALSE;
	}

	return TRUE;
}

gboolean
open_desktop_item_help (GnomeDesktopItem * desktop_item)
{
	const gchar *doc_path;
	gchar *help_uri;

	GError *error;

	if (!desktop_item)
		return FALSE;

	doc_path = gnome_desktop_item_get_string (desktop_item, "DocPath");

	if (doc_path)
	{
		help_uri = g_strdup_printf ("ghelp:%s", doc_path);

		error = NULL;

		gnome_url_show (help_uri, &error);

		if (error)
		{
			g_warning ("error opening %s [%s]\n", help_uri, error->message);

			return FALSE;
		}

		g_free (help_uri);
	}
	else
		return FALSE;

	return TRUE;
}

gboolean
desktop_item_is_in_main_menu (GnomeDesktopItem * desktop_item)
{
	return desktop_uri_is_in_main_menu (gnome_desktop_item_get_location (desktop_item));
}

gboolean
desktop_uri_is_in_main_menu (const gchar * uri)
{
	GSList *app_list;

	GSList *node;
	gint offset;

	app_list = get_slab_gconf_slist (SLAB_USER_SPECIFIED_APPS_KEY);

	if (!app_list)
		return FALSE;

	for (node = app_list; node; node = node->next)
	{
		offset = strlen (uri) - strlen ((gchar *) node->data);

		if (offset < 0)
			offset = 0;

		if (!strcmp (&uri[offset], (gchar *) node->data))
			return TRUE;
	}

	return FALSE;
}

gint
desktop_item_location_compare (gconstpointer a_obj, gconstpointer b_obj)
{
	const gchar *a;
	const gchar *b;

	gint offset;

	a = (const gchar *) a_obj;
	b = (const gchar *) b_obj;

	offset = strlen (a) - strlen (b);

	if (offset > 0)
		return strcmp (&a[offset], b);
	else if (offset < 0)
		return strcmp (a, &b[-offset]);
	else
		return strcmp (a, b);
}

gboolean
slab_load_image (GtkImage * image, GtkIconSize size, const gchar * image_id)
{
	GdkPixbuf *pixbuf;
	gint width;
	gint height;

	gchar *id;

	if (!image_id)
		return FALSE;

	id = g_strdup (image_id);

	gtk_icon_size_lookup (size, &width, &height);

	if (g_path_is_absolute (id))
		pixbuf = gdk_pixbuf_new_from_file_at_size (id, width, height, NULL);
	else
	{
		if (	/* file extensions are not copesetic with loading by "name" */
			g_str_has_suffix (id, ".png") ||
			g_str_has_suffix (id, ".svg") ||
			g_str_has_suffix (id, ".xpm")
		   )

			id[strlen (id) - 4] = '\0';

		pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), id, width, 0,
			NULL);
	}

	if (pixbuf)
	{
		gtk_image_set_from_pixbuf (image, pixbuf);

		g_object_unref (pixbuf);

		g_free (id);

		return TRUE;
	}
	else
	{			/* This will make it show the "broken image" icon */
		gtk_image_set_from_file (image, id);

		g_free (id);

		return FALSE;
	}
}

gchar *
string_replace_once (const gchar * str_template, const gchar * key, const gchar * value)
{
	GString *str_built;
	gint pivot;

	pivot = strstr (str_template, key) - str_template;

	str_built = g_string_new_len (str_template, pivot);
	g_string_append (str_built, value);
	g_string_append (str_built, &str_template[pivot + strlen (key)]);

	return g_string_free (str_built, FALSE);
}

void
spawn_process (const gchar * command)
{
	gchar **argv;
	GError *error = NULL;

	if (!command || strlen (command) < 1)
		return;

	argv = g_strsplit (command, " ", -1);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

	if (error)
	{
		g_warning ("error spawning [%s]: [%s]\n", command, error->message);

		g_error_free (error);
	}

	g_strfreev (argv);
}

void
copy_file (const gchar * src_uri, const gchar * dst_uri)
{
	GnomeVFSURI *src;
	GnomeVFSURI *dst;

	GnomeVFSResult retval;

	src = gnome_vfs_uri_new (src_uri);
	dst = gnome_vfs_uri_new (dst_uri);

	retval = gnome_vfs_xfer_uri (src, dst, GNOME_VFS_XFER_DEFAULT,
		GNOME_VFS_XFER_ERROR_MODE_ABORT, GNOME_VFS_XFER_OVERWRITE_MODE_SKIP, NULL, NULL);

	if (retval != GNOME_VFS_OK)
		g_warning ("error copying [%s] to [%s].", src_uri, dst_uri);

	gnome_vfs_uri_unref (src);
	gnome_vfs_uri_unref (dst);
}
