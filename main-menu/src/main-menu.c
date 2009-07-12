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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <glib.h>
#include <panel-applet.h>
#include <libgnomeui/libgnomeui.h>
#include <string.h>
#include <libslab/slab.h>

#include "main-menu-ui.h"

#include "main-menu-migration.h"

static gboolean main_menu_applet_init (PanelApplet *, const gchar *, gpointer);

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_MainMenu_Factory", PANEL_TYPE_APPLET, "Main Menu", "0",
	main_menu_applet_init, NULL);

#define CHECKPOINT_CONFIG_BASENAME "main-menu-checkpoint.conf"
#define CHECKPOINT_FILE_BASENAME "main-menu"

static gboolean
main_menu_applet_init (PanelApplet *applet, const gchar *iid, gpointer user_data)
{
	gchar *argv [1] = { "slab" };

	libslab_checkpoint_init (CHECKPOINT_CONFIG_BASENAME, CHECKPOINT_FILE_BASENAME);

	libslab_checkpoint ("Main-menu starts up");

	if (strcmp (iid, "OAFIID:GNOME_MainMenu") != 0)
		return FALSE;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* We need load messages of Network Connection Information
	   dialog from NetworkManager's mo files.
	 */
	bindtextdomain ("NetworkManager", GNOMELOCALEDIR);
	bind_textdomain_codeset ("NetworkManager", "UTF-8");

	textdomain (GETTEXT_PACKAGE);
#endif

	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, 1, argv, NULL, NULL);

	libslab_checkpoint ("Migrating old configurations");
	move_system_area_to_new_set ();
	//migrate_system_gconf_to_bookmark_file    ();
	migrate_user_apps_gconf_to_bookmark_file ();
	migrate_showable_file_types              ();

	libslab_checkpoint ("Creating user interface for whole applet");
	main_menu_ui_new (applet);

	libslab_checkpoint ("Showing all widgets in applet");
	gtk_widget_show_all (GTK_WIDGET (applet));

	libslab_thumbnail_factory_preinit ();

	libslab_checkpoint ("Finished initializing applet");
	return TRUE;
}
