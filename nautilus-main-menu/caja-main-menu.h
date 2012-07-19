/*
 *  caja-main-menu.h
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
 *  Author: Christian Neumair <chris@mate-de.org>
 * 
 */

#ifndef CAJA_MAIN_MENU_H
#define CAJA_MAIN_MENU_H

#include <glib-object.h>

G_BEGIN_DECLS

/* Declarations for the open terminal extension object.  This object will be
 * instantiated by caja.  It implements the GInterfaces 
 * exported by libcaja. */


#define CAJA_TYPE_MAIN_MENU	  (caja_main_menu_get_type ())
#define CAJA_MAIN_MENU(o)	  (G_TYPE_CHECK_INSTANCE_CAST ((o), CAJA_TYPE_MAIN_MENU, CajaMainMenu))
#define CAJA_IS_MAIN_MENU(o)	  (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAJA_TYPE_MAIN_MENU))
typedef struct _CajaMainMenu      CajaMainMenu;
typedef struct _CajaMainMenuClass CajaMainMenuClass;

struct _CajaMainMenu {
	GObject parent_slot;
};

struct _CajaMainMenuClass {
	GObjectClass parent_slot;
};

GType caja_main_menu_get_type      (void);
void  caja_main_menu_register_type (GTypeModule *module);

G_END_DECLS

#endif
