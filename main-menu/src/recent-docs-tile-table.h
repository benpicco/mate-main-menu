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

#ifndef __RECENT_DOCS_TILE_TABLE_H__
#define __RECENT_DOCS_TILE_TABLE_H__

#include <gtk/gtk.h>

#include "tile-table.h"

G_BEGIN_DECLS

#define RECENT_DOCS_TILE_TABLE_TYPE         (recent_docs_tile_table_get_type ())
#define RECENT_DOCS_TILE_TABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), RECENT_DOCS_TILE_TABLE_TYPE, RecentDocsTileTable))
#define RECENT_DOCS_TILE_TABLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), RECENT_DOCS_TILE_TABLE_TYPE, RecentDocsTileTableClass))
#define IS_RECENT_DOCS_TILE_TABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), RECENT_DOCS_TILE_TABLE_TYPE))
#define IS_RECENT_DOCS_TILE_TABLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), RECENT_DOCS_TILE_TABLE_TYPE))
#define RECENT_DOCS_TILE_TABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), RECENT_DOCS_TILE_TABLE_TYPE, RecentDocsTileTableClass))

typedef struct {
	TileTable tile_table;
} RecentDocsTileTable;

typedef struct {
	TileTableClass tile_table_class;
} RecentDocsTileTableClass;

GType recent_docs_tile_table_get_type (void);

GtkWidget *recent_docs_tile_table_new (void);

G_END_DECLS

#endif
