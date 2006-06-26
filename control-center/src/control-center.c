/*
 * This file is part of the Control Center.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but WITHOUT
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
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkentry.h>
#include <panel-applet.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomeui/libgnomeui.h>
#include <dirent.h>

#include "app-shell.h"
#include "app-shell-startup.h"
#include "slab-gnome-util.h"

void handle_static_action_clicked (Tile * tile, TileEvent * event,
				   gpointer data);
static GSList *get_actions_list ();

#define CONTROL_CENTER_ACTIONS_LIST_KEY   "/desktop/gnome/applications/main-menu/cc_actions_list"
#define CONTROL_CENTER_ACTIONS_SEPARATOR  ";"
#define CONTROL_CENTER_PREFIX             "/desktop/gnome/applications/main-menu/cc_"
#define EXIT_SHELL_ON_STATIC_ACTION       "exit_shell_on_static_action"

static GSList *
get_actions_list ()
{
	GSList *key_list;
	GSList *actions_list = NULL;
	AppAction *action;

	key_list = get_slab_gconf_slist (CONTROL_CENTER_ACTIONS_LIST_KEY);
	if (!key_list) {
		g_warning ("key not found  [%s]\n",
			   CONTROL_CENTER_ACTIONS_LIST_KEY);
		return NULL;
	}

	for (; key_list; key_list = key_list->next) {
		const gchar *entry = (const gchar *) key_list->data;

		action = g_new (AppAction, 1);
		gchar **temp =
			g_strsplit (entry, CONTROL_CENTER_ACTIONS_SEPARATOR,
				    2);
		action->name = g_strdup (temp [0]);
		if ((action->item =
		     load_desktop_item_from_unknown (temp [1])) == NULL) {
			g_warning
				("get_actions_list() - PROBLEM - Can't load %s\n",
				 temp [1]);
			g_strfreev (temp);
			continue;
		}
		g_strfreev (temp);

		actions_list = g_slist_append (actions_list, action);
	}

	return actions_list;
}

void
handle_static_action_clicked (Tile * tile, TileEvent * event, gpointer data)
{
	AppShellData *app_data = (AppShellData *) data;
	GnomeDesktopItem *item =
		(GnomeDesktopItem *) g_object_get_data (G_OBJECT (tile),
							APP_ACTION_KEY);
	open_desktop_item_exec (item);

	if (get_slab_gconf_bool
	    (g_strdup_printf
	     ("%s%s", app_data->gconf_prefix, EXIT_SHELL_ON_STATIC_ACTION)))
		gtk_main_quit ();
}

int
main (int argc, char *argv [])
{
	BonoboApplication *bonobo_app = NULL;
	gboolean hidden = FALSE;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	/* gnome_program_init clears the env variable that we depend on, so
	   call this first */
	if (apss_already_running
	    (argc, argv, &bonobo_app, "GNOME-NLD-ControlCenter")) {
		gnome_program_init ("Gnome Control Center", "0.1",
				    LIBGNOMEUI_MODULE, argc, argv, NULL,
				    NULL);
		gdk_notify_startup_complete ();
		bonobo_debug_shutdown ();
		exit (1);
	}

	gnome_program_init ("Gnome Control Center", "0.1", LIBGNOMEUI_MODULE,
			    argc, argv, NULL, NULL);

	if (argc > 1) {
		if (argc != 2 || strcmp ("-h", argv [1])) {
			printf ("Usage - control-center  [-h]\n");
			printf ("Options: -h : hide on start\n");
			printf ("\tUseful if you want to autostart the control-center singleton so it can get all it's slow loading done\n");
			exit (1);
		}
		hidden = TRUE;
	}

	AppShellData *app_data =
		appshelldata_new ("preferences.menu", NULL,
				  CONTROL_CENTER_PREFIX,
				  GTK_ICON_SIZE_DIALOG);
	generate_categories (app_data);
	GSList *actions = get_actions_list ();

	layout_shell (app_data, _("Filter"), _("Groups"), _("Common Tasks"),
		      actions, handle_static_action_clicked);

	g_signal_connect (bonobo_app, "new-instance",
			  G_CALLBACK (apss_new_instance_cb), app_data);
	create_main_window (app_data, "MyControlCenter", _("Control Center"),
			    "gnome-control-center", 975, 600, hidden);

	if (bonobo_app)
		bonobo_object_unref (bonobo_app);
	bonobo_debug_shutdown ();
	return 0;
}
