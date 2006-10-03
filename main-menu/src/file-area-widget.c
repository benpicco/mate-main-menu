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

#include "config.h"

#include "file-area-widget.h"

#include <string.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-monitor.h>

#include "application-tile.h"
#include "document-tile.h"
#include "egg-recent-item.h"
#include "gnome-utils.h"
#include "recent-files.h"
#include "slab-gnome-util.h"
#include "tile-table.h"

#define MAIN_MENU_GCONF_DIR  "/desktop/gnome/applications/main-menu"
#define FILE_AREA_GCONF_DIR  MAIN_MENU_GCONF_DIR "/file-area"
#define LOCK_DOWN_GCONF_DIR  MAIN_MENU_GCONF_DIR "/lock-down"

#define FILE_BROWSER_GCONF_KEY     MAIN_MENU_GCONF_DIR "/file_browser"
#define APP_BROWSER_GCONF_KEY      MAIN_MENU_GCONF_DIR "/application_browser"
#define FILE_CLASS_GCONF_KEY       FILE_AREA_GCONF_DIR "/file_class"
#define USER_SPEC_APPS_GCONF_KEY   FILE_AREA_GCONF_DIR "/user_specified_apps"
#define ITEM_LIMIT_GCONF_KEY       FILE_AREA_GCONF_DIR "/item_limit"
#define AB_LINK_VISIBLE_GCONF_KEY  LOCK_DOWN_GCONF_DIR "/application_browser_link_visible"
#define SHOWABLE_TYPES_GCONF_KEY   LOCK_DOWN_GCONF_DIR "/showable_file_types"
#define DISABLE_TERMINAL_GCONF_KEY "/desktop/gnome/lockdown/disable_command_line"

#define DESKTOP_ITEM_TERMINAL_EMULATOR_FLAG "TerminalEmulator"

G_DEFINE_TYPE (FileAreaWidget, file_area_widget, GTK_TYPE_VBOX)

typedef struct {
	MainMenuUI *ui;
	MainMenuEngine *engine;
	
	gint limit;
	
	GtkComboBox *selector;
	GtkNotebook *notebook;
	GtkButton *browser_link;
	
	GtkListStore *file_tables;
} FileAreaWidgetPrivate;

#define FILE_AREA_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FILE_AREA_WIDGET_TYPE, FileAreaWidgetPrivate))

#define COLUMN_FILE_CLASS     0
#define COLUMN_SELECTOR_LABEL 1
#define COLUMN_PAGE_ID        2
#define COLUMN_TILE_TABLE     3
#define COLUMN_TILE_LIST      4

#define TILE_WIDTH_SCALE 6

static void file_area_widget_finalize (GObject *);

static void layout_file_area (FileAreaWidget *);
static void update_browser_link (FileAreaWidget *, FileClass);
static void load_tables (FileAreaWidget *);
static void select_page (FileAreaWidget *, GtkTreeIter *);
static GList *get_tiles (FileAreaWidget *, FileClass);
static void update_table (FileAreaWidget *, FileClass);

static gboolean application_is_blacklisted (const gchar *);

static void selector_changed_cb (GtkComboBox *, gpointer);
static void link_clicked_cb (GtkButton *, gpointer);
static void notebook_show_cb (GtkWidget *, gpointer);

static void tile_activated_cb (Tile *, TileEvent *, gpointer);
static void tile_action_triggered_cb (Tile *, TileEvent *, TileAction *, gpointer);

static void recent_apps_store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *,
	const gchar *, GnomeVFSMonitorEventType, gpointer);
static void recent_files_store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *,
	const gchar *, GnomeVFSMonitorEventType, gpointer);

static void tile_table_update_cb (TileTable *, TileTableUpdateEvent *, gpointer);
static void tile_table_uri_added_cb (TileTable *, TileTableURIAddedEvent *, gpointer);
static void tile_table_n_rows_notify_cb (GObject *, GParamSpec *, gpointer);

static void user_spec_apps_gconf_notify_cb (GConfClient *, guint, GConfEntry *, gpointer);

