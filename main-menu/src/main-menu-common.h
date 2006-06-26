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

#ifndef __MAIN_MENU_COMMON_H__
#define __MAIN_MENU_COMMON_H__

#include <glib-object.h>
#include <panel-applet.h>

#include "main-menu-conf.h"

G_BEGIN_DECLS
#define MAIN_MENU_ENGINE_TYPE         (main_menu_engine_get_type ())
#define MAIN_MENU_ENGINE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIN_MENU_ENGINE_TYPE, MainMenuEngine))
#define MAIN_MENU_ENGINE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), MAIN_MENU_ENGINE_TYPE, MainMenuEngineClass))
#define IS_MAIN_MENU_ENGINE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIN_MENU_ENGINE_TYPE))
#define IS_MAIN_MENU_ENGINE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), MAIN_MENU_ENGINE_TYPE))
#define MAIN_MENU_ENGINE_GET_CLASS(o) (G_TYPE_CHECK_GET_CLASS ((o), MAIN_MENU_ENGINE_TYPE, MainMenuEngineClass))
#define MAIN_MENU_UI_TYPE         (main_menu_ui_get_type ())
#define MAIN_MENU_UI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIN_MENU_UI_TYPE, MainMenuUI))
#define MAIN_MENU_UI_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), MAIN_MENU_UI_TYPE, MainMenuUIClass))
#define IS_MAIN_MENU_UI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIN_MENU_UI_TYPE))
#define IS_MAIN_MENU_UI_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), MAIN_MENU_UI_TYPE))
#define MAIN_MENU_UI_GET_CLASS(o) (G_TYPE_CHECK_GET_CLASS ((o), MAIN_MENU_UI_TYPE, MainMenuUIClass))
	typedef struct {
	GObject parent_placeholder;
} MainMenuEngine;

typedef struct {
	GObjectClass parent_class;
} MainMenuEngineClass;

typedef struct {
	GObject parent_placeholder;
} MainMenuUI;

typedef struct {
	GObjectClass parent_class;
} MainMenuUIClass;


GType main_menu_engine_get_type (void);

MainMenuEngine *main_menu_engine_new (MainMenuConf * conf);

void main_menu_engine_link_ui (MainMenuEngine * engine, MainMenuUI * ui);
gboolean main_menu_engine_search_available (MainMenuEngine * engine);
void main_menu_engine_execute_search (MainMenuEngine * engine,
				      const gchar * search_string);
GList *main_menu_engine_get_system_list (MainMenuEngine * engine);
void main_menu_engine_add_user_app (MainMenuEngine * engine,
				    const gchar * desktop_uri);


GType main_menu_ui_get_type (void);

MainMenuUI *main_menu_ui_new (PanelApplet * applet, MainMenuConf * conf,
			      MainMenuEngine * engine);

void main_menu_ui_release (MainMenuUI * ui);
void main_menu_ui_close (MainMenuUI * ui, gboolean assured);
void main_menu_ui_reset_search (MainMenuUI * ui);

G_END_DECLS
#endif /* __MAIN_MENU_COMMON_H__ */
