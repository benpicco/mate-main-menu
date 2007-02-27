/*
 *  nautilus-main-menu.c
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

#include "nautilus-main-menu.h"

#include <libnautilus-extension/nautilus-menu-provider.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmain.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <string.h> /* for strcmp */
#include <unistd.h> /* for chdir */

static void nautilus_main_menu_instance_init (NautilusMainMenu      *cvs);
static void nautilus_main_menu_class_init    (NautilusMainMenuClass *class);

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
get_menu_file_info (NautilusFileInfo *file_info)
{
	MenuFileInfo  ret;
	gchar            *mime_type;
	
	g_assert (file_info);

	mime_type = nautilus_file_info_get_mime_type (file_info);
	g_print ("Checking mime type %s\n", mime_type);
	
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
main_menu_callback (NautilusMenuItem *item,
			NautilusFileInfo *file_info)
{
	static GConfClient *client;

	client = gconf_client_get_default ();

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

static NautilusMenuItem *
main_menu_menu_item_new (MenuFileInfo  menu_file_info)
{
	NautilusMenuItem *ret;
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

	ret = nautilus_menu_item_new ("NautilusMainMenu::main_menu",
				      name, tooltip, "gnome-fs-client");

	return ret;
}

static GList *
nautilus_main_menu_get_file_items (NautilusMenuProvider *provider,
				       GtkWidget            *window,
				       GList                *files)
{
	NautilusMenuItem *item;
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
nautilus_main_menu_menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
	iface->get_file_items = nautilus_main_menu_get_file_items;
}

static void 
nautilus_main_menu_instance_init (NautilusMainMenu *cvs)
{
}

static void
nautilus_main_menu_class_init (NautilusMainMenuClass *class)
{
}

GType
nautilus_main_menu_get_type (void) 
{
	return menu_type;
}

void
nautilus_main_menu_register_type (GTypeModule *module)
{
	static const GTypeInfo info = {
		sizeof (NautilusMainMenuClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) nautilus_main_menu_class_init,
		NULL, 
		NULL,
		sizeof (NautilusMainMenu),
		0,
		(GInstanceInitFunc) nautilus_main_menu_instance_init,
	};

	static const GInterfaceInfo menu_provider_iface_info = {
		(GInterfaceInitFunc) nautilus_main_menu_menu_provider_iface_init,
		NULL,
		NULL
	};

	menu_type = g_type_module_register_type (module,
						     G_TYPE_OBJECT,
						     "NautilusMainMenu",
						     &info, 0);

	g_type_module_add_interface (module,
				     menu_type,
				     NAUTILUS_TYPE_MENU_PROVIDER,
				     &menu_provider_iface_info);
}
