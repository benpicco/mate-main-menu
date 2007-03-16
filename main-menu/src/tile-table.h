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

#ifndef __TILE_TABLE_H__
#define __TILE_TABLE_H__

#include <gtk/gtk.h>

#include "tile.h"
#include "bookmark-agent.h"

G_BEGIN_DECLS

#define TILE_TABLE_TYPE         (tile_table_get_type ())
#define TILE_TABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_TABLE_TYPE, TileTable))
#define TILE_TABLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TILE_TABLE_TYPE, TileTableClass))
#define IS_TILE_TABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_TABLE_TYPE))
#define IS_TILE_TABLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TILE_TABLE_TYPE))
#define TILE_TABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TILE_TABLE_TYPE, TileTableClass))

#define TILE_TABLE_TILES_PROP   "tile-list"
#define TILE_TABLE_LIMIT_PROP   "item-limit"
#define TILE_TABLE_REORDER_PROP "reorderable"
#define TILE_TABLE_MODIFY_PROP  "modifiable"

typedef struct {
	GtkTable gtk_table;
} TileTable;

typedef struct {
	GtkTableClass gtk_table_class;
} TileTableClass;

typedef Tile         * (* ItemToTileFunc) (BookmarkItem *, gpointer);
typedef BookmarkItem * (* URIToItemFunc)  (const gchar *, gpointer);

GType tile_table_get_type (void);

GtkWidget *tile_table_new     (BookmarkAgent *agent, gint limit, gint n_cols,
                               gboolean reorderable, gboolean modifiable,
                               ItemToTileFunc itt, gpointer data_itt,
                               URIToItemFunc uti, gpointer data_uti);
void       tile_table_reload  (TileTable *this);
void       tile_table_add_uri (TileTable *this, const gchar *uri);

G_END_DECLS

#endif
