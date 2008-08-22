/*
 *  main-menu.c
 * 
 *  Copyright (C) 2004, 2005 Free Software Foundation, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Christian Neumair <chris@gnome-de.org>
 * 
 */

#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#include "nautilus-main-menu.h"

#include <libnautilus-extension/nautilus-extension-types.h>
#include <libintl.h>

static GType type_list[1];

void
nautilus_module_initialize (GTypeModule *module)
{
	g_print ("Initializing nautilus-main-menu extension\n");

	nautilus_main_menu_register_type (module);
	type_list[0] = NAUTILUS_TYPE_MAIN_MENU;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

void
nautilus_module_shutdown (void)
{
	g_print ("Shutting down nautilus-main-menu extension\n");
}

void 
nautilus_module_list_types (const GType **types,
			    int          *num_types)
{
	*types = type_list;
	*num_types = G_N_ELEMENTS (type_list);
}
