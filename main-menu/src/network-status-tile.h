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

#ifndef __NETWORK_STATUS_TILE_H__
#define __NETWORK_STATUS_TILE_H__

#include "nameplate-tile.h"

G_BEGIN_DECLS

#define NETWORK_STATUS_TILE_TYPE         (network_status_tile_get_type ())
#define NETWORK_STATUS_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NETWORK_STATUS_TILE_TYPE, NetworkStatusTile))
#define NETWORK_STATUS_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), NETWORK_STATUS_TILE_TYPE, NetworkStatusTileClass))
#define IS_NETWORK_STATUS_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NETWORK_STATUS_TILE_TYPE))
#define IS_NETWORK_STATUS_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), NETWORK_STATUS_TILE_TYPE))
#define NETWORK_STATUS_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NETWORK_STATUS_TILE_TYPE, NetworkStatusTileClass))

typedef struct
{
	NameplateTile nameplate_tile;
} NetworkStatusTile;

typedef struct
{
	NameplateTileClass nameplate_tile_class;
} NetworkStatusTileClass;

#define NETWORK_STATUS_TILE_ACTION_OPEN 0

GType network_status_tile_get_type (void);

GtkWidget *network_status_tile_new (void);

void network_tile_update_status (GtkWidget * widget);

G_END_DECLS
#endif
