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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <glib.h>
#include <panel-applet.h>
#include <string.h>

#include "main-menu-common.h"
#include "main-menu-conf.h"

static gboolean main_menu_applet_init (PanelApplet *, const gchar *,
				       gpointer);

PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_MainMenu_Factory",
			     PANEL_TYPE_APPLET,
			     "Main Menu", "0", main_menu_applet_init, NULL);

static gboolean
main_menu_applet_init (PanelApplet * applet, const gchar * iid,
		       gpointer user_data)
{
	gchar *argv [1] = { "slab" };

	MainMenuConf *conf;
	MainMenuEngine *engine;
	MainMenuUI *ui;


	if (strcmp (iid, "OAFIID:GNOME_MainMenu") != 0)
		return FALSE;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* We need load messages of Network Connection Information dialog
	   from NetworkManager's mo files. */
	bindtextdomain ("NetworkManager", GNOMELOCALEDIR);
	bind_textdomain_codeset ("NetworkManager", "UTF-8");

	textdomain (GETTEXT_PACKAGE);
#endif

	gnome_program_init (PACKAGE,
			    VERSION, LIBGNOMEUI_MODULE, 1, argv, NULL, NULL);

	conf = main_menu_conf_new ();
	engine = main_menu_engine_new (conf);
	ui = main_menu_ui_new (applet, conf, engine);

	main_menu_engine_link_ui (engine, ui);

	main_menu_ui_release (ui);

	return TRUE;
}
