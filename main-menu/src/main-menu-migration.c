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

#include "main-menu-migration.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include <libslab/slab.h>

#define SYSTEM_BOOKMARK_FILENAME "system-items.xbel"
#define SYSTEM_BOOKMARK_MIGRATED_TO_NEW_SET "system-items.migrated"
#define APPS_BOOKMARK_FILENAME   "applications.xbel"

#define SYSTEM_ITEM_MATECONF_KEY    "/desktop/mate/applications/main-menu/system-area/system_item_list"
#define HELP_ITEM_MATECONF_KEY      "/desktop/mate/applications/main-menu/system-area/help_item"
#define CC_ITEM_MATECONF_KEY        "/desktop/mate/applications/main-menu/system-area/control_center_item"
#define PM_ITEM_MATECONF_KEY        "/desktop/mate/applications/main-menu/system-area/package_manager_item"
#define LOCKSCREEN_MATECONF_KEY     "/desktop/mate/applications/main-menu/lock_screen_priority"
#define USER_APPS_MATECONF_KEY      "/desktop/mate/applications/main-menu/file-area/user_specified_apps"
#define SHOWABLE_TYPES_MATECONF_KEY "/desktop/mate/applications/main-menu/lock-down/showable_file_types"

#define LOGOUT_DESKTOP_ITEM      "mate-session-kill.desktop"

void
move_system_area_to_new_set ()
{
	/* for libslab 0.9.12 start with the new default system area because
	* 1. We have significantly changed the default set and want these new values to show up
	* 2. bug (BNC #447550) caused translated title entry to overwrite valid one
		 causing errors and a customized file when user did not explicitly want it
	* 3. bug in the default system-items.xbel ranking also caused a customized file
	     again without the user explicitly wanting one.
	*/
	gchar * filename;
	GFile *file;
	GFileOutputStream *filestream;

	filename = g_build_filename (
		g_get_user_data_dir (), PACKAGE, SYSTEM_BOOKMARK_MIGRATED_TO_NEW_SET, NULL);
	if (! g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		file = g_file_new_for_path (filename);
		filestream = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
		if (filestream)
			g_object_unref (filestream);
		g_object_unref (file);
		g_free (filename);
		
		filename = g_build_filename (
			g_get_user_data_dir (), PACKAGE, SYSTEM_BOOKMARK_FILENAME, NULL);
		if (g_file_test (filename, G_FILE_TEST_EXISTS))
		{
			file = g_file_new_for_path (filename);
			g_file_delete (file, NULL, NULL);
			g_object_unref (file);
		}
	}
	g_free (filename);
}

