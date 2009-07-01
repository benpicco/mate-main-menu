/*
 * This file is part of the Application Browser.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * The Application Browser is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Application Browser is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Control Center; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <panel-applet.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomeui/libgnomeui.h>
#include <dirent.h>
#include <unique/unique.h>
#include <stdlib.h>

#include "app-shell.h"
#include "slab-gnome-util.h"

#define APPLICATION_BROWSER_PREFIX  "/desktop/gnome/applications/main-menu/ab_"
#define NEW_APPS_MAX_ITEMS  (APPLICATION_BROWSER_PREFIX "new_apps_max_items")
#define COMMAND_NEW_INSTANCE 1

static UniqueResponse
unique_app_message_cb (UniqueApp *app, gint command, UniqueMessageData *data,
		       guint time, gpointer user_data)
{
	AppShellData *app_data = user_data;
	gboolean  visible;

	if (command != COMMAND_NEW_INSTANCE)
		return UNIQUE_RESPONSE_PASSTHROUGH;

	g_object_get (app_data->main_app, "visible", &visible, NULL);

	if (!visible)
		show_shell (app_data);


	gtk_window_present (GTK_WINDOW (app_data->main_app));
	gtk_widget_grab_focus (SLAB_SECTION (app_data->filter_section)->contents);

	return UNIQUE_RESPONSE_OK;
}

int
main (int argc, char *argv[])
{
	UniqueApp *unique_app = NULL;
	gboolean hidden = FALSE;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	if (argc > 1)
	{
		if (argc != 2 || strcmp ("-h", argv[1]))
		{
			printf ("Usage - application-browser [-h]\n");
			printf ("Options: -h : hide on start\n");
			printf ("\tUseful if you want to autostart the application-browser singleton so it can get all it's slow loading done\n");
			exit (1);
		}
		hidden = TRUE;
	}

	gnome_program_init ("Gnome Application Browser", "0.1", LIBGNOMEUI_MODULE,
		argc, argv, NULL, NULL);

	unique_app = unique_app_new_with_commands ("org.gnome.MainMenu", NULL,
						   "new_instance", COMMAND_NEW_INSTANCE, NULL);

	if (unique_app_is_running (unique_app))
	{
		unique_app_send_message (unique_app, COMMAND_NEW_INSTANCE, NULL);
		g_object_unref (unique_app);

		return 0;
	}

	NewAppConfig *config = g_new0 (NewAppConfig, 1);
	config->max_items = get_slab_gconf_int (NEW_APPS_MAX_ITEMS);
	config->name = _("New Applications");
	AppShellData *app_data = appshelldata_new ("applications.menu", config,
		APPLICATION_BROWSER_PREFIX, GTK_ICON_SIZE_DND, TRUE, FALSE);
	generate_categories (app_data);

	layout_shell (app_data, _("Filter"), _("Groups"), _("Application Actions"), NULL, NULL);

	g_signal_connect (unique_app, "message-received", G_CALLBACK (unique_app_message_cb), app_data);

	create_main_window (app_data, "MyApplicationBrowser", _("Application Browser"),
		"gnome-fs-client", 940, 600, hidden);

	g_object_unref (unique_app);

	return 0;
};
