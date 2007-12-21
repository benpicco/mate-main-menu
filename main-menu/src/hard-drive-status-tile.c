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

#include "hard-drive-status-tile.h"

#include <string.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib/gi18n.h>
#include <glibtop/fsusage.h>
#include <libhal.h>
#include <libhal-storage.h>
#include <gconf/gconf-client.h>

#include "slab-gnome-util.h"
#include "libslab-utils.h"

#define GIGA (1024 * 1024 * 1024)
#define MEGA (1024 * 1024)
#define KILO (1024)

#define TIMEOUT_KEY_DIR "/apps/procman"
#define TIMEOUT_KEY     "/apps/procman/disks_interval"

#define SYSTEM_MONITOR_GCONF_KEY "/desktop/gnome/applications/main-menu/system_monitor"

G_DEFINE_TYPE (HardDriveStatusTile, hard_drive_status_tile, NAMEPLATE_TILE_TYPE)

static void hard_drive_status_tile_finalize (GObject *);
static void hard_drive_status_tile_destroy (GtkObject *);
static void hard_drive_status_tile_style_set (GtkWidget *, GtkStyle *);

static void hard_drive_status_tile_activated (Tile *, TileEvent *);

static void open_hard_drive_tile (Tile *, TileEvent *, TileAction *);

static void update_tile (HardDriveStatusTile *);

static void tile_hide_event_cb (GtkWidget *, gpointer);
static void tile_show_event_cb (GtkWidget *, gpointer);

typedef struct
{
	LibHalContext *hal_context;
	DBusConnection *dbus_connection;
	
	GConfClient *gconf;
	
	guint64 capacity_bytes;
	guint64 available_bytes;
	
	guint timeout_notify;
	
	guint update_timeout;
} HardDriveStatusTilePrivate;

#define HARD_DRIVE_STATUS_TILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), HARD_DRIVE_STATUS_TILE_TYPE, HardDriveStatusTilePrivate))

static void hard_drive_status_tile_class_init (HardDriveStatusTileClass * hd_tile_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (hd_tile_class);
	GtkObjectClass *gtk_obj_class = GTK_OBJECT_CLASS (hd_tile_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (hd_tile_class);
	TileClass *tile_class = TILE_CLASS (hd_tile_class);

	g_obj_class->finalize = hard_drive_status_tile_finalize;

	gtk_obj_class->destroy = hard_drive_status_tile_destroy;

	widget_class->style_set = hard_drive_status_tile_style_set;

	tile_class->tile_activated = hard_drive_status_tile_activated;

	g_type_class_add_private (hd_tile_class, sizeof (HardDriveStatusTilePrivate));
}

GtkWidget *
hard_drive_status_tile_new ()
{
	GtkWidget *tile;

	GtkWidget *image;
	GtkWidget *header;
	GtkWidget *subheader;

	AtkObject *accessible;
	char *name;

	image = gtk_image_new ();
	slab_load_image (GTK_IMAGE (image), GTK_ICON_SIZE_BUTTON, "gnome-dev-harddisk");

	name = g_strdup (_("Hard _Drive"));

	header = gtk_label_new (name);
	gtk_label_set_use_underline (GTK_LABEL (header), TRUE);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	subheader = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (subheader), 0.0, 0.5);
	gtk_widget_modify_fg (subheader, GTK_STATE_NORMAL,
		&subheader->style->fg[GTK_STATE_INSENSITIVE]);

	tile = g_object_new (HARD_DRIVE_STATUS_TILE_TYPE, "tile-uri", "tile://hard-drive-status",
		"nameplate-image", image, "nameplate-header", header, "nameplate-subheader",
		subheader, NULL);

	TILE (tile)->actions = g_new0 (TileAction *, 1);
	TILE (tile)->n_actions = 1;

	TILE (tile)->actions[HARD_DRIVE_STATUS_TILE_ACTION_OPEN] =
		tile_action_new (TILE (tile), open_hard_drive_tile, NULL,
		TILE_ACTION_OPENS_NEW_WINDOW);

	TILE (tile)->default_action = TILE (tile)->actions[HARD_DRIVE_STATUS_TILE_ACTION_OPEN];

	g_signal_connect (G_OBJECT (tile), "hide", G_CALLBACK (tile_hide_event_cb), NULL);
	g_signal_connect (G_OBJECT (tile), "show", G_CALLBACK (tile_show_event_cb), NULL);

	accessible = gtk_widget_get_accessible (tile);
	atk_object_set_name (accessible, name);

	gtk_label_set_mnemonic_widget (GTK_LABEL (header), GTK_WIDGET (tile));

	g_free (name);

	return GTK_WIDGET (tile);
}