void
migrate_system_mateconf_to_bookmark_file ()
{
	BookmarkAgent        *agent;
	BookmarkStoreStatus   status;
	BookmarkItem        **items;

	gboolean need_migration;

	GList            *mateconf_system_list;
	gint              system_tile_type;
	MateDesktopItem *ditem;
	gchar            *path;
	const gchar      *loc;
	gchar            *uri;
	const gchar      *name;

	GBookmarkFile *bm_file;
	gchar         *bm_dir;
	gchar         *bm_path;

	GList  *screensavers;
	gchar  *exec_string;
	gchar **argv;
	gchar  *cmd_path;

	GError *error = NULL;

	GList *node_i;
	GList *node_j;
	gint   i;


	agent = bookmark_agent_get_instance (BOOKMARK_STORE_SYSTEM);
	g_object_get (G_OBJECT (agent), BOOKMARK_AGENT_STORE_STATUS_PROP, & status, NULL);

	if (status == BOOKMARK_STORE_USER) {
		g_object_get (G_OBJECT (agent), BOOKMARK_AGENT_ITEMS_PROP, & items, NULL);

		for (i = 0; items && items [i]; ++i) {
			if (g_str_has_suffix (items [i]->uri, "yelp.desktop") && ! items [i]->title) {
				items [i]->title = g_strdup (_("Help"));
				bookmark_agent_add_item (agent, items [i]);
			}
			else if (g_str_has_suffix (items [i]->uri, "mate-session-logout.desktop") && ! items [i]->title) {
				items [i]->title = g_strdup (_("Logout"));
				bookmark_agent_add_item (agent, items [i]);
			}
			else if (g_str_has_suffix (items [i]->uri, "mate-session-shutdown.desktop")) {
				items [i]->title = g_strdup (_("Shutdown"));
				bookmark_agent_add_item (agent, items [i]);
			}
		}

		goto exit;
	}
	else if (status == BOOKMARK_STORE_DEFAULT_ONLY)
		goto exit;

	mateconf_system_list = (GList *) libslab_get_mateconf_value (SYSTEM_ITEM_MATECONF_KEY);

	if (! mateconf_system_list)
		goto exit;

	need_migration = g_list_length (mateconf_system_list) != 5;

	for (node_i = mateconf_system_list, i = 0; ! need_migration && node_i; node_i = node_i->next, ++i)
		need_migration |= ! (GPOINTER_TO_INT (node_i->data) == i);

	if (need_migration) {
		bm_dir  = g_build_filename (g_get_user_data_dir (), PACKAGE, NULL);
		g_mkdir_with_parents (bm_dir, 0700);

		bm_file = g_bookmark_file_new ();

		for (node_i = mateconf_system_list; node_i; node_i = node_i->next) {
			system_tile_type = GPOINTER_TO_INT (node_i->data);

			ditem = NULL;

			if (system_tile_type == 0)
				ditem = libslab_mate_desktop_item_new_from_unknown_id (
					(gchar *) libslab_get_mateconf_value (HELP_ITEM_MATECONF_KEY));
			else if (system_tile_type == 1)
				ditem = libslab_mate_desktop_item_new_from_unknown_id (
					(gchar *) libslab_get_mateconf_value (CC_ITEM_MATECONF_KEY));
			else if (system_tile_type == 2)
				ditem = libslab_mate_desktop_item_new_from_unknown_id (
					(gchar *) libslab_get_mateconf_value (PM_ITEM_MATECONF_KEY));
			else if (system_tile_type == 3) {
				screensavers = libslab_get_mateconf_value (LOCKSCREEN_MATECONF_KEY);

				for (node_j = screensavers; node_j; node_j = node_j->next) {
					exec_string = (gchar *) node_j->data;

					g_shell_parse_argv (exec_string, NULL, & argv, NULL);
		
					cmd_path = g_find_program_in_path (argv [0]);

					if (cmd_path) {
						ditem = mate_desktop_item_new ();

						path = g_build_filename (
							g_get_user_data_dir (), PACKAGE, "lockscreen.desktop", NULL);

						mate_desktop_item_set_location_file (ditem, path);
						mate_desktop_item_set_string (
							ditem, MATE_DESKTOP_ITEM_NAME, _("Lock Screen"));
						mate_desktop_item_set_string (
							ditem, MATE_DESKTOP_ITEM_ICON, "mate-lockscreen");
						mate_desktop_item_set_string (
							ditem, MATE_DESKTOP_ITEM_EXEC, exec_string);
						mate_desktop_item_set_boolean (
							ditem, MATE_DESKTOP_ITEM_TERMINAL, FALSE);
						mate_desktop_item_set_entry_type (
							ditem, MATE_DESKTOP_ITEM_TYPE_APPLICATION);
						mate_desktop_item_set_string (
							ditem, MATE_DESKTOP_ITEM_CATEGORIES, "MATE;GTK;");
						mate_desktop_item_set_string (
							ditem, MATE_DESKTOP_ITEM_ONLY_SHOW_IN, "MATE;");

						mate_desktop_item_save (ditem, NULL, TRUE, NULL);

						g_free (path);

						break;
					}

					g_strfreev (argv);
					g_free (cmd_path);
				}

				for (node_j = screensavers; node_j; node_j = node_j->next)
					g_free (node_j->data);

				g_list_free (screensavers);
			}
			else if (system_tile_type == 4)
				ditem = libslab_mate_desktop_item_new_from_unknown_id (LOGOUT_DESKTOP_ITEM);
			else
				ditem = NULL;

			if (ditem) {
				loc = mate_desktop_item_get_location (ditem);

				if (g_path_is_absolute (loc))
					uri = g_filename_to_uri (loc, NULL, NULL);
				else
					uri = g_strdup (loc);
			}
			else
				uri = NULL;

			if (uri) {
				g_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
				g_bookmark_file_add_application (
					bm_file, uri,
					mate_desktop_item_get_localestring (ditem, MATE_DESKTOP_ITEM_NAME),
					mate_desktop_item_get_localestring (ditem, MATE_DESKTOP_ITEM_EXEC));

				name = mate_desktop_item_get_string (ditem, MATE_DESKTOP_ITEM_NAME);

				if (! strcmp (name, "Yelp"))
					g_bookmark_file_set_title (bm_file, uri, _("Help"));
				else if (! strcmp (name, "Session Logout Dialog"))
					g_bookmark_file_set_title (bm_file, uri, _("Logout"));
				else if (! strcmp (name, "System Shutdown Dialog"))
					g_bookmark_file_set_title (bm_file, uri, _("Shutdown"));
			}

			g_free (uri);

			if (ditem)
				mate_desktop_item_unref (ditem);
		}

		bm_path = g_build_filename (bm_dir, SYSTEM_BOOKMARK_FILENAME, NULL);

		if (! g_bookmark_file_to_file (bm_file, bm_path, & error))
			libslab_handle_g_error (& error, "%s: cannot save store [%s]", G_STRFUNC, bm_path);

		g_free (bm_dir);
		g_free (bm_path);
		g_bookmark_file_free (bm_file);
	}

	g_list_free (mateconf_system_list);

exit:

	g_object_unref (agent);
}

