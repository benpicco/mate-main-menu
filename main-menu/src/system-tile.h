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

#ifndef __SYSTEM_TILE_H__
#define __SYSTEM_TILE_H__

#include "nameplate-tile.h"

#include <libgnome/gnome-desktop-item.h>

#include "main-menu-conf.h"

G_BEGIN_DECLS

#define SYSTEM_TILE_TYPE         (system_tile_get_type ())
#define SYSTEM_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SYSTEM_TILE_TYPE, SystemTile))
#define SYSTEM_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), SYSTEM_TILE_TYPE, SystemTileClass))
#define IS_SYSTEM_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SYSTEM_TILE_TYPE))
#define IS_SYSTEM_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), SYSTEM_TILE_TYPE))
#define SYSTEM_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SYSTEM_TILE_TYPE, SystemTileClass))

typedef struct
{
	NameplateTile nameplate_tile;
} SystemTile;

typedef struct
{
	NameplateTileClass nameplate_tile_class;
} SystemTileClass;

#define SYSTEM_TILE_ACTION_OPEN 0

typedef enum
{
	SYSTEM_TILE_TYPE_HELP = 0,
	SYSTEM_TILE_TYPE_CONTROL_CENTER = 1,
	SYSTEM_TILE_TYPE_PACKAGE_MANAGER = 2,
	SYSTEM_TILE_TYPE_LOCK_SCREEN = 3,
	SYSTEM_TILE_TYPE_LOG_OUT = 4,
	SYSTEM_TILE_TYPE_SENTINEL = 5
} SystemTileType;

GType system_tile_get_type (void);

GtkWidget *system_tile_new_with_type (SystemTileType type, MainMenuConf * conf);

G_END_DECLS
#endif
