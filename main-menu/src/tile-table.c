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

#include "tile-table.h"

#include "tile.h"
#include "nameplate-tile.h"

G_DEFINE_TYPE (TileTable, tile_table, GTK_TYPE_TABLE)

typedef struct {
	BookmarkAgent   *agent;

	GList           *tiles;

	GtkBin         **bins;
	gint             n_bins;

	gint             limit;

	gboolean         reorderable;
	gboolean         modifiable;

	gint             reord_bin_orig;
	gint             reord_bin_curr;

	ItemToTileFunc   create_tile_func;
	gpointer         tile_func_data;
	URIToItemFunc    create_item_func;
	gpointer         item_func_data;
} TileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_TABLE_TYPE, TileTablePrivate))

enum {
	PROP_0,
	PROP_TILES,
	PROP_LIMIT,
	PROP_REORDER,
	PROP_MODIFY
};

static void     get_property  (GObject *, guint, GValue *, GParamSpec *);
static void     set_property  (GObject *, guint, const GValue *, GParamSpec *);
static void     finalize      (GObject *);
static gboolean drag_motion   (GtkWidget *, GdkDragContext *, gint, gint, guint);
static void     drag_leave    (GtkWidget *, GdkDragContext *, guint);
static void     drag_data_rcv (GtkWidget *, GdkDragContext *, gint, gint,
                               GtkSelectionData *, guint, guint);

static void   update_bins                  (TileTable *, GList *);
static void   insert_into_bin              (TileTable *, Tile *, gint);
static void   empty_bin                    (TileTable *, gint);
static void   resize_table                 (TileTable *, guint, guint);
static GList *reorder_tiles                (TileTable *, gint, gint);
static void   save_reorder                 (TileTable *, GList *);
static void   connect_signal_if_not_exists (Tile *, const gchar *, GCallback, gpointer);

static void tile_activated_cb  (Tile *, TileEvent *, gpointer);
static void tile_drag_begin_cb (GtkWidget *, GdkDragContext *, gpointer);
static void tile_drag_end_cb   (GtkWidget *, GdkDragContext *, gpointer);
static void agent_notify_cb    (GObject *, GParamSpec *, gpointer);

GtkWidget *
tile_table_new (BookmarkAgent *agent, gint limit, gint n_cols,
                gboolean reorderable, gboolean modifiable,
		ItemToTileFunc itt_func, gpointer data_itt,
		URIToItemFunc uti_func, gpointer data_uti)
{
	GtkWidget        *this;
	TileTablePrivate *priv;


	this = g_object_new (TILE_TABLE_TYPE, "n-columns", n_cols, "homogeneous", TRUE, NULL);
	priv = PRIVATE (this);

	priv->agent       = agent;
	priv->limit       = limit;
	priv->reorderable = reorderable;
	priv->modifiable  = modifiable;

	priv->create_tile_func = itt_func;
	priv->tile_func_data   = data_itt;
	priv->create_item_func = uti_func;
	priv->item_func_data   = data_uti;

	tile_table_reload (TILE_TABLE (this));

	g_signal_connect (
		G_OBJECT (priv->agent), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
		G_CALLBACK (agent_notify_cb), this);

	return this;
}