void
migrate_user_apps_mateconf_to_bookmark_file ()
{
	BookmarkAgent *agent;
	BookmarkStoreStatus status;

	GList *user_apps_list;

	MateDesktopItem *ditem;
	const gchar      *loc;
	gchar            *uri;

	GBookmarkFile *bm_file;
	gchar         *bm_dir;
	gchar         *bm_path;

	GError *error = NULL;

	GList *node;


	agent = bookmark_agent_get_instance (BOOKMARK_STORE_USER_APPS);
	g_object_get (G_OBJECT (agent), BOOKMARK_AGENT_STORE_STATUS_PROP, & status, NULL);
	g_object_unref (G_OBJECT (agent));

	if (status == BOOKMARK_STORE_USER || status == BOOKMARK_STORE_DEFAULT_ONLY)
		return;

	user_apps_list = (GList *) libslab_get_mateconf_value (USER_APPS_MATECONF_KEY);

	if (! user_apps_list)
		return;

	bm_file = g_bookmark_file_new ();

	for (node = user_apps_list; node; node = node->next) {
		ditem = libslab_mate_desktop_item_new_from_unknown_id ((gchar *) node->data);

		if (ditem) {
			loc = mate_desktop_item_get_location (ditem);

			if (g_path_is_absolute (loc))
				uri = g_filename_to_uri (loc, NULL, NULL);
			else
				uri = g_strdup (loc);
		}
		else
			uri = NULL;

		if (uri) {
			g_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
			g_bookmark_file_add_application (
				bm_file, uri,
				mate_desktop_item_get_localestring (ditem, MATE_DESKTOP_ITEM_NAME),
				mate_desktop_item_get_localestring (ditem, MATE_DESKTOP_ITEM_EXEC));
		}

		g_free (uri);

		if (ditem)
			mate_desktop_item_unref (ditem);

		g_free (node->data);
	}

	bm_dir  = g_build_filename (g_get_user_data_dir (), PACKAGE, NULL);
	bm_path = g_build_filename (bm_dir, APPS_BOOKMARK_FILENAME, NULL);

	g_mkdir_with_parents (bm_dir, 0700);

	if (! g_bookmark_file_to_file (bm_file, bm_path, & error))
		libslab_handle_g_error (& error, "%s: cannot save store [%s]", G_STRFUNC, bm_path);

	g_free (bm_dir);
	g_free (bm_path);
	g_bookmark_file_free (bm_file);
	g_list_free (user_apps_list);
}

void
migrate_showable_file_types ()
{
	GList *showable_files;
	GList *node;

	gchar *mig_lock_path;
	gchar *config_dir;

	gboolean allowed_types [3];
	gboolean need_migration = TRUE;

	gint i;


	mig_lock_path = g_build_filename (
		g_get_user_config_dir (), PACKAGE, "showable_files_migrated", NULL);

	if (g_file_test (mig_lock_path, G_FILE_TEST_EXISTS))
		goto exit;

	showable_files = (GList *) libslab_get_mateconf_value (SHOWABLE_TYPES_MATECONF_KEY);

	for (i = 0; i < 3; ++i)
		allowed_types [i] = FALSE;

	if (g_list_length (showable_files) == 3) {
		for (node = showable_files; node; node = node->next) {
			i = GPOINTER_TO_INT (node->data);

			if (0 <= i && i < 3)
				allowed_types [i] = TRUE;
		}
	}

	g_list_free (showable_files);

	for (i = 0; i < 3; ++i)
		need_migration &= allowed_types [i];

	if (need_migration) {
		showable_files = NULL;

		for (i = 0; i < 6; ++i)
			showable_files = g_list_append (showable_files, GINT_TO_POINTER (i));

		libslab_set_mateconf_value (SHOWABLE_TYPES_MATECONF_KEY, showable_files);

		g_list_free (showable_files);
	}

	config_dir = g_path_get_dirname (mig_lock_path);
	g_mkdir_with_parents (config_dir, 0755);
	g_free (config_dir);

	fclose (g_fopen (mig_lock_path, "w"));

exit:

	g_free (mig_lock_path);
}