GtkWidget *file_area_widget_new (MainMenuUI * ui, MainMenuEngine * engine)
{
	FileAreaWidget *this;
	FileAreaWidgetPrivate *priv;

	GtkWidget *alignment;
	GtkWidget *table;
	GtkCellRenderer *renderer;

	GtkTreeIter iter_cur;

	GtkTreeIter iter;
	gint page_id;

	GList *showable_types_list;
	gboolean showable [FILE_CLASS_SENTINEL];

	GList *tiles;

	GnomeVFSMonitorHandle *handle;
	gchar *filename;
	gchar *uri;

	gchar *markup;

	GList *node;
	gint i;

	this = g_object_new (FILE_AREA_WIDGET_TYPE, "homogeneous", FALSE, "spacing", 12, NULL);

	priv = FILE_AREA_WIDGET_GET_PRIVATE (this);
	priv->ui = ui;
	priv->engine = engine;

	this->selector_label = get_main_menu_section_header (_("Show:"));

	priv->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_notebook_set_show_tabs (priv->notebook, FALSE);
	gtk_notebook_set_show_border (priv->notebook, FALSE);

	priv->file_tables = gtk_list_store_new (
		5, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT, TILE_TABLE_TYPE, G_TYPE_POINTER);

	priv->selector =
		GTK_COMBO_BOX (gtk_combo_box_new_with_model (GTK_TREE_MODEL (priv->file_tables)));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->selector), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->selector), renderer, "text", 1,
		NULL);

	priv->limit = GPOINTER_TO_INT (get_gconf_value (ITEM_LIMIT_GCONF_KEY));

	showable_types_list = (GList *) get_gconf_value (SHOWABLE_TYPES_GCONF_KEY);

	for (i = 0; i < FILE_CLASS_SENTINEL; ++i)
		showable [i] = FALSE;

	for (node = showable_types_list; node; node = node->next)
		showable [GPOINTER_TO_INT (node->data)] = TRUE;

	g_list_free (showable_types_list);

	for (i = 0; i < FILE_CLASS_SENTINEL; ++i) {
		if (showable [i]) {
			table = tile_table_new (2, i == USER_SPECIFIED_APPS ? TRUE : FALSE,
				TILE_TABLE_REORDERING_PUSH_PULL);

			alignment = gtk_alignment_new (0.5, 0.0, 0.0, 0.0);
			gtk_container_add (GTK_CONTAINER (alignment), table);

			page_id = gtk_notebook_append_page (priv->notebook, alignment, NULL);

			gtk_list_store_append (priv->file_tables, &iter);

			tiles = get_tiles (this, i);

			switch (i) {
			case USER_SPECIFIED_APPS:
				markup = _("Favorite Applications");

				connect_gconf_notify (USER_SPEC_APPS_GCONF_KEY,
					user_spec_apps_gconf_notify_cb, this);

				g_signal_connect (G_OBJECT (table), "tile-table-update",
					G_CALLBACK (tile_table_update_cb), this);

				g_signal_connect (G_OBJECT (table), "tile-table-uri-added",
					G_CALLBACK (tile_table_uri_added_cb), this);

				g_signal_connect (G_OBJECT (table), "notify::n-rows",
					G_CALLBACK (tile_table_n_rows_notify_cb), this);
				break;

			case RECENTLY_USED_APPS:
				markup = _("Recently Used Applications");

				filename = g_build_filename (
					g_get_home_dir (), RECENT_APPS_FILE_PATH, NULL);

				uri = gnome_vfs_get_uri_from_local_path (filename);

				gnome_vfs_monitor_add (&handle, uri, GNOME_VFS_MONITOR_FILE,
					recent_apps_store_monitor_cb, this);

				g_free (filename);
				g_free (uri);
				break;

			case RECENT_FILES:
				markup = _("Recent Documents");

				filename = g_build_filename (
					g_get_home_dir (), RECENT_FILES_FILE_PATH, NULL);

				uri = gnome_vfs_get_uri_from_local_path (filename);

				gnome_vfs_monitor_add (&handle, uri, GNOME_VFS_MONITOR_FILE,
					recent_files_store_monitor_cb, this);

				g_free (filename);
				g_free (uri);
				break;
				break;

			default:
				markup = "Unknown Type";
				break;
			}

			gtk_list_store_set (priv->file_tables, &iter,
				COLUMN_FILE_CLASS, i,
				COLUMN_SELECTOR_LABEL, markup,
				COLUMN_PAGE_ID, page_id,
				COLUMN_TILE_TABLE, table,
				COLUMN_TILE_LIST, tiles,
				-1);

			if (GPOINTER_TO_INT (get_gconf_value (FILE_CLASS_GCONF_KEY)) == i)
				iter_cur = iter;
		}
	}

	load_tables (this);

	g_signal_connect (G_OBJECT (priv->selector), "changed", G_CALLBACK (selector_changed_cb),
		this);

	g_signal_connect (G_OBJECT (priv->notebook), "show", G_CALLBACK (notebook_show_cb), this);

	if (GPOINTER_TO_INT (get_gconf_value (AB_LINK_VISIBLE_GCONF_KEY))) {
		priv->browser_link = GTK_BUTTON (gtk_button_new ());

		update_browser_link (this,
			GPOINTER_TO_INT (get_gconf_value (FILE_CLASS_GCONF_KEY)));

		g_signal_connect (G_OBJECT (priv->browser_link), "clicked",
			G_CALLBACK (link_clicked_cb), this);
	}

	layout_file_area (this);

	select_page (this, &iter_cur);

	return GTK_WIDGET (this);
}

