/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006 Novell, Inc.
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

#include "main-menu-common.h"

#include <string.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-monitor.h>

#include "recent-files.h"
#include "egg-recent-item.h"

#include "slab-gnome-util.h"

static void main_menu_engine_class_init (MainMenuEngineClass *);
static void main_menu_engine_init (MainMenuEngine *);
static void main_menu_engine_dispose (GObject *);

typedef struct
{
	MainMenuConf *conf;
	MainMenuUI *ui;
} MainMenuEnginePrivate;

#define MAIN_MENU_ENGINE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_ENGINE_TYPE, MainMenuEnginePrivate))

GType
main_menu_engine_get_type (void)
{
	static GType object_type = 0;

	if (!object_type)
	{
		static const GTypeInfo object_info = {
			sizeof (MainMenuEngineClass),
			NULL,
			NULL,
			(GClassInitFunc) main_menu_engine_class_init,
			NULL,
			NULL,
			sizeof (MainMenuEngine),
			0,
			(GInstanceInitFunc) main_menu_engine_init
		};

		object_type =
			g_type_register_static (G_TYPE_OBJECT, "MainMenuEngine", &object_info, 0);
	}

	return object_type;
}

static void
main_menu_engine_class_init (MainMenuEngineClass * main_menu_engine_class)
{
	GObjectClass *g_obj_class = (GObjectClass *) main_menu_engine_class;

	g_obj_class->dispose = main_menu_engine_dispose;

	g_type_class_add_private (main_menu_engine_class, sizeof (MainMenuEnginePrivate));
}

static void
main_menu_engine_init (MainMenuEngine * main_menu_engine)
{
}

MainMenuEngine *
main_menu_engine_new (MainMenuConf * conf)
{
	MainMenuEngine *engine;
	MainMenuEnginePrivate *priv;

	engine = g_object_new (MAIN_MENU_ENGINE_TYPE, NULL);
	priv = MAIN_MENU_ENGINE_GET_PRIVATE (engine);

	priv->conf = conf;
	priv->ui = NULL;

	return engine;
}

static void
main_menu_engine_dispose (GObject * obj)
{
}

void
main_menu_engine_link_ui (MainMenuEngine * engine, MainMenuUI * ui)
{
	MainMenuEnginePrivate *priv = MAIN_MENU_ENGINE_GET_PRIVATE (engine);

	priv->ui = ui;
}

gboolean
main_menu_engine_search_available (MainMenuEngine * engine)
{
	const gchar *beagle_home;
	gchar *socket_dir;
	gchar *socket_path;

	gchar *remote_storage_dir;

	struct stat buf;

	gboolean path_is_on_block_device;

	gchar *tmp;

	beagle_home = g_getenv ("BEAGLE_HOME");

	if (!beagle_home)
		beagle_home = g_get_home_dir ();

	if (stat (beagle_home, &buf) < 0)
		path_is_on_block_device = FALSE;
	else
		path_is_on_block_device = (buf.st_dev >> 8 != 0);

	if (!path_is_on_block_device || g_getenv ("BEAGLE_SYNCHRONIZE_LOCALLY") != NULL)
	{
		remote_storage_dir =
			g_build_filename (beagle_home, ".beagle", "remote_storage_dir", NULL);

		if (!g_file_test (remote_storage_dir, G_FILE_TEST_EXISTS))
		{
			g_free (remote_storage_dir);

			return FALSE;
		}

		if (!g_file_get_contents (remote_storage_dir, &socket_dir, NULL, NULL))
		{
			g_free (remote_storage_dir);

			return FALSE;
		}

		g_free (remote_storage_dir);

		/* There's a newline at the end that we want to strip off */
		tmp = strrchr (socket_dir, '\n');

		if (tmp != NULL)
			*tmp = '\0';

		if (!g_file_test (socket_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
		{
			g_free (socket_dir);

			return FALSE;
		}
	}
	else
		socket_dir = g_build_filename (beagle_home, ".beagle", NULL);

	socket_path = g_build_filename (socket_dir, "socket", NULL);

	g_free (socket_dir);

	if (stat (socket_path, &buf) == -1 || !S_ISSOCK (buf.st_mode))
	{
		g_free (socket_path);

		return FALSE;
	}

	g_free (socket_path);

	return TRUE;
}

void
main_menu_engine_execute_search (MainMenuEngine * engine, const gchar * search_string)
{
	MainMenuEnginePrivate *priv = MAIN_MENU_ENGINE_GET_PRIVATE (engine);

	const gchar *cmd_template;

	GString *cmd;
	gint pivot;
	gchar **argv;

	GError *error = NULL;

	if (strlen (search_string) < 1)
		return;

	cmd_template = priv->conf->search_command;

	if (!cmd_template || strlen (cmd_template) < 1)
	{
		g_warning ("could not find search command!\n");

		return;
	}

	pivot = strstr (cmd_template, "SEARCH_STRING") - cmd_template;

	cmd = g_string_new_len (cmd_template, pivot);
	g_string_append (cmd, search_string);
	g_string_append (cmd, &cmd_template[pivot + strlen ("SEARCH_STRING")]);

	argv = g_strsplit (cmd->str, " ", -1);

	g_string_free (cmd, TRUE);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

	if (error)
	{
		g_warning ("error: [%s]\n", error->message);

		g_error_free (error);
	}

	g_strfreev (argv);

	main_menu_ui_reset_search (priv->ui);
	main_menu_ui_close (priv->ui, FALSE);
}

GList *
main_menu_engine_get_system_list (MainMenuEngine * engine)
{
	MainMenuEnginePrivate *priv = MAIN_MENU_ENGINE_GET_PRIVATE (engine);

	return priv->conf->system_area_conf->system_list;
}

void
main_menu_engine_add_user_app (MainMenuEngine * engine, const gchar * desktop_uri)
{
	MainMenuEnginePrivate *priv = MAIN_MENU_ENGINE_GET_PRIVATE (engine);

	if (!main_menu_conf_find_user_app_by_uri (priv->conf, desktop_uri))
	{
		priv->conf->file_area_conf->user_specified_apps =
			g_list_append (priv->conf->file_area_conf->user_specified_apps,
			g_strdup (desktop_uri));

		main_menu_conf_sync (priv->conf, priv->conf->file_area_conf->user_specified_apps);
	}
}

