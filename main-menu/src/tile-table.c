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

#include "tile-table.h"

#include "tile.h"

G_DEFINE_TYPE (TileTable, tile_table, GTK_TYPE_TABLE)

static void tile_table_finalize (GObject *);

typedef struct
{
	GList *tiles;
	
	GtkBin **bins;
	gint n_bins;
	
	gint limit;
	gboolean reorderable;
	TileTableReorderingPriority priority;
} TileTablePrivate;

#define TILE_TABLE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_TABLE_TYPE, TileTablePrivate))

enum
{
	TILE_TABLE_UPDATE_SIGNAL,
	TILE_TABLE_URI_ADDED_SIGNAL,
	LAST_SIGNAL
};

static guint tile_table_signals[LAST_SIGNAL] = { 0 };

static void tile_table_update (TileTable *, TileTableUpdateEvent *);

static void update_bins (TileTable *);
static void insert_into_bin (TileTable *, Tile *, gint);
static void empty_bin (TileTable *, gint);
static void resize_table (TileTable *, guint, guint);

static void tile_implicit_enable_cb (Tile *, TileEvent *, gpointer);
static void tile_implicit_disable_cb (Tile *, TileEvent *, gpointer);

static void tile_drag_data_rcv_cb (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *,
	guint, guint, gpointer);

#define FILE_AREA_TABLE_ROW_SPACINGS 6
#define FILE_AREA_TABLE_COL_SPACINGS 6

static void
tile_table_class_init (TileTableClass * this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	g_obj_class->finalize = tile_table_finalize;

	this_class->tile_table_update = tile_table_update;
	this_class->tile_table_uri_added = NULL;

	g_type_class_add_private (this_class, sizeof (TileTablePrivate));

	tile_table_signals[TILE_TABLE_UPDATE_SIGNAL] = g_signal_new ("tile-table-update",
		G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (TileTableClass, tile_table_update),
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

	tile_table_signals[TILE_TABLE_URI_ADDED_SIGNAL] = g_signal_new ("tile-table-uri-added",
		G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (TileTableClass, tile_table_uri_added),
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
}

GtkWidget *
tile_table_new (guint n_cols, gboolean reorderable, TileTableReorderingPriority priority)
{
	TileTable *table = g_object_new (TILE_TABLE_TYPE,
		"n-columns", n_cols,
		"homogeneous", TRUE,
		"row-spacing", FILE_AREA_TABLE_ROW_SPACINGS,
		"column-spacing", FILE_AREA_TABLE_COL_SPACINGS,
		NULL);

	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (table);

	priv->reorderable = reorderable;
	priv->priority = priority;

	return GTK_WIDGET (table);
}

static void
tile_table_init (TileTable * this)
{
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);

	priv->tiles = NULL;

	priv->limit = G_MAXINT;
	priv->reorderable = TRUE;
	priv->priority = TILE_TABLE_REORDERING_PUSH_PULL;
}

static void
tile_table_finalize (GObject * g_object)
{
	(*G_OBJECT_CLASS (tile_table_parent_class)->finalize) (g_object);
}

gint
tile_table_load_tiles (TileTable * this, GList * tiles)
{
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);
	GList *node;
	gboolean connected;
	guint n_rows;
	guint n_cols;

	for (node = priv->tiles; node; node = node->next)
		gtk_widget_destroy (GTK_WIDGET (node->data));

	g_list_free (priv->tiles);

	priv->tiles = tiles;

	for (node = priv->tiles; node; node = node->next)
	{
		g_assert (IS_TILE (node->data));

		connected =
			GPOINTER_TO_INT (g_object_get_data (G_OBJECT (node->data),
				"tile-table-connected"));

		if (!connected)
		{
			if (priv->reorderable)
			{
				gtk_drag_dest_set (GTK_WIDGET (node->data), GTK_DEST_DEFAULT_ALL,
					NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);

				gtk_drag_dest_add_uri_targets (GTK_WIDGET (node->data));

				g_signal_connect (G_OBJECT (node->data), "drag-data-received",
					G_CALLBACK (tile_drag_data_rcv_cb), this);
			}

			g_signal_connect (G_OBJECT (node->data), "tile-implicit-enable",
				G_CALLBACK (tile_implicit_enable_cb), this);

			g_signal_connect (G_OBJECT (node->data), "tile-implicit-disable",
				G_CALLBACK (tile_implicit_disable_cb), this);

			g_object_set_data (G_OBJECT (node->data), "tile-table-connected",
				GINT_TO_POINTER (TRUE));
		}
	}

	update_bins (this);

	g_object_get (G_OBJECT (this), "n-rows", &n_rows, "n-columns", &n_cols, NULL);

	return n_rows * n_cols;
}

