/*
 *  caja-main-menu.c
 * 
 *  Copyright (C) 2006 Novell, Inc.
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
 *  Author: JP Rosevear <jpr@novell.com>
 * 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "caja-main-menu.h"

#include <libcaja-extension/caja-menu-provider.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <string.h> /* for strcmp */
#include <unistd.h> /* for chdir */

static void caja_main_menu_instance_init (CajaMainMenu      *cvs);
static void caja_main_menu_class_init    (CajaMainMenuClass *class);

static GType menu_type = 0;

typedef enum {
	FILE_INFO_LAUNCHER,
	FILE_INFO_DOCUMENT,
	FILE_INFO_LAUNCHER_RM,
	FILE_INFO_DOCUMENT_RM,
	FILE_INFO_OTHER,
} MenuFileInfo;

static const char * const document_blacklist [] =
{
	"application/x-executable"
};

static MenuFileInfo
get_menu_file_info (CajaFileInfo *file_info)
{
	MenuFileInfo  ret;
	gchar            *mime_type;
	
	g_assert (file_info);

	mime_type = caja_file_info_get_mime_type (file_info);
	
	if (strcmp (mime_type, "application/x-desktop") == 0) {
		ret = FILE_INFO_LAUNCHER;
	} else {
		int i;
		
		ret = FILE_INFO_DOCUMENT;

		for (i = 0; i < G_N_ELEMENTS (document_blacklist); i++) {
			if (strcmp (mime_type, document_blacklist [i]) == 0) {
				ret = FILE_INFO_DOCUMENT;
				break;
			}
		}
	}

	g_free (mime_type);

	return ret;
}

static void
main_menu_callback (CajaMenuItem *item,
			CajaFileInfo *file_info)
{
	switch (get_menu_file_info (file_info)) {
		case FILE_INFO_LAUNCHER:
			/* Add to the launcher favorites */
			break;

		case FILE_INFO_DOCUMENT:
			/* Add to document favorites */
			break;

		case FILE_INFO_OTHER:
		default:
			g_assert_not_reached ();
	}
}

static CajaMenuItem *
main_menu_menu_item_new (MenuFileInfo  menu_file_info)
{
	CajaMenuItem *ret;
	const char *name;
	const char *tooltip;		

	switch (menu_file_info) {
	case FILE_INFO_LAUNCHER:
		name = _("Add to Favorites");
		tooltip = _("Add the current launcher to favorites");
		break;

	case FILE_INFO_DOCUMENT:
		name = _("Add to Favorites");
		tooltip = _("Add the current document to favorites");
		break;

	case FILE_INFO_LAUNCHER_RM:
		name = _("Remove from Favorites");
		tooltip = _("Remove the current document from favorites");
		break;

	case FILE_INFO_DOCUMENT_RM:
		name = _("Remove from Favorites");
		tooltip = _("Remove the current document from favorites");
		break;

	case FILE_INFO_OTHER:
	default:
		g_assert_not_reached ();
	}

	ret = caja_menu_item_new ("CajaMainMenu::main_menu",
				      name, tooltip, "mate-fs-client");

	return ret;
}

static GList *
caja_main_menu_get_file_items (CajaMenuProvider *provider,
				       GtkWidget            *window,
				       GList                *files)
{
	CajaMenuItem *item;
	MenuFileInfo  menu_file_info;
	GList *l;
	
	for (l = files; l != NULL; l = l->next) {
		menu_file_info = get_menu_file_info (l->data);
		
		switch (menu_file_info) {
		case FILE_INFO_LAUNCHER:
		case FILE_INFO_DOCUMENT:
			item = main_menu_menu_item_new (menu_file_info);
			g_signal_connect (item, "activate",
					  G_CALLBACK (main_menu_callback),
					  files->data);

			return g_list_append (NULL, item);
		case FILE_INFO_OTHER:
			continue;
			
		default:
			g_assert_not_reached ();
		}
	}

	return NULL;
}

static void
caja_main_menu_menu_provider_iface_init (CajaMenuProviderIface *iface)
{
	iface->get_file_items = caja_main_menu_get_file_items;
}

static void 
caja_main_menu_instance_init (CajaMainMenu *cvs)
{
}

static void
caja_main_menu_class_init (CajaMainMenuClass *class)
{
}

GType
caja_main_menu_get_type (void) 
{
	return menu_type;
}

void
caja_main_menu_register_type (GTypeModule *module)
{
	static const GTypeInfo info = {
		sizeof (CajaMainMenuClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) caja_main_menu_class_init,
		NULL, 
		NULL,
		sizeof (CajaMainMenu),
		0,
		(GInstanceInitFunc) caja_main_menu_instance_init,
	};

	static const GInterfaceInfo menu_provider_iface_info = {
		(GInterfaceInitFunc) caja_main_menu_menu_provider_iface_init,
		NULL,
		NULL
	};

	menu_type = g_type_module_register_type (module,
						     G_TYPE_OBJECT,
						     "CajaMainMenu",
						     &info, 0);

	g_type_module_add_interface (module,
				     menu_type,
				     CAJA_TYPE_MENU_PROVIDER,
				     &menu_provider_iface_info);
}
