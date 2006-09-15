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

#ifndef __TILE_TABLE_H__
#define __TILE_TABLE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define TILE_TABLE_TYPE         (tile_table_get_type ())
#define TILE_TABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_TABLE_TYPE, TileTable))
#define TILE_TABLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TILE_TABLE_TYPE, TileTableClass))
#define IS_TILE_TABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_TABLE_TYPE))
#define IS_TILE_TABLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TILE_TABLE_TYPE))
#define TILE_TABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TILE_TABLE_TYPE, TileTableClass))

typedef struct
{
	GtkTable parent;
} TileTable;

typedef struct
{
	guint32 time;

	GList *tiles_prev;
	GList *tiles_curr;
} TileTableUpdateEvent;

typedef struct
{
	guint32 time;

	gchar *uri;
} TileTableURIAddedEvent;

typedef struct
{
	GtkTableClass parent_class;

	void (*tile_table_update) (TileTable *, TileTableUpdateEvent *);
	void (*tile_table_uri_added) (TileTable *, TileTableURIAddedEvent *);
} TileTableClass;

typedef enum
{
	TILE_TABLE_REORDERING_SWAP,
	TILE_TABLE_REORDERING_PUSH,
	TILE_TABLE_REORDERING_PUSH_PULL
} TileTableReorderingPriority;

GType tile_table_get_type (void);

GtkWidget *tile_table_new (guint n_cols, gboolean reorderable,
	TileTableReorderingPriority priority);

gint tile_table_load_tiles (TileTable * table, GList * tiles);
void tile_table_set_limit (TileTable * table, gint limit);

G_END_DECLS
#endif