void
tile_table_reload (TileTable *this)
{
	TileTablePrivate *priv = PRIVATE (this);

	BookmarkItem **items = NULL;
	GList         *tiles = NULL;
	GtkWidget     *tile;
	gint           n_tiles;

	GtkSizeGroup *icon_size_group;

	GList *node;
	gint   i;


	g_object_get (G_OBJECT (priv->agent), BOOKMARK_AGENT_ITEMS_PROP, & items, NULL);

	for (i = 0, n_tiles = 0; (priv->limit < 0 || n_tiles < priv->limit) && items && items [i]; ++i) {
		tile = GTK_WIDGET (priv->create_tile_func (items [i], priv->tile_func_data));

		if (tile) {
			tiles = g_list_append (tiles, tile);
			++n_tiles;
		}
	}

	for (node = priv->tiles; node; node = node->next)
		gtk_widget_destroy (GTK_WIDGET (node->data));

	g_list_free (priv->tiles);

	priv->tiles = NULL;

	icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	for (node = tiles; node; node = node->next) {
		tile = GTK_WIDGET (node->data);

		g_object_set_data (G_OBJECT (node->data), "tile-table", this);

		connect_signal_if_not_exists (
			TILE (tile), "tile-activated", G_CALLBACK (tile_activated_cb), NULL);
		connect_signal_if_not_exists (
			TILE (tile), "drag-begin", G_CALLBACK (tile_drag_begin_cb), this);
		connect_signal_if_not_exists (
			TILE (tile), "drag-end", G_CALLBACK (tile_drag_end_cb), this);

		priv->tiles = g_list_append (priv->tiles, tile);

		if (IS_NAMEPLATE_TILE (tile))
			gtk_size_group_add_widget (icon_size_group, NAMEPLATE_TILE (tile)->image);
	}

	g_list_free (tiles);

	update_bins (this, priv->tiles);

	g_object_notify (G_OBJECT (this), TILE_TABLE_TILES_PROP);
}

void
tile_table_add_uri (TileTable *this, const gchar *uri)
{
	TileTablePrivate *priv = PRIVATE (this);

	BookmarkItem *item;


	item = priv->create_item_func (uri, priv->item_func_data);
	bookmark_agent_add_item (priv->agent, item);
	bookmark_item_free (item);
}