void
tile_table_set_limit (TileTable * this, gint limit)
{
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);

	if (limit < 0)
		limit = G_MAXINT;

	if (limit != priv->limit)
	{
		priv->limit = limit;

		update_bins (this);
	}
}

static void
tile_table_update (TileTable * this, TileTableUpdateEvent * event)
{
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);

	g_list_free (priv->tiles);
	priv->tiles = event->tiles_curr;

	update_bins (this);
}

static void
update_bins (TileTable * this)
{
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);

	GList *node;
	gint index;

	gint n_valid;
	gint n_rows;
	gint n_cols;

	for (node = priv->tiles, n_valid = 0; node; node = node->next)
	{
		if (TILE (node->data)->enabled)
			++n_valid;

		if (n_valid >= priv->limit)
			tile_explicit_disable (TILE (node->data));
	}

	if (n_valid > priv->limit)
		n_valid = priv->limit;

	g_object_get (G_OBJECT (this), "n-columns", &n_cols, NULL);

	n_rows = (n_valid + n_cols - 1) / n_cols;

	resize_table (this, n_rows, n_cols);

	for (node = priv->tiles, index = 0; node && index < priv->n_bins; node = node->next)
		if (TILE (node->data)->enabled)
			insert_into_bin (this, TILE (node->data), index++);

	for (; index < priv->n_bins; ++index)
		empty_bin (this, index);

	gtk_widget_show_all (GTK_WIDGET (this));
}

static void
insert_into_bin (TileTable * this, Tile * tile, gint index)
{
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);

	guint n_cols;

	GtkWidget *parent;
	GtkWidget *child;

	if (!GTK_IS_BIN (priv->bins[index]))
	{
		priv->bins[index] = GTK_BIN (gtk_alignment_new (0.0, 0.5, 1.0, 1.0));

		g_object_get (G_OBJECT (this), "n-columns", &n_cols, NULL);

		gtk_table_attach (GTK_TABLE (this), GTK_WIDGET (priv->bins[index]), index % n_cols,
			index % n_cols + 1, index / n_cols, index / n_cols + 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	}
	else
	{
		child = gtk_bin_get_child (priv->bins[index]);

		if (!tile_compare (child, tile))
			return;

		if (child)
		{
			g_object_ref (G_OBJECT (child));
			gtk_container_remove (GTK_CONTAINER (priv->bins[index]), child);
		}
	}

	if ((parent = gtk_widget_get_parent (GTK_WIDGET (tile))))
	{
		g_object_ref (G_OBJECT (tile));
		gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (tile));
		gtk_container_add (GTK_CONTAINER (priv->bins[index]), GTK_WIDGET (tile));
		g_object_unref (G_OBJECT (tile));
	}
	else
		gtk_container_add (GTK_CONTAINER (priv->bins[index]), GTK_WIDGET (tile));
}

static void
empty_bin (TileTable * this, gint index)
{
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);
	GtkWidget *child;

	if (priv->bins[index])
	{
		if ((child = gtk_bin_get_child (priv->bins[index])))
		{
			g_object_ref (G_OBJECT (child));
			gtk_container_remove (GTK_CONTAINER (priv->bins[index]), child);
		}
	}
}

