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

#ifndef __MAIN_MENU_CONF_H_
#define __MAIN_MENU_CONF_H_

#include <glib-object.h>
#include <libgnome/gnome-desktop-item.h>

G_BEGIN_DECLS
#define MAIN_MENU_CONF_TYPE (main_menu_conf_get_type ())
#define MAIN_MENU_CONF(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), MAIN_MENU_CONF_TYPE, MainMenuConf))
#define MAIN_MENU_CONF_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MAIN_MENU_CONF_TYPE, MainMenuConfClass))
#define IS_MAIN_MENU_CONF(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MAIN_MENU_CONF_TYPE))
#define IS_MAIN_MENU_CONF_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MAIN_MENU_CONF_TYPE))
#define MAIN_MENU_CONF_GET_CLASS(obj) (G_TYPE_CHECK_GET_CLASS ((obj), MAIN_MENU_CONF_TYPE, MainMenuConfClass))
	typedef enum {
	USER_SPECIFIED_APPS = 0,
	RECENTLY_USED_APPS = 1,
	RECENT_FILES = 2,
	FILE_CLASS_SENTINEL = 3	/* must always be last and equal (in value)
				   to the number of possible file class types 
				 */
} FileClass;

typedef enum {
	FILE_REORDERING_SWAP,
	FILE_REORDERING_PUSH,
	FILE_REORDERING_PUSH_PULL
} FileReorderingPriority;

typedef struct {
	FileClass file_class;
	FileReorderingPriority reordering_priority;

	GList *user_specified_apps;
	gint item_limit;
	gboolean disable_command_line;
} FileAreaConf;

typedef struct {
	GList *system_list;

	gchar *help_item;
	gchar *control_center_item;
	gchar *package_manager_item;

	gboolean disable_lock_screen;
	gboolean disable_log_out;
} SystemAreaConf;

typedef struct {
	gboolean search_area_visible;
	gboolean ab_link_visible;
	gboolean system_area_visible;
	gboolean status_area_visible;

	gboolean file_area_showable_types [FILE_CLASS_SENTINEL];
} LockDownConf;

typedef struct {
	GObject parent_placeholder;

	gboolean urgent_close;
	gchar *search_command;
	GnomeDesktopItem *app_browser;
	GnomeDesktopItem *file_browser;

	FileAreaConf *file_area_conf;
	SystemAreaConf *system_area_conf;

	LockDownConf *lock_down_conf;
} MainMenuConf;

typedef struct {
	GObjectClass parent_class;

	void (*file_list_changed) (MainMenuConf *);
} MainMenuConfClass;

GType main_menu_conf_get_type (void);

MainMenuConf *main_menu_conf_new (void);
GList *main_menu_conf_find_user_app_by_uri (MainMenuConf * conf,
					    const gchar * uri);
void main_menu_conf_sync (MainMenuConf * conf, gpointer field);

void main_menu_conf_dump (MainMenuConf * conf);

G_END_DECLS
#endif /* __MAIN_MENU_CONF_H_ */