static void
tile_table_class_init (TileTableClass *this_class)
{
	GObjectClass   *g_obj_class  = G_OBJECT_CLASS   (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);

	GParamSpec *tiles_pspec;
	GParamSpec *limit_pspec;
	GParamSpec *reorder_pspec;
	GParamSpec *modify_pspec;


	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;
	g_obj_class->finalize     = finalize;

	widget_class->drag_motion        = drag_motion;
	widget_class->drag_leave         = drag_leave;
	widget_class->drag_data_received = drag_data_rcv;

	tiles_pspec = g_param_spec_pointer (
		TILE_TABLE_TILES_PROP, TILE_TABLE_TILES_PROP,
		"the GList which contains the Tiles for this table",
		G_PARAM_READABLE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	limit_pspec = g_param_spec_int (
		TILE_TABLE_LIMIT_PROP, TILE_TABLE_LIMIT_PROP,
		"the maximum number of Tiles this table can hold, -1 if unlimited",
		-1, G_MAXINT, G_MAXINT,
		G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	reorder_pspec = g_param_spec_boolean (
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDER_PROP,
		"TRUE if reordering allowed for this table",
		TRUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	modify_pspec = g_param_spec_boolean (
		TILE_TABLE_MODIFY_PROP, TILE_TABLE_MODIFY_PROP,
		"TRUE if tiles are allowed to be dragged into this table",
		TRUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_TILES,   tiles_pspec);
	g_object_class_install_property (g_obj_class, PROP_LIMIT,   limit_pspec);
	g_object_class_install_property (g_obj_class, PROP_REORDER, reorder_pspec);
	g_object_class_install_property (g_obj_class, PROP_MODIFY,  modify_pspec);

	g_type_class_add_private (this_class, sizeof (TileTablePrivate));
}

static void
tile_table_init (TileTable *this)
{
	TileTablePrivate *priv = PRIVATE (this);

	priv->agent               = NULL;

	priv->tiles               = NULL;

	priv->bins                = NULL;
	priv->n_bins              = 0;

	priv->limit               = -1;

	priv->reorderable         = FALSE;
	priv->modifiable          = FALSE;

	priv->reord_bin_orig      = -1;
	priv->reord_bin_curr      = -1;

	priv->create_tile_func    = NULL;
	priv->tile_func_data      = NULL;
	priv->create_item_func    = NULL;
	priv->item_func_data      = NULL;
}

static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	TileTablePrivate *priv = PRIVATE (g_obj);

	switch (prop_id) {
		case PROP_TILES:
			g_value_set_pointer (value, priv->tiles);
			break;

		case PROP_LIMIT:
			g_value_set_int (value, priv->limit);
			break;

		case PROP_REORDER:
			g_value_set_boolean (value, priv->reorderable);
			break;

		case PROP_MODIFY:
			g_value_set_boolean (value, priv->modifiable);
			break;

		default:
			break;
	}
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	TileTable        *this = TILE_TABLE (g_obj);
	TileTablePrivate *priv = PRIVATE    (this);

	gint limit;


	switch (prop_id) {
		case PROP_LIMIT:
			limit = g_value_get_int (value);

			if (limit != priv->limit) {
				priv->limit = limit;
				update_bins (this, priv->tiles);
			}

			break;

		case PROP_REORDER:
			priv->reorderable = g_value_get_boolean (value);
			break;

		case PROP_MODIFY:
			priv->modifiable = g_value_get_boolean (value);

			if (priv->modifiable) {
				gtk_drag_dest_set (
					GTK_WIDGET (this),
					GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
					NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);
				gtk_drag_dest_add_uri_targets (GTK_WIDGET (this));
			}
			else
				gtk_drag_dest_unset (GTK_WIDGET (this));

			break;

		default:
			break;
	}
}

static void
finalize (GObject *g_obj)
{
	TileTablePrivate *priv = PRIVATE (g_obj);

	g_free (priv->bins);

	G_OBJECT_CLASS (tile_table_parent_class)->finalize (g_obj);
}

static gboolean
drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time)
{
	TileTable        *this = TILE_TABLE (widget);
	TileTablePrivate *priv = PRIVATE    (this);

	GtkWidget *src_tile;
	TileTable *src_table;

	GList *tiles_reord;

	gint n_rows, n_cols;
	gint bin_row, bin_col;

	gint bin_index;


	if (! priv->reorderable)
		return FALSE;

	src_tile = gtk_drag_get_source_widget (context);

	if (! (src_tile && IS_TILE (src_tile))) {
		gtk_drag_highlight (widget);

		return FALSE;
	}

	src_table = TILE_TABLE (g_object_get_data (G_OBJECT (src_tile), "tile-table"));

	if (src_table != TILE_TABLE (widget)) {
		gtk_drag_highlight (widget);

		return FALSE;
	}

	g_object_get (G_OBJECT (widget), "n-rows", & n_rows, "n-columns", & n_cols, NULL);

	bin_row = y * n_rows / widget->allocation.height;
	bin_col = x * n_cols / widget->allocation.width;

	bin_index = bin_row * n_cols + bin_col;

	if (priv->reord_bin_curr != bin_index) {
		priv->reord_bin_curr = bin_index;

		tiles_reord = reorder_tiles (this, priv->reord_bin_orig, priv->reord_bin_curr);

		if (tiles_reord)
			update_bins (this, tiles_reord);
		else
			update_bins (this, priv->tiles);

		g_list_free (tiles_reord);
	}

	return FALSE;
}

static void
drag_leave (GtkWidget *widget, GdkDragContext *context, guint time)
{
	TileTable        *this = TILE_TABLE (widget);
	TileTablePrivate *priv = PRIVATE    (this);


	gtk_drag_unhighlight (widget);

	update_bins (TILE_TABLE (widget), priv->tiles);
}

static void
drag_data_rcv (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
               GtkSelectionData *selection, guint info, guint time)
{
	TileTable        *this = TILE_TABLE (widget);
	TileTablePrivate *priv = PRIVATE    (this);

	GtkWidget *src_tile;
	TileTable *src_table;

	gboolean reordering;

	GList *tiles_new;

	gchar **uris;
	gint    i;


	src_tile = gtk_drag_get_source_widget (context);

	if (src_tile && IS_TILE (src_tile)) {
		src_table = TILE_TABLE (g_object_get_data (G_OBJECT (src_tile), "tile-table"));

		reordering = (src_table == TILE_TABLE (widget));
	}
	else
		reordering = FALSE;

	if (reordering) {
		if (priv->reorderable) {
			tiles_new = reorder_tiles (this, priv->reord_bin_orig, priv->reord_bin_curr);
			save_reorder (this, tiles_new);
			g_list_free (tiles_new);
		}
	}
	else {
		uris = gtk_selection_data_get_uris (selection);

		for (i = 0; uris && uris [i]; ++i)
			tile_table_add_uri (this, uris [i]);

		g_strfreev (uris);
	}

	gtk_drag_finish (context, TRUE, FALSE, (guint32) time);
}

static void
connect_signal_if_not_exists (Tile *tile, const gchar *signal, GCallback cb, gpointer user_data)
{
	gulong handler_id = g_signal_handler_find (tile, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, cb, NULL);

	if (! handler_id)
		g_signal_connect (G_OBJECT (tile), signal, cb, user_data);
}

static GList *
reorder_tiles (TileTable *this, gint src_index, gint dst_index)
{
	TileTablePrivate *priv = PRIVATE (this);

	GList *tiles_reord;
	gint   n_tiles;

	GList *src_node;
	GList *dst_node;


	n_tiles = g_list_length (priv->tiles);

	if (dst_index >= n_tiles)
		dst_index = n_tiles - 1;

	if (src_index == dst_index)
		return NULL;

	tiles_reord = g_list_copy (priv->tiles);

	src_node = g_list_nth (tiles_reord, src_index);
	dst_node = g_list_nth (tiles_reord, dst_index);

	tiles_reord = g_list_remove_link (tiles_reord, src_node);

	if (src_index < dst_index)
		dst_node = dst_node->next;

	tiles_reord = g_list_insert_before (tiles_reord, dst_node, src_node->data);

	return tiles_reord;
}

static void
update_bins (TileTable *this, GList *tiles)
{
	TileTablePrivate *priv = PRIVATE (this);

	gint n_tiles;
	gint n_rows;
	gint n_cols;

	GList *node;
	gint   i;


	if (! tiles)
		return;

	g_object_get (G_OBJECT (this), "n-columns", & n_cols, NULL);

	n_tiles = g_list_length (tiles);

	n_rows = (n_tiles + n_cols - 1) / n_cols;

	resize_table (this, n_rows, n_cols);

	for (node = tiles, i = 0; node && i < priv->n_bins; node = node->next)
		insert_into_bin (this, TILE (node->data), i++);

	for (; i < priv->n_bins; ++i)
		empty_bin (this, i);

	gtk_widget_show_all (GTK_WIDGET (this));
}

static void
insert_into_bin (TileTable *this, Tile *tile, gint index)
{
	TileTablePrivate *priv = PRIVATE (this);

	guint n_cols;

	GtkWidget *parent;
	GtkWidget *child;


	if (! GTK_IS_BIN (priv->bins [index])) {
		priv->bins [index] = GTK_BIN (gtk_alignment_new (0.0, 0.5, 1.0, 1.0));

		g_object_get (G_OBJECT (this), "n-columns", & n_cols, NULL);

		gtk_table_attach (
			GTK_TABLE (this), GTK_WIDGET (priv->bins [index]),
			index % n_cols, index % n_cols + 1,
			index / n_cols, index / n_cols + 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	}
	else {
		child = gtk_bin_get_child (priv->bins [index]);

		if (! tile_compare (child, tile))
			return;

		if (child) {
			g_object_ref (G_OBJECT (child));
			gtk_container_remove (GTK_CONTAINER (priv->bins [index]), child);
		}
	}

	if ((parent = gtk_widget_get_parent (GTK_WIDGET (tile)))) {
		g_object_ref (G_OBJECT (tile));
		gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (tile));
		gtk_container_add (GTK_CONTAINER (priv->bins [index]), GTK_WIDGET (tile));
		g_object_unref (G_OBJECT (tile));
	}
	else
		gtk_container_add (GTK_CONTAINER (priv->bins [index]), GTK_WIDGET (tile));

	g_object_set_data (G_OBJECT (tile), "tile-table-bin", GINT_TO_POINTER (index));
}

static void
empty_bin (TileTable *this, gint index)
{
	TileTablePrivate *priv = PRIVATE (this);

	GtkWidget *child;


	if (priv->bins [index]) {
		if ((child = gtk_bin_get_child (priv->bins [index]))) {
			g_object_ref (G_OBJECT (child));
			gtk_container_remove (GTK_CONTAINER (priv->bins [index]), child);
		}
	}
}

static void
resize_table (TileTable *this, guint n_rows_new, guint n_cols_new)
{
	TileTablePrivate *priv = PRIVATE (this);

	GtkBin **bins_new;
	gint     n_bins_new;

	GtkWidget *child;

	gint i;


	n_bins_new = n_rows_new * n_cols_new;

	if (n_bins_new == priv->n_bins)
		return;

	bins_new = g_new0 (GtkBin *, n_bins_new);

	if (priv->bins) {
		for (i = 0; i < priv->n_bins; ++i) {
			if (i < n_bins_new)
				bins_new [i] = priv->bins [i];
			else {
				if (priv->bins [i]) {
					if ((child = gtk_bin_get_child (priv->bins [i]))) {
						g_object_ref (G_OBJECT (child));
						gtk_container_remove (
							GTK_CONTAINER (priv->bins [i]), child);
					}

					gtk_widget_destroy (GTK_WIDGET (priv->bins [i]));
				}
			}
		}

		g_free (priv->bins);
	}

	priv->bins   = bins_new;
	priv->n_bins = n_bins_new;

	gtk_table_resize (GTK_TABLE (this), n_rows_new, n_cols_new);
}

static void
save_reorder (TileTable *this, GList *tiles_new)
{
	TileTablePrivate *priv = PRIVATE (this);

	gboolean equal = FALSE;

	gchar **uris;
	gint    n_items;

	GList *node_u;
	GList *node_v;
	gint   i;


	if (! tiles_new || priv->tiles == tiles_new)
		return;

	n_items = g_list_length (priv->tiles);

	if (n_items == g_list_length (tiles_new)) {
		node_u = priv->tiles;
		node_v = tiles_new;
		equal  = TRUE;

		while (equal && node_u && node_v) {
			if (tile_compare (node_u->data, node_v->data))
                		equal = FALSE;

			node_u = node_u->next;
			node_v = node_v->next;
		}
	}

	if (! equal) {
		g_list_free (priv->tiles);
		priv->tiles = g_list_copy (tiles_new);
		update_bins (this, priv->tiles);

		uris = g_new0 (gchar *, n_items + 1);

		for (node_u = priv->tiles, i = 0; node_u && i < n_items; node_u = node_u->next, ++i)
			uris [i] = g_strdup (TILE (node_u->data)->uri);

		bookmark_agent_reorder_items (priv->agent, (const gchar **) uris);
		g_object_notify (G_OBJECT (this), TILE_TABLE_TILES_PROP);

		g_strfreev (uris);
	}
}

static void
tile_activated_cb (Tile *tile, TileEvent *event, gpointer user_data)
{
	if (event->type == TILE_EVENT_ACTIVATED_DOUBLE_CLICK)
		return;

	tile_trigger_action_with_time (tile, tile->default_action, event->time);
}

static void
tile_drag_begin_cb (GtkWidget *widget, GdkDragContext *context, gpointer user_data)
{
	TileTablePrivate *priv = PRIVATE (user_data);

	priv->reord_bin_orig = GPOINTER_TO_INT (
		g_object_get_data (G_OBJECT (widget), "tile-table-bin"));
}

static void
tile_drag_end_cb (GtkWidget *widget, GdkDragContext *context, gpointer user_data)
{
	TileTablePrivate *priv = PRIVATE (user_data);

	priv->reord_bin_orig = -1;
	priv->reord_bin_curr = -1;
}

static void
agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	tile_table_reload (TILE_TABLE (user_data));
}
