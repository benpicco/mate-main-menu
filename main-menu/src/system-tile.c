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

#include "system-tile.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-client.h>

#include "slab-gnome-util.h"
#include "gnome-utils.h"

G_DEFINE_TYPE (SystemTile, system_tile, NAMEPLATE_TILE_TYPE)

static void system_tile_finalize (GObject *);
static void system_tile_style_set (GtkWidget *, GtkStyle *);

static void load_image (SystemTile *);
static GtkWidget *create_header (const gchar *);

static void system_tile_activated (Tile *, TileEvent *);

static void system_tile_open (Tile *, TileEvent *, TileAction *);
static void system_tile_logout (Tile *, TileEvent *, TileAction *);
static void system_tile_lock_screen (Tile *, TileEvent *, TileAction *);

typedef struct
{
	SystemTileType type;
	
	GnomeDesktopItem *desktop_item;
	
	gchar *image_id;
	gboolean image_is_broken;
	
	MainMenuConf *conf;
} SystemTilePrivate;

#define SYSTEM_TILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SYSTEM_TILE_TYPE, SystemTilePrivate))

static void system_tile_class_init (SystemTileClass * sys_tile_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (sys_tile_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (sys_tile_class);
	TileClass *tile_class = TILE_CLASS (sys_tile_class);

	g_obj_class->finalize = system_tile_finalize;

	widget_class->style_set = system_tile_style_set;

	tile_class->tile_activated = system_tile_activated;

	g_type_class_add_private (sys_tile_class, sizeof (SystemTilePrivate));
}

GtkWidget *
system_tile_new_with_type (SystemTileType type, MainMenuConf * conf)
{
	SystemTile *tile;
	SystemTilePrivate *priv;

	gchar *uri = NULL;
	GtkWidget *header = NULL;

	GnomeDesktopItem *desktop_item = NULL;
	gchar *image_id = NULL;

	if (!(0 <= type && type < SYSTEM_TILE_TYPE_SENTINEL))
		return NULL;

	switch (type)
	{
	case SYSTEM_TILE_TYPE_HELP:
		desktop_item = load_desktop_item_by_unknown_id (conf->system_area_conf->help_item);

		if (desktop_item)
		{
			image_id =
				g_strdup (gnome_desktop_item_get_localestring (desktop_item,
					"Icon"));
			uri = g_strdup (gnome_desktop_item_get_location (desktop_item));
			header = create_header (_("Help"));
		}

		break;

	case SYSTEM_TILE_TYPE_CONTROL_CENTER:
		desktop_item = load_desktop_item_by_unknown_id (
			conf->system_area_conf->control_center_item);

		if (desktop_item)
		{
			image_id = g_strdup (
				gnome_desktop_item_get_localestring (desktop_item, "Icon"));
			uri = g_strdup (gnome_desktop_item_get_location (desktop_item));
			header = create_header (_("Control Center"));
		}

		break;

	case SYSTEM_TILE_TYPE_PACKAGE_MANAGER:
		desktop_item = load_desktop_item_by_unknown_id (
			conf->system_area_conf->package_manager_item);

		if (desktop_item)
		{
			image_id = g_strdup (
				gnome_desktop_item_get_localestring (desktop_item, "Icon"));
			uri = g_strdup (gnome_desktop_item_get_location (desktop_item));
			header = create_header (_("Install Software"));
		}

		break;

	case SYSTEM_TILE_TYPE_LOG_OUT:
		image_id = "gnome-logout";
		header = create_header (_("Log Out ..."));
		uri = "system-tile://logout";

		break;

	case SYSTEM_TILE_TYPE_LOCK_SCREEN:
		image_id = "gnome-lockscreen";
		header = create_header (_("Lock Screen ..."));
		uri = "system-tile://lockscreen";

		break;

	default:
		break;
	}

	if (!uri)
		return NULL;

	if (!header)
	{
		g_warning ("Unable to make header for SystemTileType:%d\n", type);

		header = create_header (_("Unknown"));
	}

	tile = g_object_new (SYSTEM_TILE_TYPE, "tile-uri", uri, "nameplate-image", gtk_image_new (),
		"nameplate-header", header, "nameplate-subheader", NULL, NULL);

	TILE (tile)->actions = g_new0 (TileAction *, 1);
	TILE (tile)->n_actions = 1;

	TILE (tile)->actions[SYSTEM_TILE_ACTION_OPEN] =
		tile_action_new (TILE (tile), system_tile_open, NULL, TILE_ACTION_OPENS_NEW_WINDOW);

	TILE (tile)->default_action = TILE (tile)->actions[SYSTEM_TILE_ACTION_OPEN];

	priv = SYSTEM_TILE_GET_PRIVATE (tile);
	priv->type = type;
	priv->desktop_item = desktop_item;
	priv->image_id = image_id;
	priv->conf = conf;

	load_image (tile);

	return GTK_WIDGET (tile);
}

static void
system_tile_init (SystemTile * tile)
{
	SystemTilePrivate *priv = SYSTEM_TILE_GET_PRIVATE (tile);

	priv->desktop_item = NULL;
	priv->image_id = NULL;
	priv->image_is_broken = TRUE;
}

static void
system_tile_finalize (GObject * g_object)
{
	/* FIXME - more to free ? */
	(*G_OBJECT_CLASS (system_tile_parent_class)->finalize) (g_object);
}

static void
system_tile_style_set (GtkWidget * widget, GtkStyle * prev_style)
{
	load_image (SYSTEM_TILE (widget));
}

static void
load_image (SystemTile * tile)
{
	SystemTilePrivate *priv = SYSTEM_TILE_GET_PRIVATE (tile);

	priv->image_is_broken = load_image_by_id (
		GTK_IMAGE (NAMEPLATE_TILE (tile)->image), GTK_ICON_SIZE_MENU, priv->image_id);
}

static GtkWidget *
create_header (const gchar * name)
{
	GtkWidget *header;

	header = gtk_label_new (name);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	return header;
}

static void
system_tile_activated (Tile * tile, TileEvent * event)
{
	tile_trigger_action_with_time (tile, tile->default_action, event->time);
}

static void
system_tile_open (Tile * tile, TileEvent * event, TileAction * action)
{
	SystemTilePrivate *priv = SYSTEM_TILE_GET_PRIVATE (tile);

	switch (priv->type)
	{
	case SYSTEM_TILE_TYPE_HELP:
	case SYSTEM_TILE_TYPE_CONTROL_CENTER:
	case SYSTEM_TILE_TYPE_PACKAGE_MANAGER:
		open_desktop_item_exec (SYSTEM_TILE_GET_PRIVATE (tile)->desktop_item);
		break;

	case SYSTEM_TILE_TYPE_LOG_OUT:
		system_tile_logout (tile, event, action);
		break;

	case SYSTEM_TILE_TYPE_LOCK_SCREEN:
		system_tile_lock_screen (tile, event, action);
		break;

	default:
		break;
	}
}

static void
system_tile_logout (Tile * tile, TileEvent * event, TileAction * action)
{
	GnomeClient *client = gnome_master_client ();

	if (!client)
		return;

	gnome_client_request_save (client, GNOME_SAVE_GLOBAL, TRUE, GNOME_INTERACT_ANY, FALSE,
		TRUE);
}

static void
system_tile_lock_screen (Tile * tile, TileEvent * event, TileAction * action)
{
	GSList *command_priority;

	gchar *exec_string;
	gchar *cmd_string;
	gchar *arg_string;

	GSList *node;
	gint i;

	command_priority = get_slab_gconf_slist (SLAB_LOCK_SCREEN_PRIORITY_KEY);

	for (node = command_priority; node; node = node->next)
	{
		exec_string = (gchar *) node->data;

		cmd_string = g_strdup (exec_string);
		arg_string = NULL;

		for (i = 0; i < strlen (exec_string); ++i)
		{
			if (g_ascii_isspace (exec_string[i]))
			{
				cmd_string = g_strndup (exec_string, i);
				arg_string = g_strdup (&exec_string[i + 1]);
			}
		}

		cmd_string = g_find_program_in_path (cmd_string);

		if (cmd_string)
		{
			exec_string = g_strdup_printf ("%s %s", cmd_string, arg_string);

			g_spawn_async (NULL, g_strsplit (exec_string, " ", 0), NULL, 0, NULL, NULL,
				NULL, NULL);

			g_free (cmd_string);
			g_free (arg_string);
			g_free (exec_string);

			return;
		}
	}

	g_warning ("could not find a command to lock screen\n");
}
