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

#include "recent-apps-tile-table.h"

#include <string.h>

#include "recent-files.h"
#include "application-tile.h"
#include "libslab-utils.h"

#define APP_BLACKLIST_GCONF_KEY    "/desktop/gnome/applications/main-menu/file-area/file_blacklist"
#define DISABLE_TERMINAL_GCONF_KEY "/desktop/gnome/lockdown/disable_command_line"

G_DEFINE_TYPE (RecentAppsTileTable, recent_apps_tile_table, TILE_TABLE_TYPE)

typedef struct {
	MainMenuRecentMonitor *recent_monitor;
} RecentAppsTileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), RECENT_APPS_TILE_TABLE_TYPE, RecentAppsTileTablePrivate))

static void recent_apps_tile_table_finalize (GObject *);

static void reload_tiles (TileTable *);

static gboolean uri_is_in_blacklist (const gchar *);

static void recent_monitor_changed_cb (MainMenuRecentMonitor *, gpointer);

GtkWidget *
recent_apps_tile_table_new ()
{
	RecentAppsTileTable        *this;
	RecentAppsTileTablePrivate *priv;
	
	
	this = g_object_new (
		RECENT_APPS_TILE_TABLE_TYPE,
		"n-columns",             2,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_NONE,
		NULL);

	priv = PRIVATE (this);

	priv->recent_monitor = main_menu_recent_monitor_new ();

	g_signal_connect (
		G_OBJECT (priv->recent_monitor), "changed",
		G_CALLBACK (recent_monitor_changed_cb), this);

	reload_tiles (TILE_TABLE (this));

	return GTK_WIDGET (this);
}

static void
recent_apps_tile_table_class_init (RecentAppsTileTableClass *this_class)
{
	GObjectClass   *g_obj_class      = G_OBJECT_CLASS   (this_class);
	TileTableClass *tile_table_class = TILE_TABLE_CLASS (this_class);


	g_obj_class->finalize = recent_apps_tile_table_finalize;

	tile_table_class->reload = reload_tiles;

	g_type_class_add_private (this_class, sizeof (RecentAppsTileTablePrivate));
}

static void
recent_apps_tile_table_init (RecentAppsTileTable *this)
{
	PRIVATE (this)->recent_monitor = NULL;
}

static void
recent_apps_tile_table_finalize (GObject *g_obj)
{
	g_object_unref (PRIVATE (g_obj)->recent_monitor);

	G_OBJECT_CLASS (recent_apps_tile_table_parent_class)->finalize (g_obj);
}

static void
reload_tiles (TileTable *this)
{
	RecentAppsTileTablePrivate *priv = PRIVATE (this);

	GList              *apps;
	MainMenuRecentFile *app;

	const gchar *uri;

	gboolean blacklisted;

	GList     *tiles = NULL;
	GtkWidget *tile;

	GList *node;


	apps = main_menu_get_recent_apps (priv->recent_monitor);

	for (node = apps; node; node = node->next) {
		app = (MainMenuRecentFile *) node->data;

		uri = main_menu_recent_file_get_uri (app);

		blacklisted =
			libslab_system_item_store_has_uri (uri) ||
			libslab_user_apps_store_has_uri   (uri) ||
			uri_is_in_blacklist               (uri);

		if (! blacklisted) {
			tile = application_tile_new (uri);

			if (tile)
				tiles = g_list_append (tiles, tile);
		}

		g_object_unref (app);
	}

	g_object_set (G_OBJECT (this), TILE_TABLE_TILES_PROP, tiles, NULL);

	g_list_free (apps);
	g_list_free (tiles);
}

static gboolean
uri_is_in_blacklist (const gchar *uri)
{
	GList *blacklist;

	gboolean disable_term;
	gboolean blacklisted;

	GList *node;


	disable_term = GPOINTER_TO_INT (libslab_get_gconf_value (DISABLE_TERMINAL_GCONF_KEY));
	blacklisted  = disable_term && libslab_desktop_item_is_a_terminal (uri);

	if (blacklisted)
		return TRUE;

	blacklist = libslab_get_gconf_value (APP_BLACKLIST_GCONF_KEY);

	for (node = blacklist; node; node = node->next) {
		if (! blacklisted && strstr (uri, (gchar *) node->data))
			blacklisted = TRUE;

		g_free (node->data);
	}

	g_list_free (blacklist);

	return blacklisted;
}

static void
recent_monitor_changed_cb (MainMenuRecentMonitor *monitor, gpointer user_data)
{
	reload_tiles (TILE_TABLE (user_data));
}