static void
hard_drive_status_tile_init (HardDriveStatusTile * tile)
{
	HardDriveStatusTilePrivate *priv = HARD_DRIVE_STATUS_TILE_GET_PRIVATE (tile);
	DBusError error;

	priv->hal_context = libhal_ctx_new ();

	if (!priv->hal_context)
		return;

	dbus_error_init (&error);
	priv->dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

	dbus_connection_set_exit_on_disconnect (priv->dbus_connection, FALSE);

	if (dbus_error_is_set (&error))
	{
		g_warning ("error (%s): [%s]\n", error.name, error.message);

		dbus_error_free (&error);

		priv->hal_context = NULL;
		priv->dbus_connection = NULL;

		return;
	}

	dbus_error_free (&error);

	dbus_connection_setup_with_g_main (priv->dbus_connection, g_main_context_default ());

	libhal_ctx_set_dbus_connection (priv->hal_context, priv->dbus_connection);

	dbus_error_init (&error);
	libhal_ctx_init (priv->hal_context, &error);

	if (dbus_error_is_set (&error))
	{
		g_warning ("error (%s): [%s]\n", error.name, error.message);

		dbus_error_free (&error);

		priv->hal_context = NULL;

		return;
	}

	dbus_error_free (&error);

	priv->gconf = gconf_client_get_default ();
	gconf_client_add_dir (priv->gconf, TIMEOUT_KEY_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
}

static void
hard_drive_status_tile_finalize (GObject * g_object)
{
/*	if (data->hal_context)
		libhal_ctx_free (data->hal_context); */
	/* FIXME */
	(*G_OBJECT_CLASS (hard_drive_status_tile_parent_class)->finalize) (g_object);
}

static void
hard_drive_status_tile_destroy (GtkObject * gtk_object)
{
	HardDriveStatusTile *tile = HARD_DRIVE_STATUS_TILE (gtk_object);
	HardDriveStatusTilePrivate *priv = HARD_DRIVE_STATUS_TILE_GET_PRIVATE (tile);

	if (!priv)
		return;

	if (priv->timeout_notify)
	{
		gconf_client_notify_remove (priv->gconf, priv->timeout_notify);
		priv->timeout_notify = 0;
	}

	if (priv->update_timeout)
	{
		g_source_remove (priv->update_timeout);
		priv->update_timeout = 0;
	}
}

static void
hard_drive_status_tile_style_set (GtkWidget * widget, GtkStyle * prev_style)
{
	update_tile (HARD_DRIVE_STATUS_TILE (widget));
}

static void
hard_drive_status_tile_activated (Tile * tile, TileEvent * event)
{
	if (event->type != TILE_EVENT_ACTIVATED_DOUBLE_CLICK)
		tile_trigger_action_with_time (tile, tile->default_action, event->time);
}

static GList *
get_pertinent_devices (LibHalContext * hal_context)
{
	GList *devices;

	gchar **udis;
	gint n_udis;

	LibHalVolume *vol;
	gchar *mount_point;

	DBusError error;

	gint i;

	if (!hal_context)
		return NULL;

	dbus_error_init (&error);
	udis = libhal_find_device_by_capability (hal_context, "volume", &n_udis, &error);

	if (dbus_error_is_set (&error))
	{
		g_warning ("error (%s): [%s]\n", error.name, error.message);

		dbus_error_free (&error);

		return NULL;
	}

	dbus_error_free (&error);

	devices = NULL;

	for (i = 0; i < n_udis; ++i)
	{
		vol = libhal_volume_from_udi (hal_context, udis[i]);

		if (libhal_volume_is_mounted (vol) && !libhal_volume_is_disc (vol))
		{
			mount_point = g_strdup (libhal_volume_get_mount_point (vol));
			devices = g_list_append (devices, mount_point);
		}

		libhal_volume_free (vol);
	}

	libhal_free_string_array (udis);

	return devices;
}

static void
compute_usage (HardDriveStatusTile * tile)
{
	HardDriveStatusTilePrivate *priv = HARD_DRIVE_STATUS_TILE_GET_PRIVATE (tile);

	GList *devices;
	glibtop_fsusage fs_usage;

	GList *node;

	if (!priv->hal_context)
		return;

	devices = get_pertinent_devices (priv->hal_context);

	priv->capacity_bytes = 0;
	priv->available_bytes = 0;

	for (node = devices; node; node = node->next)
	{
		if (node->data)
		{
			glibtop_get_fsusage (&fs_usage, (gchar *) node->data);

			priv->available_bytes += fs_usage.bfree * fs_usage.block_size;
			priv->capacity_bytes += fs_usage.blocks * fs_usage.block_size;

			g_free (node->data);
		}
	}

	g_list_free (devices);
}

static gchar *
size_bytes_to_string (guint64 size)
{
	gchar *size_string;
	unsigned long long size_bytes;

/* FIXME:  this is just a work around for gcc warnings about %lu not big enough
 * to hold guint64 on 32bit machines.  on 64bit machines, however, gcc warns
 * that %llu is too big for guint64.  ho-hum.
 */

	size_bytes = (unsigned long long) size;

	if (size_bytes > GIGA)
		size_string = g_strdup_printf (_("%lluG"), size_bytes / GIGA);
	else if (size_bytes > MEGA)
		size_string = g_strdup_printf (_("%lluM"), size_bytes / MEGA);
	else if (size_bytes > KILO)
		size_string = g_strdup_printf (_("%lluK"), size_bytes / KILO);
	else
		size_string = g_strdup_printf (_("%llub"), size_bytes);

	return size_string;
}

static void
update_tile (HardDriveStatusTile * tile)
{
	HardDriveStatusTilePrivate *priv = HARD_DRIVE_STATUS_TILE_GET_PRIVATE (tile);
	AtkObject *accessible;

	gchar *markup = NULL;

	gchar *available;
	gchar *capacity;

	compute_usage (tile);

	available = size_bytes_to_string (priv->available_bytes);
	capacity = size_bytes_to_string (priv->capacity_bytes);

	markup = g_strdup_printf (_("%s Free / %s Total"), available, capacity);

	gtk_label_set_text (GTK_LABEL (NAMEPLATE_TILE (tile)->subheader), markup);

	accessible = gtk_widget_get_accessible (GTK_WIDGET (tile));
	if (markup)
	  atk_object_set_description (accessible, markup);

	g_free (available);
	g_free (capacity);
	g_free (markup);
}

static gboolean
timeout_cb (gpointer user_data)
{
	HardDriveStatusTile *tile = HARD_DRIVE_STATUS_TILE (user_data);

	update_tile (tile);

	return TRUE;
}

static void
timeout_changed_cb (GConfClient * client, guint id, GConfEntry * entry, gpointer user_data)
{
	HardDriveStatusTile *tile = HARD_DRIVE_STATUS_TILE (user_data);
	HardDriveStatusTilePrivate *priv = HARD_DRIVE_STATUS_TILE_GET_PRIVATE (tile);

	int timeout;

	if (priv->update_timeout)
		g_source_remove (priv->update_timeout);

	timeout = gconf_value_get_int (gconf_entry_get_value (entry));
	timeout = MAX (timeout, 1000);

	priv->update_timeout = g_timeout_add (timeout, timeout_cb, tile);
}

static void
tile_hide_event_cb (GtkWidget * widget, gpointer user_data)
{
	HardDriveStatusTile *tile = HARD_DRIVE_STATUS_TILE (widget);
	HardDriveStatusTilePrivate *priv = HARD_DRIVE_STATUS_TILE_GET_PRIVATE (tile);

	if (priv->timeout_notify)
	{
		gconf_client_notify_remove (priv->gconf, priv->timeout_notify);
		priv->timeout_notify = 0;
	}

	if (priv->update_timeout)
	{
		g_source_remove (priv->update_timeout);
		priv->update_timeout = 0;
	}

	update_tile (tile);
}

static void
tile_show_event_cb (GtkWidget * widget, gpointer user_data)
{
	HardDriveStatusTile *tile = HARD_DRIVE_STATUS_TILE (widget);
	HardDriveStatusTilePrivate *priv = HARD_DRIVE_STATUS_TILE_GET_PRIVATE (tile);

	update_tile (tile);
	if (!priv->update_timeout)
	{
		int timeout;

		timeout = gconf_client_get_int (priv->gconf, TIMEOUT_KEY, NULL);
		timeout = MAX (timeout, 1000);

		priv->timeout_notify =
			gconf_client_notify_add (priv->gconf, TIMEOUT_KEY, timeout_changed_cb, tile,
			NULL, NULL);

		priv->update_timeout = g_timeout_add (timeout, timeout_cb, tile);
	}
}

static void
open_hard_drive_tile (Tile * tile, TileEvent * event, TileAction * action)
{
	GnomeDesktopItem *ditem;
	gchar *fb_ditem_id;
	

	fb_ditem_id = (gchar *) libslab_get_gconf_value (SYSTEM_MONITOR_GCONF_KEY);

	if (! fb_ditem_id)
		fb_ditem_id = g_strdup ("nautilus.desktop");

	ditem = libslab_gnome_desktop_item_new_from_unknown_id (fb_ditem_id);

	if (! open_desktop_item_exec (ditem))
		g_warning ("open_hard_drive_tile: couldn't exec item\n");

	gnome_desktop_item_unref (ditem);
	g_free (fb_ditem_id);
}