static void
file_area_widget_class_init (FileAreaWidgetClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	g_obj_class->finalize = file_area_widget_finalize;

	g_type_class_add_private (this_class, sizeof (FileAreaWidgetPrivate));
}

static void
file_area_widget_init (FileAreaWidget *this)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (this);

	this->selector_label = NULL;

	priv->ui = NULL;
	priv->engine = NULL;

	priv->limit = 0;

	priv->selector = NULL;
	priv->notebook = NULL;
	priv->browser_link = NULL;

	priv->file_tables = NULL;
}

static void
file_area_widget_finalize (GObject *g_object)
{
	/* FIXME */
	(*G_OBJECT_CLASS (file_area_widget_parent_class)->finalize) (g_object);
}

void
file_area_widget_update (FileAreaWidget *this)
{
	update_browser_link (this, GPOINTER_TO_INT (get_gconf_value (FILE_CLASS_GCONF_KEY)));
}

void
layout_file_area (FileAreaWidget *this)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (this);

	GtkWidget *hbox;
	GtkWidget *alignment;

	hbox = gtk_hbox_new (FALSE, 12);

	gtk_misc_set_alignment (GTK_MISC (this->selector_label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), this->selector_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (priv->selector), FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (this), hbox, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (this), GTK_WIDGET (priv->notebook), FALSE, FALSE, 0);

	if (priv->browser_link) {
		alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
		gtk_container_add (GTK_CONTAINER (alignment), GTK_WIDGET (priv->browser_link));

		gtk_box_pack_end (GTK_BOX (this), alignment, FALSE, FALSE, 0);
	}
}

static void
update_browser_link (FileAreaWidget *this, FileClass file_class)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (this);

	switch (file_class) {
		case RECENTLY_USED_APPS:
		case USER_SPECIFIED_APPS:
			gtk_button_set_label (priv->browser_link, _("More Applications..."));
			break;

		case RECENT_FILES:
			gtk_button_set_label (priv->browser_link, _("All Documents..."));
			break;

		default:
			break;
	}
}

static void
load_tables (FileAreaWidget *this)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (this);

	TileTable *table;

	gpointer list_ptr;

	GtkTreeIter iter;
	gboolean has_next;

	has_next = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->file_tables), &iter);

	while (has_next)
	{
		gtk_tree_model_get (GTK_TREE_MODEL (priv->file_tables), &iter, COLUMN_TILE_TABLE,
			&table, COLUMN_TILE_LIST, &list_ptr, -1);

		tile_table_load_tiles (table, (GList *) list_ptr);

		has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->file_tables), &iter);
	}
}

static void
select_page (FileAreaWidget *this, GtkTreeIter *iter)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (this);

	FileClass file_class;
	gint page_id;

	gtk_tree_model_get (GTK_TREE_MODEL (priv->file_tables), iter, 0, &file_class, 2, &page_id,
		-1);

	gtk_combo_box_set_active_iter (priv->selector, iter);
	gtk_notebook_set_current_page (priv->notebook, page_id);
	update_browser_link (this, file_class);

	set_gconf_value (FILE_CLASS_GCONF_KEY, GINT_TO_POINTER (file_class));
}

