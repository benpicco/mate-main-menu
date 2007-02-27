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

#ifndef __MAIN_MENU_COMMON_H__
#define __MAIN_MENU_COMMON_H__

#include <gtk/gtk.h>
#include <panel-applet.h>

G_BEGIN_DECLS

#define MAIN_MENU_UI_TYPE         (main_menu_ui_get_type ())
#define MAIN_MENU_UI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIN_MENU_UI_TYPE, MainMenuUI))
#define MAIN_MENU_UI_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), MAIN_MENU_UI_TYPE, MainMenuUIClass))
#define IS_MAIN_MENU_UI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIN_MENU_UI_TYPE))
#define IS_MAIN_MENU_UI_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), MAIN_MENU_UI_TYPE))
#define MAIN_MENU_UI_GET_CLASS(o) (G_TYPE_CHECK_GET_CLASS ((o), MAIN_MENU_UI_TYPE, MainMenuUIClass))

typedef struct {
	GObject g_object;
} MainMenuUI;

typedef struct {
	GObjectClass g_object_class;
} MainMenuUIClass;

GType main_menu_ui_get_type (void);

MainMenuUI *main_menu_ui_new (PanelApplet *applet);

G_END_DECLS

#endif
