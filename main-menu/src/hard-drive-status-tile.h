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

#ifndef __HARD_DRIVE_STATUS_TILE_H__
#define __HARD_DRIVE_STATUS_TILE_H__

#include "nameplate-tile.h"

G_BEGIN_DECLS

#define HARD_DRIVE_STATUS_TILE_TYPE         (hard_drive_status_tile_get_type ())
#define HARD_DRIVE_STATUS_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), HARD_DRIVE_STATUS_TILE_TYPE, HardDriveStatusTile))
#define HARD_DRIVE_STATUS_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), HARD_DRIVE_STATUS_TILE_TYPE, HardDriveStatusTileClass))
#define IS_HARD_DRIVE_STATUS_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), HARD_DRIVE_STATUS_TILE_TYPE))
#define IS_HARD_DRIVE_STATUS_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), HARD_DRIVE_STATUS_TILE_TYPE))
#define HARD_DRIVE_STATUS_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), HARD_DRIVE_STATUS_TILE_TYPE, HardDriveStatusTileClass))

typedef struct
{
	NameplateTile nameplate_tile;
} HardDriveStatusTile;

typedef struct
{
	NameplateTileClass nameplate_tile_class;
} HardDriveStatusTileClass;

#define HARD_DRIVE_STATUS_TILE_ACTION_OPEN 0

GType hard_drive_status_tile_get_type (void);

GtkWidget *hard_drive_status_tile_new (void);

G_END_DECLS
#endif