static void
resize_table (TileTable * this, guint n_rows_new, guint n_cols_new)
{
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);

	GtkBin **bins_new;
	gint n_bins_new;

	GtkWidget *child;

	gint i;

	n_bins_new = n_rows_new * n_cols_new;

	if (n_bins_new == priv->n_bins)
		return;

	bins_new = g_new0 (GtkBin *, n_bins_new);

	if (priv->bins)
	{
		for (i = 0; i < priv->n_bins; ++i)
		{
			if (i < n_bins_new)
				bins_new[i] = priv->bins[i];
			else
			{
				if (priv->bins[i])
				{
					if ((child = gtk_bin_get_child (priv->bins[i])))
					{
						g_object_ref (G_OBJECT (child));
						gtk_container_remove (GTK_CONTAINER (priv->bins[i]),
							child);
					}

					gtk_widget_destroy (GTK_WIDGET (priv->bins[i]));
				}
			}
		}

		g_free (priv->bins);
	}

	priv->bins = bins_new;
	priv->n_bins = n_bins_new;

	gtk_table_resize (GTK_TABLE (this), n_rows_new, n_cols_new);
}

static void
tile_implicit_enable_cb (Tile * tile, TileEvent * event, gpointer user_data)
{
	update_bins (TILE_TABLE (user_data));
}

static void
tile_implicit_disable_cb (Tile * tile, TileEvent * event, gpointer user_data)
{
	update_bins (TILE_TABLE (user_data));
}

static void
tile_drag_data_rcv_cb (GtkWidget * dst_widget, GdkDragContext * drag_context, gint x, gint y,
	GtkSelectionData * selection, guint info, guint time, gpointer user_data)
{
	TileTable *this = TILE_TABLE (user_data);
	TileTablePrivate *priv = TILE_TABLE_GET_PRIVATE (this);

	GList *tiles_prev;
	GList *tiles_curr;

	GtkWidget *src_widget;
	gchar **uris;

	GList *src_node;
	GList *dst_node;
	GList *last_node;

	gint src_index;
	gint dst_index;

	TileTableUpdateEvent *update_event;
	TileTableURIAddedEvent *uri_event;

	gpointer tmp;
	gint i;

	src_widget = gtk_drag_get_source_widget (drag_context);

	if (!src_widget || !IS_TILE (src_widget) || !tile_compare (src_widget, dst_widget))
	{
		uris = gtk_selection_data_get_uris (selection);

		for (i = 0; uris && uris[i]; ++i)
		{
			uri_event = g_new0 (TileTableURIAddedEvent, 1);
			uri_event->time = (guint32) time;
			uri_event->uri = g_strdup (uris[i]);

			g_signal_emit (this, tile_table_signals[TILE_TABLE_URI_ADDED_SIGNAL], 0,
				uri_event);
		}

		goto exit;
	}

	tiles_prev = g_list_copy (priv->tiles);
	tiles_curr = g_list_copy (priv->tiles);

	src_node = g_list_find_custom (tiles_curr, src_widget, tile_compare);
	dst_node = g_list_find_custom (tiles_curr, dst_widget, tile_compare);

	if (priv->priority == TILE_TABLE_REORDERING_SWAP)
	{
		tmp = src_node->data;
		src_node->data = dst_node->data;
		dst_node->data = tmp;
	}
	else
	{
		src_index = g_list_position (tiles_curr, src_node);
		dst_index = g_list_position (tiles_curr, dst_node);

		tiles_curr = g_list_remove_link (tiles_curr, src_node);

		if (priv->priority == TILE_TABLE_REORDERING_PUSH)
		{
			if (src_index < dst_index)
			{
				last_node = g_list_last (tiles_curr);

				tiles_curr = g_list_remove_link (tiles_curr, last_node);
				tiles_curr = g_list_prepend (tiles_curr, last_node->data);
			}
		}
		else if (src_index < dst_index)
			dst_node = dst_node->next;

		tiles_curr = g_list_insert_before (tiles_curr, dst_node, src_node->data);
	}

	update_event = g_new0 (TileTableUpdateEvent, 1);
	update_event->time = (guint32) time;
	update_event->tiles_prev = tiles_prev;
	update_event->tiles_curr = tiles_curr;

	g_signal_emit (this, tile_table_signals[TILE_TABLE_UPDATE_SIGNAL], 0, update_event);

      exit:

	gtk_drag_finish (drag_context, TRUE, FALSE, (guint32) time);
}
