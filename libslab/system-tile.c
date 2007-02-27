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

#include "system-tile.h"

#include <string.h>
#include <glib/gi18n.h>
#include <glib/gmacros.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-client.h>

#include "slab-gnome-util.h"
#include "libslab-utils.h"

G_DEFINE_TYPE (SystemTile, system_tile, NAMEPLATE_TILE_TYPE)

static void system_tile_finalize (GObject *);
static void system_tile_style_set (GtkWidget *, GtkStyle *);

static void load_image (SystemTile *);
static GtkWidget *create_header (const gchar *);

static void open_trigger   (Tile *, TileEvent *, TileAction *);
static void remove_trigger (Tile *, TileEvent *, TileAction *);

typedef struct {
	GnomeDesktopItem *desktop_item;
	
	gchar    *image_id;
	gboolean  image_is_broken;
} SystemTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SYSTEM_TILE_TYPE, SystemTilePrivate))

GtkWidget *
system_tile_new (const gchar *desktop_item_id, const gchar *title)
{
	SystemTile        *this;
	SystemTilePrivate *priv;

	gchar     *uri    = NULL;
	GtkWidget *header = NULL;

	GtkMenu *context_menu;

	TileAction  **actions;
	TileAction   *action;
	GtkWidget    *menu_item;
	GtkContainer *menu_ctnr;

	GnomeDesktopItem *desktop_item = NULL;
	gchar            *image_id     = NULL;
	gchar            *header_txt   = NULL;

	gchar *markup;

	AtkObject *accessible = NULL;


	desktop_item = libslab_gnome_desktop_item_new_from_unknown_id (desktop_item_id);

	if (desktop_item) {
		image_id = g_strdup (gnome_desktop_item_get_localestring (desktop_item, "Icon"));
		uri      = g_strdup (gnome_desktop_item_get_location (desktop_item));

		if (title)
			header_txt = g_strdup (title);
		else
			header_txt = g_strdup (
				gnome_desktop_item_get_localestring (desktop_item, "Name"));
	}

	if (! uri)
		return NULL;

	header = create_header (header_txt);

	context_menu = GTK_MENU (gtk_menu_new ());

	this = g_object_new (
		SYSTEM_TILE_TYPE,
		"tile-uri",            uri,
		"context-menu",        context_menu,
		"nameplate-image",     gtk_image_new (),
		"nameplate-header",    header,
		"nameplate-subheader", NULL,
		NULL);

	actions = g_new0 (TileAction *, 2);

	TILE (this)->actions   = actions;
	TILE (this)->n_actions = 2;

	menu_ctnr = GTK_CONTAINER (TILE (this)->context_menu);

	markup = g_markup_printf_escaped (_("<b>Open %s</b>"), header_txt);
	action = tile_action_new (TILE (this), open_trigger, markup, TILE_ACTION_OPENS_NEW_WINDOW);
	actions [SYSTEM_TILE_ACTION_OPEN] = action;
	g_free (markup);

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));

	gtk_container_add (menu_ctnr, menu_item);

	TILE (this)->default_action = action;

	gtk_container_add (menu_ctnr, gtk_separator_menu_item_new ());

	markup = g_markup_printf_escaped (_("Remove from System Items"));
	action = tile_action_new (TILE (this), remove_trigger, markup, 0);
	actions [SYSTEM_TILE_ACTION_REMOVE] = action;
	g_free (markup);

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));

	gtk_container_add (menu_ctnr, menu_item);

	gtk_widget_show_all (GTK_WIDGET (TILE (this)->context_menu));

	priv = PRIVATE (this);
	priv->desktop_item = desktop_item;
	priv->image_id = g_strdup (image_id);

	load_image (this);

	/* Set up the mnemonic for the tile */
	gtk_label_set_mnemonic_widget (GTK_LABEL (header), GTK_WIDGET (this));

	/* Set up the accessible name for the tile */
	accessible = gtk_widget_get_accessible (GTK_WIDGET (this));
	if (header_txt)
		atk_object_set_name (accessible, header_txt);

	g_free (header_txt);
	g_free (image_id);
	g_free (uri);

	return GTK_WIDGET (this);
}

static void
system_tile_class_init (SystemTileClass *this_class)
{
	GObjectClass   *g_obj_class  = G_OBJECT_CLASS   (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);

	g_obj_class->finalize = system_tile_finalize;

	widget_class->style_set = system_tile_style_set;

	g_type_class_add_private (this_class, sizeof (SystemTilePrivate));
}

static void
system_tile_init (SystemTile *this)
{
	SystemTilePrivate *priv = PRIVATE (this);

	priv->desktop_item    = NULL;
	priv->image_id        = NULL;
	priv->image_is_broken = TRUE;
}

static void
system_tile_finalize (GObject *g_obj)
{
        SystemTilePrivate *priv = PRIVATE (g_obj);

	g_free (priv->image_id);
	gnome_desktop_item_unref (priv->desktop_item);

	G_OBJECT_CLASS (system_tile_parent_class)->finalize (g_obj);
}

static void
system_tile_style_set (GtkWidget *widget, GtkStyle *prev_style)
{
	load_image (SYSTEM_TILE (widget));
}

static void
load_image (SystemTile *this)
{
	SystemTilePrivate *priv = PRIVATE (this);

	GtkImage *image = GTK_IMAGE (NAMEPLATE_TILE (this)->image);


	g_object_set (G_OBJECT (image), "icon-size", GTK_ICON_SIZE_MENU, NULL);

	priv->image_is_broken = libslab_gtk_image_set_by_id (image, priv->image_id);
}

static GtkWidget *
create_header (const gchar *name)
{
	GtkWidget *header;

	header = gtk_label_new (name);
	gtk_label_set_use_underline (GTK_LABEL (header), TRUE);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	return header;
}

static void
open_trigger (Tile *this, TileEvent *event, TileAction *action)
{
	open_desktop_item_exec (PRIVATE (this)->desktop_item);
}

static void
remove_trigger (Tile *this, TileEvent *event, TileAction *action)
{
	libslab_remove_system_item (this->uri);
}