static GList *
get_tiles (FileAreaWidget *this, FileClass file_class)
{
	GList *files = NULL;
	GList *tiles = NULL;

	GtkWidget *tile;
	gchar *desktop_item_url;

	GnomeDesktopItem *item;
	const gchar *categories;
	gboolean disable_term;

	gint icon_width;

	GList *existing_files = NULL;
	gboolean lost_tile = FALSE;

	GList *node;


	disable_term = GPOINTER_TO_INT (get_gconf_value (DISABLE_TERMINAL_GCONF_KEY));

	if (file_class == USER_SPECIFIED_APPS) {
		files = get_gconf_value (USER_SPEC_APPS_GCONF_KEY);

		for (node = files; node; node = node->next) {
			tile = application_tile_new ((gchar *) node->data);

			if (disable_term) {
				item = application_tile_get_desktop_item (APPLICATION_TILE (tile));
				categories = gnome_desktop_item_get_string (
					item, GNOME_DESKTOP_ITEM_CATEGORIES);

				if (categories && strstr (categories, DESKTOP_ITEM_TERMINAL_EMULATOR_FLAG)) {
					gtk_widget_destroy (tile);
					tile = NULL;
				}
			}

			if (tile) {
				tiles = g_list_append (tiles, tile);

				existing_files = g_list_append (existing_files, node->data);
			}
			else {
				lost_tile = TRUE;

				g_free (node->data);
			}
		}

		if (lost_tile) {
			set_gconf_value (USER_SPEC_APPS_GCONF_KEY, existing_files);

			for (node = existing_files; node; node = node->next)
				g_free (node->data);

			g_list_free (existing_files);
		}
	}

	else if (file_class == RECENTLY_USED_APPS) {
		files = get_recent_files (RECENT_APPS_FILE_PATH);

		for (node = files; node; node = node->next) {
			desktop_item_url = egg_recent_item_get_uri ((EggRecentItem *) node->data);

			if (!application_is_blacklisted (desktop_item_url)) {
				tile = application_tile_new (desktop_item_url);

				if (disable_term) {
					item = application_tile_get_desktop_item (
						APPLICATION_TILE (tile));
					categories = gnome_desktop_item_get_string (
						item, GNOME_DESKTOP_ITEM_CATEGORIES);

					if (categories && strstr (categories, DESKTOP_ITEM_TERMINAL_EMULATOR_FLAG)) {
						gtk_widget_destroy (tile);
						tile = NULL;
					}
				}

				if (tile)
					tiles = g_list_append (tiles, tile);
			}

			g_free (desktop_item_url);
			egg_recent_item_unref (node->data);
		}
	}

	else {
		files = get_recent_files (RECENT_FILES_FILE_PATH);

		for (node = files; node; node = node->next)
			tiles = g_list_append (tiles,
				document_tile_new ((EggRecentItem *) node->data));
	}

	gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &icon_width, NULL);

	for (node = tiles; node; node = node->next) {
		gtk_widget_set_size_request (GTK_WIDGET (node->data), TILE_WIDTH_SCALE *icon_width,
			-1);

		g_signal_connect (G_OBJECT (node->data), "tile-activated",
			G_CALLBACK (tile_activated_cb), NULL);

		g_signal_connect (G_OBJECT (node->data), "tile-action-triggered",
			G_CALLBACK (tile_action_triggered_cb), this);
	}

	g_list_free (files);

	return tiles;
}

static void
update_table (FileAreaWidget *this, FileClass file_class)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (this);

	GtkTreeIter iter;
	gboolean has_next;

	FileClass file_class_i;
	TileTable *table;

	GList *tiles = NULL;

	has_next = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->file_tables), &iter);

	while (has_next) {
		gtk_tree_model_get (GTK_TREE_MODEL (priv->file_tables), &iter, COLUMN_FILE_CLASS,
			&file_class_i, COLUMN_TILE_TABLE, &table, -1);

		if (file_class_i == file_class) {
			tiles = get_tiles (this, file_class_i);

			gtk_list_store_set (priv->file_tables, &iter, COLUMN_TILE_LIST, tiles, -1);

			tile_table_load_tiles (table, tiles);

			if (file_class == USER_SPECIFIED_APPS)
				tile_table_set_limit (table, -1);
		}

		has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->file_tables), &iter);
	}
}

static gboolean
application_is_blacklisted (const gchar *desktop_item_url)
{
	GSList *blacklist;
	GSList *node;
	gboolean retval;

	retval = FALSE;
	blacklist = get_slab_gconf_slist (SLAB_FILE_BLACKLIST);

	for (node = blacklist; node; node = node->next) {
		if (strstr (desktop_item_url, (gchar *) node->data)) {
			retval = TRUE;
			/* free the rest of list and break out */
			for (; node; node = node->next)
				g_free (node->data);
			break;
		}
		g_free (node->data);
	}
	g_slist_free (blacklist);

	return retval;
}

static void
selector_changed_cb (GtkComboBox *combo_box, gpointer user_data)
{
	FileAreaWidget *this = FILE_AREA_WIDGET (user_data);

	GtkTreeIter iter;

	gtk_combo_box_get_active_iter (combo_box, &iter);

	select_page (this, &iter);
}

