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

#ifndef __BOOKMARK_TILE_TABLE_H__
#define __BOOKMARK_TILE_TABLE_H__

#include <gtk/gtk.h>

#include "tile-table.h"
#include "libslab-utils.h"

G_BEGIN_DECLS

#define BOOKMARK_TILE_TABLE_TYPE         (bookmark_tile_table_get_type ())
#define BOOKMARK_TILE_TABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BOOKMARK_TILE_TABLE_TYPE, BookmarkTileTable))
#define BOOKMARK_TILE_TABLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), BOOKMARK_TILE_TABLE_TYPE, BookmarkTileTableClass))
#define IS_BOOKMARK_TILE_TABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BOOKMARK_TILE_TABLE_TYPE))
#define IS_BOOKMARK_TILE_TABLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), BOOKMARK_TILE_TABLE_TYPE))
#define BOOKMARK_TILE_TABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BOOKMARK_TILE_TABLE_TYPE, BookmarkTileTableClass))

typedef struct {
	TileTable tile_table;
} BookmarkTileTable;

typedef struct {
	TileTableClass tile_table_class;

	gchar *     (* get_store_path)        (gboolean);
	void        (* update_bookmark_store) (LibSlabBookmarkFile *, LibSlabBookmarkFile *, const gchar *);
	GtkWidget * (* get_tile)              (LibSlabBookmarkFile *, const gchar *);
} BookmarkTileTableClass;

GType bookmark_tile_table_get_type (void);

G_END_DECLS

#endif