static void
link_clicked_cb (GtkButton *button, gpointer user_data)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (user_data);

	GtkTreeIter iter;
	FileClass file_class;

	gchar *desktop_item_id;
	GnomeDesktopItem *browser;

	gtk_combo_box_get_active_iter (priv->selector, &iter);

	gtk_tree_model_get (GTK_TREE_MODEL (priv->file_tables), &iter, 0, &file_class, -1);

	if (file_class == RECENT_FILES)
		desktop_item_id = (gchar *) get_gconf_value (FILE_BROWSER_GCONF_KEY);
	else
		desktop_item_id = (gchar *) get_gconf_value (APP_BROWSER_GCONF_KEY);

	browser = load_desktop_item_by_unknown_id (desktop_item_id);

	if (browser) {
		open_desktop_item_exec (browser);

		main_menu_ui_close (priv->ui, FALSE);
	}

	g_free (desktop_item_id);
}

static void
notebook_show_cb (GtkWidget *widget, gpointer user_data)
{
	FileAreaWidget *this = FILE_AREA_WIDGET (user_data);
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (this);

	GtkTreeIter iter;
	gboolean has_next;

	FileClass file_class;

	has_next = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->file_tables), &iter);

	while (has_next) {
		gtk_tree_model_get (GTK_TREE_MODEL (priv->file_tables), &iter, 0, &file_class, -1);

		if (GPOINTER_TO_INT (get_gconf_value (FILE_CLASS_GCONF_KEY)) == file_class) {
			select_page (this, &iter);

			return;
		}

		has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->file_tables), &iter);
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
tile_action_triggered_cb (Tile *tile, TileEvent *event, TileAction *action, gpointer user_data)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (user_data);

	if (TILE_ACTION_CHECK_FLAG (action, TILE_ACTION_OPENS_NEW_WINDOW)
		&& !TILE_ACTION_CHECK_FLAG (action, TILE_ACTION_OPENS_HELP))
		main_menu_ui_close (priv->ui, FALSE);
}

static void
recent_apps_store_monitor_cb (GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	update_table (FILE_AREA_WIDGET (user_data), RECENTLY_USED_APPS);
}

static void
recent_files_store_monitor_cb (GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	update_table (FILE_AREA_WIDGET (user_data), RECENT_FILES);
}

static void
tile_table_update_cb (TileTable *table, TileTableUpdateEvent *event, gpointer user_data)
{
	GList *new_app_list = NULL;

	GList *node_u, *node_v;
	gboolean equal;

	GList *node;

	if (g_list_length (event->tiles_prev) == g_list_length (event->tiles_curr)) {
		for (
			node_u = event->tiles_prev, node_v = event->tiles_curr, equal = TRUE;
			equal && node_u && node_v;
			node_u = node_u->next, node_v = node_v->next )

			if (tile_compare (node_u->data, node_v->data));
                equal = FALSE;
	}

	if (equal)
		return;

	for (node = event->tiles_curr; node; node = node->next)
		new_app_list = g_list_append (new_app_list, TILE (node->data)->uri);

	set_gconf_value (USER_SPEC_APPS_GCONF_KEY, new_app_list);
}

static void
tile_table_uri_added_cb (TileTable *table, TileTableURIAddedEvent *event, gpointer user_data)
{
	GList *app_list;
	gint uri_len;

	if (!event->uri)
		return;

	uri_len = strlen (event->uri);

	if (!strcmp (&event->uri [uri_len - 8], ".desktop")) {
		app_list = get_gconf_value (USER_SPEC_APPS_GCONF_KEY);
		app_list = g_list_append (app_list, event->uri);
		set_gconf_value (USER_SPEC_APPS_GCONF_KEY, app_list);
	}
}

static void
tile_table_n_rows_notify_cb (GObject *obj, GParamSpec *spec, gpointer user_data)
{
	FileAreaWidgetPrivate *priv = FILE_AREA_WIDGET_GET_PRIVATE (user_data);

	GtkTreeIter iter;
	gboolean has_next;

	TileTable *table;

	gint gconf_limit;

	guint n_rows;
	guint n_cols;

	g_object_get (obj, "n-rows", &n_rows, "n-columns", &n_cols, NULL);

	priv->limit = n_rows * n_cols;
	gconf_limit = GPOINTER_TO_INT (get_gconf_value (ITEM_LIMIT_GCONF_KEY));

	if (priv->limit < gconf_limit)
		priv->limit = gconf_limit;

	has_next = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->file_tables), &iter);

	while (has_next) {
		gtk_tree_model_get (GTK_TREE_MODEL (priv->file_tables), &iter, COLUMN_TILE_TABLE,
			&table, -1);

		tile_table_set_limit (table, priv->limit);

		has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->file_tables), &iter);
	}
}

static void
user_spec_apps_gconf_notify_cb (GConfClient *client, guint conn_id, GConfEntry *entry,
	gpointer user_data)
{
	update_table (FILE_AREA_WIDGET (user_data), USER_SPECIFIED_APPS);
}
