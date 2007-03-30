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

#include "main-menu-ui.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <panel-applet.h>
#include <glade/glade.h>
#include <cairo.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "tile.h"
#include "application-tile.h"
#include "document-tile.h"
#include "directory-tile.h"
#include "system-tile.h"
#include "hard-drive-status-tile.h"
#include "network-status-tile.h"

#include "tile-table.h"

#include "tomboykeybinder.h"
#include "libslab-utils.h"
#include "double-click-detector.h"

#define ROOT_GCONF_DIR             "/desktop/gnome/applications/main-menu"
#define CURRENT_PAGE_GCONF_KEY     ROOT_GCONF_DIR "/file-area/file_class"
#define URGENT_CLOSE_GCONF_KEY     ROOT_GCONF_DIR "/urgent_close"
#define MAX_TOTAL_ITEMS_GCONF_KEY  ROOT_GCONF_DIR "/file-area/max_total_items"
#define MIN_RECENT_ITEMS_GCONF_KEY ROOT_GCONF_DIR "/file-area/min_recent_items"
#define APP_BROWSER_GCONF_KEY      ROOT_GCONF_DIR "/application_browser"
#define FILE_BROWSER_GCONF_KEY     ROOT_GCONF_DIR "/file_browser"
#define SEARCH_CMD_GCONF_KEY       ROOT_GCONF_DIR "/search_command"
#define FILE_MGR_OPEN_GCONF_KEY    ROOT_GCONF_DIR "/file-area/file_mgr_open_cmd"
#define APP_BLACKLIST_GCONF_KEY    ROOT_GCONF_DIR "/file-area/file_blacklist"

#define LOCKDOWN_GCONF_DIR           ROOT_GCONF_DIR "/lock-down"
#define MORE_LINK_VIS_GCONF_KEY      LOCKDOWN_GCONF_DIR "/application_browser_link_visible"
#define SEARCH_VIS_GCONF_KEY         LOCKDOWN_GCONF_DIR "/search_area_visible"
#define STATUS_VIS_GCONF_KEY         LOCKDOWN_GCONF_DIR "/status_area_visible"
#define SYSTEM_VIS_GCONF_KEY         LOCKDOWN_GCONF_DIR "/system_area_visible"
#define SHOWABLE_TYPES_GCONF_KEY     LOCKDOWN_GCONF_DIR "/showable_file_types"
#define MODIFIABLE_SYSTEM_GCONF_KEY  LOCKDOWN_GCONF_DIR "/user_modifiable_system_area"
#define MODIFIABLE_APPS_GCONF_KEY    LOCKDOWN_GCONF_DIR "/user_modifiable_apps"
#define MODIFIABLE_DOCS_GCONF_KEY    LOCKDOWN_GCONF_DIR "/user_modifiable_docs"
#define MODIFIABLE_DIRS_GCONF_KEY    LOCKDOWN_GCONF_DIR "/user_modifiable_dirs"
#define DISABLE_TERMINAL_GCONF_KEY   "/desktop/gnome/lockdown/disable_command_line"
#define PANEL_LOCKDOWN_GCONF_DIR     "/apps/panel/global"
#define DISABLE_LOGOUT_GCONF_KEY     PANEL_LOCKDOWN_GCONF_DIR "/disable_log_out"
#define DISABLE_LOCKSCREEN_GCONF_KEY PANEL_LOCKDOWN_GCONF_DIR "/disable_lock_screen"

G_DEFINE_TYPE (MainMenuUI, main_menu_ui, G_TYPE_OBJECT)

typedef struct {
	PanelApplet *panel_applet;
	GtkWidget   *panel_about_dialog;

	GladeXML *main_menu_xml;
	GladeXML *panel_button_xml;

	GtkToggleButton *panel_buttons [4];
	GtkToggleButton *panel_button;

	GtkWidget *slab_window;

	GtkWidget *top_pane;
	GtkWidget *left_pane;

	GtkWidget *search_section;
	GtkWidget *search_entry;

	GtkNotebook *file_section;
	GtkWidget   *page_selectors    [3];
	gint         notebook_page_ids [3];

	TileTable     *file_tables     [5];
	GtkWidget     *table_sections  [5];
	gboolean       allowable_types [5];

	TileTable *sys_table;

	GtkWidget *more_buttons  [3];
	GtkWidget *more_sections [3];

	gint max_total_items;

	GtkWidget *status_section;
	GtkWidget *system_section;

	BookmarkAgent *bm_agents [BOOKMARK_STORE_N_TYPES];

	GnomeVFSVolumeMonitor *volume_mon;
	GList                 *mounts;

	guint search_cmd_gconf_mntr_id;
	guint current_page_gconf_mntr_id;
	guint more_link_vis_gconf_mntr_id;
	guint search_vis_gconf_mntr_id;
	guint status_vis_gconf_mntr_id;
	guint system_vis_gconf_mntr_id;
	guint showable_types_gconf_mntr_id;
	guint modifiable_system_gconf_mntr_id;
	guint modifiable_apps_gconf_mntr_id;
	guint modifiable_docs_gconf_mntr_id;
	guint modifiable_dirs_gconf_mntr_id;
	guint disable_term_gconf_mntr_id;
	guint disable_logout_gconf_mntr_id;
	guint disable_lockscreen_gconf_mntr_id;

	gboolean ptr_is_grabbed;
	gboolean kbd_is_grabbed;
} MainMenuUIPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_UI_TYPE, MainMenuUIPrivate))

static void main_menu_ui_finalize (GObject *);

static void create_panel_button      (MainMenuUI *);
static void create_slab_window       (MainMenuUI *);
static void create_search_section    (MainMenuUI *);
static void create_file_section      (MainMenuUI *);
static void create_user_apps_section (MainMenuUI *);
static void create_rct_apps_section  (MainMenuUI *);
static void create_user_docs_section (MainMenuUI *);
static void create_rct_docs_section  (MainMenuUI *);
static void create_user_dirs_section (MainMenuUI *);
static void create_system_section    (MainMenuUI *);
static void create_status_section    (MainMenuUI *);
static void create_more_buttons      (MainMenuUI *);
static void setup_file_tables        (MainMenuUI *);
static void setup_bookmark_agents    (MainMenuUI *);
static void setup_lock_down          (MainMenuUI *);

static void       select_page                (MainMenuUI *);
static void       update_limits              (MainMenuUI *);
static void       connect_to_tile_triggers   (MainMenuUI *, TileTable *);
static void       hide_slab_if_urgent_close  (MainMenuUI *);
static void       set_search_section_visible (MainMenuUI *);
static void       set_table_section_visible  (MainMenuUI *, TileTable *);
static gchar    **get_search_argv            (const gchar *);
static void       reorient_panel_button      (MainMenuUI *);
static void       bind_beagle_search_key     (MainMenuUI *);
static void       launch_search              (MainMenuUI *);
static void       grab_pointer_and_keyboard  (MainMenuUI *, guint32);
static void       apply_lockdown_settings    (MainMenuUI *);
static gboolean   app_is_in_blacklist        (const gchar *);

static Tile *item_to_user_app_tile   (BookmarkItem *, gpointer);
static Tile *item_to_recent_app_tile (BookmarkItem *, gpointer);
static Tile *item_to_user_doc_tile   (BookmarkItem *, gpointer);
static Tile *item_to_recent_doc_tile (BookmarkItem *, gpointer);
static Tile *item_to_dir_tile        (BookmarkItem *, gpointer);
static Tile *item_to_system_tile     (BookmarkItem *, gpointer);
static BookmarkItem *app_uri_to_item (const gchar *, gpointer);
static BookmarkItem *doc_uri_to_item (const gchar *, gpointer);

static void     panel_button_clicked_cb           (GtkButton *, gpointer);
static gboolean panel_button_button_press_cb      (GtkWidget *, GdkEventButton *, gpointer);
static void     panel_button_drag_data_rcv_cb     (GtkWidget *, GdkDragContext *, gint, gint,
                                                   GtkSelectionData *, guint, guint, gpointer);
static gboolean slab_window_expose_cb             (GtkWidget *, GdkEventExpose *, gpointer);
static gboolean slab_window_key_press_cb          (GtkWidget *, GdkEventKey *, gpointer);
static gboolean slab_window_button_press_cb       (GtkWidget *, GdkEventButton *, gpointer);
static void     slab_window_allocate_cb           (GtkWidget *, GtkAllocation *, gpointer);
static void     slab_window_map_event_cb          (GtkWidget *, GdkEvent *, gpointer);
static void     slab_window_unmap_event_cb        (GtkWidget *, GdkEvent *, gpointer);
static gboolean slab_window_grab_broken_cb        (GtkWidget *, GdkEvent *, gpointer);
static void     search_entry_activate_cb          (GtkEntry *, gpointer);
static void     page_button_clicked_cb            (GtkButton *, gpointer);
static void     tile_table_notify_cb              (GObject *, GParamSpec *, gpointer);
static void     gtk_table_notify_cb               (GObject *, GParamSpec *, gpointer);
static void     tile_action_triggered_cb          (Tile *, TileEvent *, TileAction *, gpointer);
static void     more_buttons_clicked_cb            (GtkButton *, gpointer);
static void     search_cmd_notify_cb              (GConfClient *, guint, GConfEntry *, gpointer);
static void     current_page_notify_cb            (GConfClient *, guint, GConfEntry *, gpointer);
static void     lockdown_notify_cb                (GConfClient *, guint, GConfEntry *, gpointer);
static void     panel_menu_open_cb                (BonoboUIComponent *, gpointer, const gchar *);
static void     panel_menu_about_cb               (BonoboUIComponent *, gpointer, const gchar *);
static void     panel_applet_change_orient_cb     (PanelApplet *, PanelAppletOrient, gpointer);
static void     panel_applet_change_background_cb (PanelApplet *, PanelAppletBackgroundType, GdkColor *,
                                                   GdkPixmap * pixmap, gpointer);
static void     slab_window_tomboy_bindkey_cb     (gchar *, gpointer);
static void     search_tomboy_bindkey_cb          (gchar *, gpointer);
static gboolean grabbing_window_event_cb          (GtkWidget *, GdkEvent *, gpointer);
static void     user_app_agent_notify_cb          (GObject *, GParamSpec *, gpointer);
static void     user_doc_agent_notify_cb          (GObject *, GParamSpec *, gpointer);
static void     volume_monitor_mount_cb           (GnomeVFSVolumeMonitor *, GnomeVFSVolume *, gpointer);

static GdkFilterReturn slab_gdk_message_filter (GdkXEvent *, GdkEvent *, gpointer);

static const BonoboUIVerb applet_bonobo_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("MainMenuOpen",  panel_menu_open_cb),
	BONOBO_UI_UNSAFE_VERB ("MainMenuAbout", panel_menu_about_cb),
	BONOBO_UI_VERB_END
};

static const gchar *main_menu_authors [] = {
	"Jim Krehl <jimmyk@novell.com>",
	"Scott Reeves <sreeves@novell.com>",
	"Dan Winship <danw@novell.com>",
	NULL
};

static const gchar *main_menu_artists [] = {
	"Garrett LeSage <garrett@novell.com>",
	"Jakub Steiner <jimmac@novell.com>",
	NULL
};

enum {
	APPS_PAGE,
	DOCS_PAGE,
	DIRS_PAGE
};

enum {
	USER_APPS_TABLE,
	RCNT_APPS_TABLE,
	USER_DOCS_TABLE,
	RCNT_DOCS_TABLE,
	USER_DIRS_TABLE
};

enum {
	PANEL_BUTTON_ORIENT_TOP,
	PANEL_BUTTON_ORIENT_BOTTOM,
	PANEL_BUTTON_ORIENT_LEFT,
	PANEL_BUTTON_ORIENT_RIGHT
};

static Atom slab_action_main_menu_atom = None;

MainMenuUI *
main_menu_ui_new (PanelApplet *applet)
{
	MainMenuUI        *this;
	MainMenuUIPrivate *priv;

	gchar *glade_xml_path;


	this = g_object_new (MAIN_MENU_UI_TYPE, NULL);
	priv = PRIVATE (this);

	priv->panel_applet = applet;

	glade_xml_path = g_build_filename (DATADIR, PACKAGE, "slab-window.glade", NULL);

	priv->main_menu_xml    = glade_xml_new (glade_xml_path, "slab-main-menu-window", NULL);
	priv->panel_button_xml = glade_xml_new (glade_xml_path, "slab-panel-button-root", NULL);
	g_free (glade_xml_path);

	setup_bookmark_agents    (this);
	create_panel_button      (this);
	create_slab_window       (this);
	create_search_section    (this);
	create_file_section      (this);
	create_user_apps_section (this);
	create_rct_apps_section  (this);
	create_user_docs_section (this);
	create_rct_docs_section  (this);
	create_user_dirs_section (this);
	create_system_section    (this);
	create_status_section    (this);
	create_more_buttons      (this);
	setup_file_tables        (this);
	setup_lock_down          (this);

	bind_beagle_search_key  (this);
	update_limits           (this);
	select_page             (this);
	apply_lockdown_settings (this);

	return this;
}

static void
main_menu_ui_class_init (MainMenuUIClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	g_obj_class->finalize = main_menu_ui_finalize;

	g_type_class_add_private (this_class, sizeof (MainMenuUIPrivate));
}

static void
main_menu_ui_init (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	priv->panel_applet                               = NULL;
	priv->panel_about_dialog                         = NULL;

	priv->main_menu_xml                              = NULL;
	priv->panel_button_xml                           = NULL;

	priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP]    = NULL;
	priv->panel_buttons [PANEL_BUTTON_ORIENT_BOTTOM] = NULL;
	priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT]   = NULL;
	priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT]  = NULL;
	priv->panel_button                               = NULL;

	priv->slab_window                                = NULL;

	priv->top_pane                                   = NULL;
	priv->left_pane                                  = NULL;

	priv->search_section                             = NULL;
	priv->search_entry                               = NULL;

	priv->file_section                               = NULL;
	priv->page_selectors [APPS_PAGE]                 = NULL;
	priv->page_selectors [DOCS_PAGE]                 = NULL;
	priv->page_selectors [DIRS_PAGE]                 = NULL;
	priv->notebook_page_ids [APPS_PAGE]              = 0;
	priv->notebook_page_ids [DOCS_PAGE]              = 0;
	priv->notebook_page_ids [DIRS_PAGE]              = 0;

	priv->file_tables [USER_APPS_TABLE]              = NULL;
	priv->file_tables [RCNT_APPS_TABLE]              = NULL;
	priv->file_tables [USER_DOCS_TABLE]              = NULL;
	priv->file_tables [RCNT_DOCS_TABLE]              = NULL;
	priv->file_tables [USER_DIRS_TABLE]              = NULL;
	priv->table_sections [USER_APPS_TABLE]           = NULL;
	priv->table_sections [RCNT_APPS_TABLE]           = NULL;
	priv->table_sections [USER_DOCS_TABLE]           = NULL;
	priv->table_sections [RCNT_DOCS_TABLE]           = NULL;
	priv->table_sections [USER_DIRS_TABLE]           = NULL;
	priv->allowable_types [USER_APPS_TABLE]          = TRUE;
	priv->allowable_types [RCNT_APPS_TABLE]          = TRUE;
	priv->allowable_types [USER_DOCS_TABLE]          = TRUE;
	priv->allowable_types [RCNT_DOCS_TABLE]          = TRUE;
	priv->allowable_types [USER_DIRS_TABLE]          = TRUE;

	priv->sys_table                                  = NULL;

	priv->more_buttons [APPS_PAGE]                   = NULL;
	priv->more_buttons [DOCS_PAGE]                   = NULL;
	priv->more_buttons [DIRS_PAGE]                   = NULL;
	priv->more_sections [APPS_PAGE]                  = NULL;
	priv->more_sections [DOCS_PAGE]                  = NULL;
	priv->more_sections [DIRS_PAGE]                  = NULL;

	priv->max_total_items                            = 8;

	priv->status_section                             = NULL;
	priv->system_section                             = NULL;

	priv->volume_mon                                 = NULL;

	priv->search_cmd_gconf_mntr_id                   = 0;
	priv->current_page_gconf_mntr_id                 = 0;
	priv->more_link_vis_gconf_mntr_id                = 0;
	priv->search_vis_gconf_mntr_id                   = 0;
	priv->status_vis_gconf_mntr_id                   = 0;
	priv->system_vis_gconf_mntr_id                   = 0;
	priv->showable_types_gconf_mntr_id               = 0;
	priv->modifiable_system_gconf_mntr_id            = 0;
	priv->modifiable_apps_gconf_mntr_id              = 0;
	priv->modifiable_docs_gconf_mntr_id              = 0;
	priv->modifiable_dirs_gconf_mntr_id              = 0;
	priv->disable_term_gconf_mntr_id                 = 0;
	priv->disable_logout_gconf_mntr_id               = 0;
	priv->disable_lockscreen_gconf_mntr_id           = 0;

	priv->ptr_is_grabbed                             = FALSE;
	priv->kbd_is_grabbed                             = FALSE;
}

static void
main_menu_ui_finalize (GObject *g_obj)
{
	MainMenuUIPrivate *priv = PRIVATE (g_obj);

	GConfClient *client;

	gint i;


	for (i = 0; i < 4; ++i) {
		g_object_unref (G_OBJECT (g_object_get_data (
			G_OBJECT (priv->more_buttons [i]), "double-click-detector")));

		g_object_unref (G_OBJECT (g_object_get_data (
			G_OBJECT (priv->panel_buttons [i]), "double-click-detector")));

		gtk_widget_unref (GTK_WIDGET (priv->panel_buttons [i]));
	}

	libslab_gconf_notify_remove (priv->search_cmd_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->current_page_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->more_link_vis_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->search_vis_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->status_vis_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->system_vis_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->showable_types_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->modifiable_system_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->modifiable_apps_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->modifiable_docs_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->modifiable_dirs_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->disable_term_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->disable_logout_gconf_mntr_id);
	libslab_gconf_notify_remove (priv->disable_lockscreen_gconf_mntr_id);

	client  = gconf_client_get_default ();
	gconf_client_remove_dir (client, PANEL_LOCKDOWN_GCONF_DIR, NULL);
	g_object_unref (client);

	for (i = 0; i < BOOKMARK_STORE_N_TYPES; ++i)
		g_object_unref (priv->bm_agents [i]);

	g_list_foreach (priv->mounts, (GFunc) gnome_vfs_volume_unref, NULL);
	g_list_free (priv->mounts);
	gnome_vfs_volume_monitor_unref (priv->volume_mon);

	G_OBJECT_CLASS (main_menu_ui_parent_class)->finalize (g_obj);
}

static void
create_panel_button (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *button_root;
	GtkWidget *button_parent;

	gint i;


	button_root = glade_xml_get_widget (
		priv->panel_button_xml, "slab-panel-button-root");

	gtk_widget_hide (button_root);

	priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP] = GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-top"));
	priv->panel_buttons [PANEL_BUTTON_ORIENT_BOTTOM] = GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-bottom"));
	priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT] = GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-left"));
	priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT] = GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-right"));

	for (i = 0; i < 4; ++i) {
		g_object_set_data (
			G_OBJECT (priv->panel_buttons [i]), "double-click-detector",
			double_click_detector_new ());

		button_parent = gtk_widget_get_parent (GTK_WIDGET (priv->panel_buttons [i]));

		gtk_widget_ref (GTK_WIDGET (priv->panel_buttons [i]));
		gtk_container_remove (
			GTK_CONTAINER (button_parent), GTK_WIDGET (priv->panel_buttons [i]));

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "clicked",
			G_CALLBACK (panel_button_clicked_cb), this);

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "button_press_event",
			G_CALLBACK (panel_button_button_press_cb), this);

		gtk_drag_dest_set (
			GTK_WIDGET (priv->panel_buttons [i]),
			GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);
		gtk_drag_dest_add_uri_targets (GTK_WIDGET (priv->panel_buttons [i]));

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "drag-data-received",
			G_CALLBACK (panel_button_drag_data_rcv_cb), this);
	}

	gtk_widget_destroy (button_root);

	panel_applet_set_flags (priv->panel_applet, PANEL_APPLET_EXPAND_MINOR);

	reorient_panel_button (this);

	panel_applet_setup_menu_from_file (
		priv->panel_applet, NULL, "GNOME_MainMenu_ContextMenu.xml",
		NULL, applet_bonobo_verbs, this);

	g_signal_connect (
		G_OBJECT (priv->panel_applet), "change_orient",
		G_CALLBACK (panel_applet_change_orient_cb), this);

	g_signal_connect (
		G_OBJECT (priv->panel_applet), "change_background",
		G_CALLBACK (panel_applet_change_background_cb), this);
}

static void
create_slab_window (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GdkAtom slab_action_atom;


	priv->slab_window = glade_xml_get_widget (
		priv->main_menu_xml, "slab-main-menu-window");
	gtk_widget_set_app_paintable (priv->slab_window, TRUE);
	gtk_widget_hide              (priv->slab_window);
	gtk_window_stick             (GTK_WINDOW (priv->slab_window));

	priv->top_pane  = glade_xml_get_widget (priv->main_menu_xml, "top-pane");
	priv->left_pane = glade_xml_get_widget (priv->main_menu_xml, "left-pane");

	tomboy_keybinder_init ();
	tomboy_keybinder_bind ("<Ctrl>Escape", slab_window_tomboy_bindkey_cb, this);

	slab_action_atom = gdk_atom_intern ("_SLAB_ACTION", FALSE);
	slab_action_main_menu_atom = XInternAtom (
		GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "_SLAB_ACTION_MAIN_MENU", FALSE);
	gdk_display_add_client_message_filter (
		gdk_display_get_default (), slab_action_atom, slab_gdk_message_filter, this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "expose-event",
		G_CALLBACK (slab_window_expose_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "key-press-event",
		G_CALLBACK (slab_window_key_press_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "button-press-event",
		G_CALLBACK (slab_window_button_press_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "grab-broken-event",
		G_CALLBACK (slab_window_grab_broken_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "size-allocate",
		G_CALLBACK (slab_window_allocate_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "map-event",
		G_CALLBACK (slab_window_map_event_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "unmap-event",
		G_CALLBACK (slab_window_unmap_event_cb), this);
}

static void
create_search_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

        priv->search_section = glade_xml_get_widget (priv->main_menu_xml, "search-section");
	priv->search_entry   = glade_xml_get_widget (priv->main_menu_xml, "search-entry");

	g_signal_connect (
		G_OBJECT (priv->search_entry), "activate",
		G_CALLBACK (search_entry_activate_cb), this);

	set_search_section_visible (this);

	priv->search_cmd_gconf_mntr_id = libslab_gconf_notify_add (
		SEARCH_CMD_GCONF_KEY, search_cmd_notify_cb, this);
}

static void
create_file_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *pages [3];

	gint i;


	priv->file_section = GTK_NOTEBOOK (glade_xml_get_widget (
		priv->main_menu_xml, "file-area-notebook"));

	priv->page_selectors [APPS_PAGE] = glade_xml_get_widget (
		priv->main_menu_xml, "slab-page-selector-button-applications");
	priv->page_selectors [DOCS_PAGE] = glade_xml_get_widget (
		priv->main_menu_xml, "slab-page-selector-button-documents");
	priv->page_selectors [DIRS_PAGE] = glade_xml_get_widget (
		priv->main_menu_xml, "slab-page-selector-button-places");

	pages [APPS_PAGE] = glade_xml_get_widget (priv->main_menu_xml, "applications-page");
	pages [DOCS_PAGE] = glade_xml_get_widget (priv->main_menu_xml, "documents-page");
	pages [DIRS_PAGE] = glade_xml_get_widget (priv->main_menu_xml, "places-page");

	for (i = 0; i < 3; ++i) {
		gtk_container_child_get (
			GTK_CONTAINER (priv->file_section), pages [i],
			"position", & priv->notebook_page_ids [i], NULL);

		g_object_set_data (
			G_OBJECT (priv->page_selectors [i]), "page-type", GINT_TO_POINTER (i));

		g_signal_connect (
			G_OBJECT (priv->page_selectors [i]), "clicked",
			G_CALLBACK (page_button_clicked_cb), this);
	}

	priv->current_page_gconf_mntr_id = libslab_gconf_notify_add (
		CURRENT_PAGE_GCONF_KEY, current_page_notify_cb, this);

	priv->table_sections [USER_APPS_TABLE] = glade_xml_get_widget (
		priv->main_menu_xml, "user-apps-section");
	priv->table_sections [RCNT_APPS_TABLE] = glade_xml_get_widget (
		priv->main_menu_xml, "recent-apps-section");
	priv->table_sections [USER_DOCS_TABLE] = glade_xml_get_widget (
		priv->main_menu_xml, "user-docs-section");
	priv->table_sections [RCNT_DOCS_TABLE] = glade_xml_get_widget (
		priv->main_menu_xml, "recent-docs-section");
	priv->table_sections [USER_DIRS_TABLE] = glade_xml_get_widget (
		priv->main_menu_xml, "user-dirs-section");
}

static void
create_system_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "system-item-table-container"));

	priv->sys_table = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_SYSTEM], -1, 1, TRUE, TRUE,
		item_to_system_tile, this, app_uri_to_item, NULL));

	connect_to_tile_triggers (this, priv->sys_table);

	gtk_container_add (ctnr, GTK_WIDGET (priv->sys_table));

	g_signal_connect (
		G_OBJECT (priv->sys_table), "notify::" TILE_TABLE_TILES_PROP,
		G_CALLBACK (tile_table_notify_cb), this);

	priv->system_section = glade_xml_get_widget (
		priv->main_menu_xml, "slab-system-section");
}

static void
create_status_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;
	GtkWidget    *tile;

	gint icon_width;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "hard-drive-status-container"));
	tile = hard_drive_status_tile_new ();

	gtk_icon_size_lookup (GTK_ICON_SIZE_DND, & icon_width, NULL);
	gtk_widget_set_size_request (tile, 6 * icon_width, -1);

	g_signal_connect (
		G_OBJECT (tile), "tile-action-triggered",
		G_CALLBACK (tile_action_triggered_cb), this);

	gtk_container_add   (ctnr, tile);
	gtk_widget_show_all (GTK_WIDGET (ctnr));

	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "network-status-container"));
	tile = network_status_tile_new ();

	gtk_widget_set_size_request (tile, 6 * icon_width, -1);

	g_signal_connect (
		G_OBJECT (tile), "tile-action-triggered",
		G_CALLBACK (tile_action_triggered_cb), this);

	gtk_container_add   (ctnr, tile);
	gtk_widget_show_all (GTK_WIDGET (ctnr));

	priv->status_section = glade_xml_get_widget (
		priv->main_menu_xml, "slab-status-section");
}

static void
create_user_apps_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "user-apps-table-container"));

	priv->file_tables [USER_APPS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_USER_APPS], -1, 2, TRUE, TRUE,
		item_to_user_app_tile, this, app_uri_to_item, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [USER_APPS_TABLE]));
}

static void
create_rct_apps_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "recent-apps-table-container"));

	priv->file_tables [RCNT_APPS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_RECENT_APPS], -1, 2, FALSE, FALSE,
		item_to_recent_app_tile, this, NULL, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [RCNT_APPS_TABLE]));
}

static void
create_user_docs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "user-docs-table-container"));

	priv->file_tables [USER_DOCS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_USER_DOCS], -1, 2, TRUE, TRUE,
		item_to_user_doc_tile, this, doc_uri_to_item, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [USER_DOCS_TABLE]));
}

static void
create_rct_docs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "recent-docs-table-container"));

	priv->file_tables [RCNT_DOCS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_RECENT_DOCS], -1, 2, FALSE, FALSE,
		item_to_recent_doc_tile, this, NULL, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [RCNT_DOCS_TABLE]));

	priv->volume_mon = gnome_vfs_get_volume_monitor ();
	priv->mounts = gnome_vfs_volume_monitor_get_mounted_volumes (priv->volume_mon);

	g_signal_connect (priv->volume_mon, "volume_mounted", G_CALLBACK (volume_monitor_mount_cb), this);
	g_signal_connect (priv->volume_mon, "volume_unmounted", G_CALLBACK (volume_monitor_mount_cb), this);
}

static void
create_user_dirs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "user-dirs-table-container"));

	priv->file_tables [USER_DIRS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_USER_DIRS], -1, 2, FALSE, FALSE,
		item_to_dir_tile, this, NULL, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [USER_DIRS_TABLE]));
}

static void
create_more_buttons (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gint i;


	priv->more_buttons [0] = glade_xml_get_widget (priv->main_menu_xml, "more-applications-button");
	priv->more_buttons [1] = glade_xml_get_widget (priv->main_menu_xml, "more-documents-button");
	priv->more_buttons [2] = glade_xml_get_widget (priv->main_menu_xml, "more-places-button");

	for (i = 0; i < 3; ++i) {
		g_object_set_data (
			G_OBJECT (priv->more_buttons [i]),
			"double-click-detector", double_click_detector_new ());

		g_signal_connect (
			G_OBJECT (priv->more_buttons [i]), "clicked",
			G_CALLBACK (more_buttons_clicked_cb), this);
	}

	priv->more_sections [0] = glade_xml_get_widget (priv->main_menu_xml, "more-apps-section");
	priv->more_sections [1] = glade_xml_get_widget (priv->main_menu_xml, "more-docs-section");
	priv->more_sections [2] = glade_xml_get_widget (priv->main_menu_xml, "more-dirs-section");
}

static void
setup_file_tables (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GList *tiles;

	gint i;


	for (i = 0; i < 5; ++i) {
		g_object_set_data (G_OBJECT (priv->file_tables [i]), "table-id", GINT_TO_POINTER (i));
		gtk_table_set_row_spacings (GTK_TABLE (priv->file_tables [i]), 6);
		gtk_table_set_col_spacings (GTK_TABLE (priv->file_tables [i]), 6);

		connect_to_tile_triggers (this, priv->file_tables [i]);

		g_object_get (G_OBJECT (priv->file_tables [i]), TILE_TABLE_TILES_PROP, & tiles, NULL);

		if (g_list_length (tiles) <= 0)
			gtk_widget_hide (priv->table_sections [i]);

		g_signal_connect (
			G_OBJECT (priv->file_tables [i]), "notify::" TILE_TABLE_TILES_PROP,
			G_CALLBACK (tile_table_notify_cb), this);

		g_signal_connect (
			G_OBJECT (priv->file_tables [i]), "notify::n-rows",
			G_CALLBACK (gtk_table_notify_cb), this);
	}
}

static void
setup_bookmark_agents (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);
	gint i;

	for (i = 0; i < BOOKMARK_STORE_N_TYPES; ++i) {
		priv->bm_agents [i] = bookmark_agent_get_instance (i);

		if (i == BOOKMARK_STORE_USER_APPS || i == BOOKMARK_STORE_SYSTEM)
			g_signal_connect (
				G_OBJECT (priv->bm_agents [i]), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
				G_CALLBACK (user_app_agent_notify_cb), this);
		else if (i == BOOKMARK_STORE_USER_DOCS)
			g_signal_connect (
				G_OBJECT (priv->bm_agents [i]), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
				G_CALLBACK (user_doc_agent_notify_cb), this);
	}
}

static void
setup_lock_down (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GConfClient *client;

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, PANEL_LOCKDOWN_GCONF_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
	g_object_unref (client);

	priv->more_link_vis_gconf_mntr_id = libslab_gconf_notify_add (
		MORE_LINK_VIS_GCONF_KEY, lockdown_notify_cb, this);
	priv->search_vis_gconf_mntr_id = libslab_gconf_notify_add (
		SEARCH_VIS_GCONF_KEY, lockdown_notify_cb, this);
	priv->status_vis_gconf_mntr_id = libslab_gconf_notify_add (
		STATUS_VIS_GCONF_KEY, lockdown_notify_cb, this);
	priv->system_vis_gconf_mntr_id = libslab_gconf_notify_add (
		SYSTEM_VIS_GCONF_KEY, lockdown_notify_cb, this);
	priv->showable_types_gconf_mntr_id = libslab_gconf_notify_add (
		SHOWABLE_TYPES_GCONF_KEY, lockdown_notify_cb, this);
	priv->modifiable_system_gconf_mntr_id = libslab_gconf_notify_add (
		MODIFIABLE_SYSTEM_GCONF_KEY, lockdown_notify_cb, this);
	priv->modifiable_apps_gconf_mntr_id = libslab_gconf_notify_add (
		MODIFIABLE_APPS_GCONF_KEY, lockdown_notify_cb, this);
	priv->modifiable_docs_gconf_mntr_id = libslab_gconf_notify_add (
		MODIFIABLE_DOCS_GCONF_KEY, lockdown_notify_cb, this);
	priv->modifiable_dirs_gconf_mntr_id = libslab_gconf_notify_add (
		MODIFIABLE_DIRS_GCONF_KEY, lockdown_notify_cb, this);
	priv->disable_term_gconf_mntr_id = libslab_gconf_notify_add (
		DISABLE_TERMINAL_GCONF_KEY, lockdown_notify_cb, this);
	priv->disable_logout_gconf_mntr_id = libslab_gconf_notify_add (
		DISABLE_LOGOUT_GCONF_KEY, lockdown_notify_cb, this);
	priv->disable_lockscreen_gconf_mntr_id = libslab_gconf_notify_add (
		DISABLE_LOCKSCREEN_GCONF_KEY, lockdown_notify_cb, this);
}

static Tile *
item_to_user_app_tile (BookmarkItem *item, gpointer data)
{
	if (app_is_in_blacklist (item->uri))
		return NULL;

	return TILE (application_tile_new (item->uri));
}

static Tile *
item_to_recent_app_tile (BookmarkItem *item, gpointer data)
{
	MainMenuUIPrivate *priv = PRIVATE (data);

	Tile *tile = NULL;

	gboolean blacklisted;


	blacklisted =
		bookmark_agent_has_item (priv->bm_agents [BOOKMARK_STORE_SYSTEM],    item->uri) ||
		bookmark_agent_has_item (priv->bm_agents [BOOKMARK_STORE_USER_APPS], item->uri) ||
		app_is_in_blacklist (item->uri);

	if (! blacklisted)
		tile = TILE (application_tile_new (item->uri));

	return tile;
}

static Tile *
item_to_user_doc_tile (BookmarkItem *item, gpointer data)
{
	return TILE (document_tile_new (item->uri, item->mime_type, item->mtime));
}

static Tile *
item_to_recent_doc_tile (BookmarkItem *item, gpointer data)
{
	MainMenuUIPrivate *priv = PRIVATE (data);

	GnomeVFSVolume *vol;
	gboolean        is_nfs = FALSE;
	GnomeVFSURI    *gvfs_uri;
	gboolean        is_local = TRUE;

	GList *node;


	if (! g_str_has_prefix (item->uri, "file://"))
		return NULL;

	for (node = priv->mounts; ! is_nfs && node; node = node->next) {
		vol = (GnomeVFSVolume *) node->data;

		is_nfs =
			((gnome_vfs_volume_get_device_type (vol) == GNOME_VFS_DEVICE_TYPE_NFS) &&
			g_str_has_prefix (item->uri, gnome_vfs_volume_get_activation_uri (vol)));
	}

	if (is_nfs)
		return NULL;

	gvfs_uri = gnome_vfs_uri_new (item->uri);
	is_local = gnome_vfs_uri_is_local (gvfs_uri);
	gnome_vfs_uri_unref (gvfs_uri);

	if (! is_local)
		return NULL;

	if (bookmark_agent_has_item (priv->bm_agents [BOOKMARK_STORE_USER_DOCS], item->uri))
		return NULL;

	return TILE (document_tile_new (item->uri, item->mime_type, item->mtime));
}

static Tile *
item_to_dir_tile (BookmarkItem *item, gpointer data)
{
	return TILE (directory_tile_new (item->uri, item->title, item->icon));
}

static Tile *
item_to_system_tile (BookmarkItem *item, gpointer data)
{
	if (app_is_in_blacklist (item->uri))
		return NULL;

	return TILE (system_tile_new (item->uri, item->title));
}

static BookmarkItem *
app_uri_to_item (const gchar *uri, gpointer data)
{
	BookmarkItem *item = g_new0 (BookmarkItem, 1);

	item->uri       = g_strdup (uri);
	item->mime_type = g_strdup ("application/x-desktop");

	return item;
}

static BookmarkItem *
doc_uri_to_item (const gchar *uri, gpointer data)
{
	BookmarkItem *item;

	GnomeVFSFileInfo        *info;
	GnomeVFSMimeApplication *default_app;


	item = g_new0 (BookmarkItem, 1);

	item->uri = g_strdup (uri);

	info = gnome_vfs_file_info_new ();

	gnome_vfs_get_file_info (
		item->uri, info, 
		GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);

	item->mime_type = g_strdup (info->mime_type);
	item->mtime     = info->mtime;

	if (item->mime_type) {
		default_app = gnome_vfs_mime_get_default_application (item->mime_type);

		item->app_name = g_strdup (gnome_vfs_mime_application_get_name (default_app));
		item->app_exec = g_strdup (gnome_vfs_mime_application_get_exec (default_app));

		gnome_vfs_mime_application_free (default_app);
	}

	gnome_vfs_file_info_unref (info);

	if (! (item->mime_type && item->app_name)) {
		bookmark_item_free (item);
		item = NULL;
	}

	return item;
}

static gboolean
app_is_in_blacklist (const gchar *uri)
{
	GList *blacklist;

	gboolean disable_term;
	gboolean disable_logout;
	gboolean disable_lockscreen;

	gboolean blacklisted;

	GList *node;


	disable_term = GPOINTER_TO_INT (libslab_get_gconf_value (DISABLE_TERMINAL_GCONF_KEY));
	blacklisted  = disable_term && libslab_desktop_item_is_a_terminal (uri);

	if (blacklisted)
		return TRUE;

	disable_logout = GPOINTER_TO_INT (libslab_get_gconf_value (DISABLE_LOGOUT_GCONF_KEY));
	blacklisted    = disable_logout && libslab_desktop_item_is_logout (uri);

	if (blacklisted)
		return TRUE;

	disable_lockscreen = GPOINTER_TO_INT (libslab_get_gconf_value (DISABLE_LOCKSCREEN_GCONF_KEY));
	blacklisted        = disable_lockscreen && libslab_desktop_item_is_lockscreen (uri);

	if (blacklisted)
		return TRUE;

	blacklist = libslab_get_gconf_value (APP_BLACKLIST_GCONF_KEY);

	for (node = blacklist; node; node = node->next) {
		if (! blacklisted && strstr (uri, (gchar *) node->data))
			blacklisted = TRUE;

		g_free (node->data);
	}

	g_list_free (blacklist);

	return blacklisted;
}

static void
select_page (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkToggleButton *button;
	gint curr_page;


	curr_page = GPOINTER_TO_INT (libslab_get_gconf_value (CURRENT_PAGE_GCONF_KEY));
	button    = GTK_TOGGLE_BUTTON (priv->page_selectors [curr_page]);

	gtk_toggle_button_set_active (button, TRUE);
}

static void
update_limits (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GObject   *user_tables   [2];
	GObject   *recent_tables [2];
	gint       n_user_bins   [2];
	GtkWidget *sections      [2];

	gint n_rows;
	gint n_cols;

	gint max_total_items_default;
	gint max_total_items_new;
	gint min_recent_items;

	gint i;


	user_tables [0] = G_OBJECT (priv->file_tables [USER_APPS_TABLE]);
	user_tables [1] = G_OBJECT (priv->file_tables [USER_DOCS_TABLE]);

	recent_tables [0] = G_OBJECT (priv->file_tables [RCNT_APPS_TABLE]);
	recent_tables [1] = G_OBJECT (priv->file_tables [RCNT_DOCS_TABLE]);

	sections [0] = priv->table_sections [USER_APPS_TABLE];
	sections [1] = priv->table_sections [USER_DOCS_TABLE];

/* TODO: make this instant apply */

	max_total_items_default = GPOINTER_TO_INT (
		libslab_get_gconf_value (MAX_TOTAL_ITEMS_GCONF_KEY));
	min_recent_items = GPOINTER_TO_INT (
		libslab_get_gconf_value (MIN_RECENT_ITEMS_GCONF_KEY));

	priv->max_total_items = max_total_items_default;

	for (i = 0; i < 2; ++i) {
		if (GTK_WIDGET_VISIBLE (sections [i]))
			g_object_get (
				user_tables [i],
				"n-rows", & n_rows, "n-columns", & n_cols, NULL);
		else
			n_rows = n_cols = 0;

		n_user_bins [i] = n_cols * n_rows;

		max_total_items_new = n_user_bins [i] + min_recent_items;

		if (priv->max_total_items < max_total_items_new)
			priv->max_total_items = max_total_items_new;
	}

	g_object_get (
		priv->file_tables [USER_DIRS_TABLE],
		"n-rows", & n_rows, "n-columns", & n_cols, NULL);

	if (priv->max_total_items < (n_rows * n_cols))
		priv->max_total_items = n_rows * n_cols;

	for (i = 0; i < 2; ++i)
		g_object_set (
			recent_tables [i],
			TILE_TABLE_LIMIT_PROP, priv->max_total_items - n_user_bins [i],
			NULL);
}

static void
connect_to_tile_triggers (MainMenuUI *this, TileTable *table)
{
	GList *tiles;
	GList *node;

	gulong handler_id;

	gint icon_width;


	g_object_get (G_OBJECT (table), TILE_TABLE_TILES_PROP, & tiles, NULL);

	for (node = tiles; node; node = node->next) {
		handler_id = g_signal_handler_find (
			G_OBJECT (node->data), G_SIGNAL_MATCH_FUNC, 0, 0,
			NULL, tile_action_triggered_cb, NULL);

		if (! handler_id)
			g_signal_connect (
				G_OBJECT (node->data), "tile-action-triggered",
				G_CALLBACK (tile_action_triggered_cb), this);


		gtk_icon_size_lookup (GTK_ICON_SIZE_DND, & icon_width, NULL);
		gtk_widget_set_size_request (GTK_WIDGET (node->data), 6 * icon_width, -1);
	}
}

static void
hide_slab_if_urgent_close (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	if (! GPOINTER_TO_INT (libslab_get_gconf_value (URGENT_CLOSE_GCONF_KEY)))
		return;

	gtk_toggle_button_set_active (priv->panel_button, FALSE);
}

static void
set_search_section_visible (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gboolean allowable;
	gboolean visible;

	gchar **argv;
	gchar  *found_cmd = NULL;


	allowable = GPOINTER_TO_INT (libslab_get_gconf_value (SEARCH_VIS_GCONF_KEY));

	if (allowable) {
		argv = get_search_argv (NULL);
		found_cmd = g_find_program_in_path (argv [0]);

		visible = (found_cmd != NULL);

		g_strfreev (argv);
		g_free (found_cmd);
	}
	else
		visible = FALSE;

	if (visible)
		gtk_widget_show (priv->search_section);
	else
		gtk_widget_hide (priv->search_section);
}

static void
set_table_section_visible (MainMenuUI *this, TileTable *table)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gint   table_id;
	GList *tiles;


	table_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (table), "table-id"));
	g_object_get (G_OBJECT (table), TILE_TABLE_TILES_PROP, & tiles, NULL);

	if (priv->allowable_types [table_id] && g_list_length (tiles) > 0)
		gtk_widget_show (priv->table_sections [table_id]);
	else
		gtk_widget_hide (priv->table_sections [table_id]);

	if (
		GTK_WIDGET_VISIBLE (priv->table_sections [USER_APPS_TABLE]) ||
		GTK_WIDGET_VISIBLE (priv->table_sections [RCNT_APPS_TABLE])
	)
		gtk_widget_show (priv->page_selectors [APPS_PAGE]);
	else
		gtk_widget_hide (priv->page_selectors [APPS_PAGE]);

	if (
		GTK_WIDGET_VISIBLE (priv->table_sections [USER_DOCS_TABLE]) ||
		GTK_WIDGET_VISIBLE (priv->table_sections [RCNT_DOCS_TABLE])
	)
		gtk_widget_show (priv->page_selectors [DOCS_PAGE]);
	else
		gtk_widget_hide (priv->page_selectors [DOCS_PAGE]);

	if (GTK_WIDGET_VISIBLE (priv->table_sections [USER_DIRS_TABLE]))
		gtk_widget_show (priv->page_selectors [DIRS_PAGE]);
	else
		gtk_widget_hide (priv->page_selectors [DIRS_PAGE]);
}

static gchar **
get_search_argv (const gchar *search_txt)
{
	gchar  *cmd;
	gint    argc;
	gchar **argv_parsed = NULL;

	gchar **argv = NULL;

	gint i;


	cmd = (gchar *) libslab_get_gconf_value (SEARCH_CMD_GCONF_KEY);

	if (! cmd) {
		g_warning ("could not find search command in gconf [" SEARCH_CMD_GCONF_KEY "]\n");

		return NULL;
	}

	if (! g_shell_parse_argv (cmd, & argc, & argv_parsed, NULL))
		goto exit;

	argv = g_new0 (gchar *, argc + 1);

	for (i = 0; i < argc; ++i) {
		if (! strcmp (argv_parsed [i], "SEARCH_STRING"))
			argv [i] = g_strdup ((search_txt == NULL) ? "" : search_txt);
		else
			argv [i] = g_strdup (argv_parsed [i]);
	}

	argv [argc] = NULL;

exit:

	g_free (cmd);
	g_strfreev (argv_parsed);

	return argv;
}

static void
reorient_panel_button (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	PanelAppletOrient orientation;

	GtkWidget *child;


	orientation = panel_applet_get_orient (priv->panel_applet);

	child = gtk_bin_get_child (GTK_BIN (priv->panel_applet));

	if (GTK_IS_WIDGET (child))
		gtk_container_remove (GTK_CONTAINER (priv->panel_applet), child);

	switch (orientation) {
		case PANEL_APPLET_ORIENT_LEFT:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT];
			break;

		case PANEL_APPLET_ORIENT_RIGHT:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT];
			break;

		case PANEL_APPLET_ORIENT_UP:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_BOTTOM];
			break;

		default:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP];
			break;
	}

	gtk_container_add (GTK_CONTAINER (priv->panel_applet), GTK_WIDGET (priv->panel_button));
}

static void
launch_search (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	const gchar *search_txt;

	gchar **argv;
	gchar  *cmd;

	GError *error = NULL;


	search_txt = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));

	argv = get_search_argv (search_txt);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, & error);

	if (error) {
		cmd = g_strjoinv (" ", argv);
		libslab_handle_g_error (
			& error, "%s: can't execute search [%s]\n", G_STRFUNC, cmd);
		g_free (cmd);
	}

	g_strfreev (argv);

	hide_slab_if_urgent_close (this);

	gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
}

static void
grab_pointer_and_keyboard (MainMenuUI *this, guint32 time)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GdkGrabStatus status;


	if (time == 0)
		time = GDK_CURRENT_TIME;

	if (GPOINTER_TO_INT (libslab_get_gconf_value (URGENT_CLOSE_GCONF_KEY))) {
		gtk_widget_grab_focus (priv->slab_window);
		gtk_grab_add          (priv->slab_window);

		status = gdk_pointer_grab (
			priv->slab_window->window, TRUE, GDK_BUTTON_PRESS_MASK,
			NULL, NULL, time);

		priv->ptr_is_grabbed = (status == GDK_GRAB_SUCCESS);

		status = gdk_keyboard_grab (priv->slab_window->window, TRUE, time);

		priv->kbd_is_grabbed = (status == GDK_GRAB_SUCCESS);
	}
	else {
		if (priv->ptr_is_grabbed) {
			gdk_pointer_ungrab (time);
			priv->ptr_is_grabbed = FALSE;
		}

		if (priv->kbd_is_grabbed) {
			gdk_keyboard_ungrab (time);
			priv->kbd_is_grabbed = FALSE;
		}

		gtk_grab_remove (priv->slab_window);
	}
}

static void
apply_lockdown_settings (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gboolean  more_link_visible;
	gboolean  status_area_visible;
	gboolean  system_area_visible;
	GList    *showable_types;

	GList *node;
	gint   i;


	more_link_visible   = GPOINTER_TO_INT (libslab_get_gconf_value (MORE_LINK_VIS_GCONF_KEY));
	status_area_visible = GPOINTER_TO_INT (libslab_get_gconf_value (STATUS_VIS_GCONF_KEY));
	system_area_visible = GPOINTER_TO_INT (libslab_get_gconf_value (SYSTEM_VIS_GCONF_KEY));
	showable_types      = (GList *) libslab_get_gconf_value (SHOWABLE_TYPES_GCONF_KEY);

	for (i = 0; i < 3; ++i)
		if (more_link_visible)
			gtk_widget_show (priv->more_sections [i]);
		else
			gtk_widget_hide (priv->more_sections [i]);

	set_search_section_visible (this);

	if (status_area_visible)
		gtk_widget_show (priv->status_section);
	else
		gtk_widget_hide (priv->status_section);

	if (system_area_visible)
		gtk_widget_show (priv->system_section);
	else
		gtk_widget_hide (priv->system_section);

	for (i = 0; i < 5; ++i)
		priv->allowable_types [i] = FALSE;

	for (node = showable_types; node; node = node->next) {
		i = GPOINTER_TO_INT (node->data);

		if (0 <= i && i < 5)
			priv->allowable_types [i] = TRUE;
	}

	g_list_free (showable_types);

	for (i = 0; i < 5; ++i)
		set_table_section_visible (this, priv->file_tables [i]);

	tile_table_reload (priv->sys_table);
	tile_table_reload (priv->file_tables [USER_APPS_TABLE]);
	tile_table_reload (priv->file_tables [RCNT_APPS_TABLE]);
	tile_table_reload (priv->file_tables [USER_DOCS_TABLE]);
	tile_table_reload (priv->file_tables [USER_DIRS_TABLE]);

	update_limits (this);
}

static void
bind_beagle_search_key (MainMenuUI *this)
{
	xmlDocPtr  doc;
	xmlNodePtr node;

	gchar    *path;
	gchar    *contents;
	gsize     length;
	gboolean  success;

	gchar *key;

	gboolean ctrl;
	gboolean alt;

	xmlChar *val;


	path = g_build_filename (
		g_get_home_dir (), ".beagle/config/searching.xml", NULL);
	success = g_file_get_contents (path, & contents, & length, NULL);
	g_free (path);

	if (! success)
		return;

	doc = xmlParseMemory (contents, length);
	g_free (contents);

	if (! doc)
		return;

	if (! doc->children || ! doc->children->children)
		goto exit;

	for (node = doc->children->children; node; node = node->next) {
		if (! node->name || strcmp ((gchar *) node->name, "ShowSearchWindowBinding"))
			continue;

		if (! node->children)
			break;

		val  = xmlGetProp (node, (xmlChar *) "Ctrl");
		ctrl = val && ! strcmp ((gchar *) val, "true");
		xmlFree (val);

		val = xmlGetProp (node, (xmlChar *) "Alt");
		alt = val && ! strcmp ((gchar *) val, "true");
		xmlFree (val);

		for (node = node->children; node; node = node->next) {
			if (! node->name || strcmp ((gchar *) node->name, "Key"))
				continue;

			val = xmlNodeGetContent (node);

			key = g_strdup_printf (
				"%s%s%s", ctrl ? "<Ctrl>" : "", alt ? "<Alt>" : "", val);
			xmlFree (val);

			tomboy_keybinder_bind (key, search_tomboy_bindkey_cb, this);

			g_free (key);

			break;
		}

		break;
	}

exit:

	xmlFreeDoc (doc);
}

static void
panel_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GtkToggleButton *toggle = GTK_TOGGLE_BUTTON (button);

	DoubleClickDetector *detector;
	GTimeVal t_curr;
	guint32  t_curr_ms;

	gboolean visible;


	detector = DOUBLE_CLICK_DETECTOR (
		g_object_get_data (G_OBJECT (toggle), "double-click-detector"));

	g_get_current_time (& t_curr);
	t_curr_ms = 1000 * t_curr.tv_sec + t_curr.tv_usec / 1000;

	visible = GTK_WIDGET_VISIBLE (priv->slab_window);

	if (! double_click_detector_is_double_click (detector, t_curr_ms, TRUE)) {
		if (! visible)
			gtk_window_present_with_time (GTK_WINDOW (priv->slab_window), t_curr_ms);
		else
			gtk_widget_hide (priv->slab_window);

		visible = GTK_WIDGET_VISIBLE (priv->slab_window);
	}

	gtk_toggle_button_set_active (priv->panel_button, visible);
}

static gboolean
panel_button_button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	if (event->button != 1)
		g_signal_stop_emission_by_name (widget, "button_press_event");
	else if (! gtk_toggle_button_get_active (priv->panel_button))
		gtk_toggle_button_set_active (priv->panel_button, TRUE);
	else
		/* do nothing */ ;

	return FALSE;
}

static void
panel_button_drag_data_rcv_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                               GtkSelectionData *selection, guint info, guint time,
                               gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	gchar **uris;
	gint    uri_len;

	gint i;


	if (gtk_drag_get_source_widget (context))
		return;

	if (! selection)
		return;

	uris = gtk_selection_data_get_uris (selection);

	if (! uris)
		return;

	for (i = 0; uris [i]; ++i) {
		if (strncmp (uris [i], "file://", 7))
			continue;

		uri_len = strlen (uris [i]);

		if (! strcmp (& uris [i] [uri_len - 8], ".desktop"))
			tile_table_add_uri (priv->file_tables [USER_APPS_TABLE], uris [i]);
		else
			tile_table_add_uri (priv->file_tables [USER_DOCS_TABLE], uris [i]);
	}

	g_strfreev (uris);
}

static gboolean
slab_window_expose_cb (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	cairo_t         *cr;
	cairo_pattern_t *gradient;


	cr = gdk_cairo_create (widget->window);

	cairo_rectangle (
		cr,
		event->area.x, event->area.y,
		event->area.width, event->area.height);

	cairo_clip (cr);

/* draw window background */

	cairo_rectangle (
		cr, 
		widget->allocation.x + 0.5, widget->allocation.y + 0.5,
		widget->allocation.width - 1, widget->allocation.height - 1);

	cairo_set_source_rgb (
		cr,
		widget->style->bg [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->bg [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->bg [GTK_STATE_ACTIVE].blue  / 65535.0);

	cairo_fill_preserve (cr);

/* draw window outline */

	cairo_set_source_rgb (
		cr,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0);

	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);

/* draw left pane background */

	cairo_rectangle (
		cr,
		priv->left_pane->allocation.x + 0.5, priv->left_pane->allocation.y + 0.5,
		priv->left_pane->allocation.width - 1, priv->left_pane->allocation.height - 1);

	cairo_set_source_rgb (
		cr,
		widget->style->bg [GTK_STATE_NORMAL].red   / 65535.0,
		widget->style->bg [GTK_STATE_NORMAL].green / 65535.0,
		widget->style->bg [GTK_STATE_NORMAL].blue  / 65535.0);

	cairo_fill_preserve (cr);

/* draw left pane outline */

	cairo_set_source_rgb (
		cr,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0);

	cairo_stroke (cr);

/* draw top pane separator */

	cairo_move_to (
		cr,
		priv->top_pane->allocation.x + 0.5,
		priv->top_pane->allocation.y + priv->top_pane->allocation.height - 0.5);

	cairo_line_to (
		cr,
		priv->top_pane->allocation.x + priv->top_pane->allocation.width - 0.5,
		priv->top_pane->allocation.y + priv->top_pane->allocation.height - 0.5);

	cairo_set_source_rgb (
		cr,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0);

	cairo_stroke (cr);

/* draw top pane gradient */

	cairo_rectangle (
		cr,
		priv->top_pane->allocation.x + 0.5, priv->top_pane->allocation.y + 0.5,
		priv->top_pane->allocation.width - 1, priv->top_pane->allocation.height - 1);

	gradient = cairo_pattern_create_linear (
		priv->top_pane->allocation.x,
		priv->top_pane->allocation.y,
		priv->top_pane->allocation.x,
		priv->top_pane->allocation.y + priv->top_pane->allocation.height);
	cairo_pattern_add_color_stop_rgba (
		gradient, 0,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0,
		0.0);
	cairo_pattern_add_color_stop_rgba (
		gradient, 1,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0,
		0.2);

	cairo_set_source (cr, gradient);
	cairo_fill_preserve (cr);

	cairo_pattern_destroy (gradient);
	cairo_destroy (cr);

	return FALSE;
}

static gboolean
slab_window_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	switch (event->keyval) {
		case GDK_Super_L:
		case GDK_Escape:
			gtk_toggle_button_set_active (priv->panel_button, FALSE);

			return TRUE;

		case GDK_W:
		case GDK_w:
			if (event->state & GDK_CONTROL_MASK) {
				gtk_toggle_button_set_active (priv->panel_button, FALSE);

				return TRUE;
			}

		default:
			return FALSE;
	}
}

static gboolean
slab_window_button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GdkWindow *ptr_window;
	
	
	ptr_window = gdk_window_at_pointer (NULL, NULL);

	if (priv->slab_window->window != ptr_window) {
		hide_slab_if_urgent_close (this);

		return TRUE;
	}

	return FALSE;
}

static void
slab_window_allocate_cb (GtkWidget *widget, GtkAllocation *alloc, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GdkScreen *panel_button_screen;

	GdkRectangle button_geom;
	GdkRectangle slab_geom;
	GdkRectangle monitor_geom;

	PanelAppletOrient orient;


	gdk_window_get_origin (GTK_WIDGET (priv->panel_button)->window, & button_geom.x, & button_geom.y);
	button_geom.width  = GTK_WIDGET (priv->panel_button)->allocation.width;
	button_geom.height = GTK_WIDGET (priv->panel_button)->allocation.height;

	slab_geom.width  = priv->slab_window->allocation.width;
	slab_geom.height = priv->slab_window->allocation.height;

	panel_button_screen = gtk_widget_get_screen (GTK_WIDGET (priv->panel_button));

	gdk_screen_get_monitor_geometry (
		panel_button_screen,
		gdk_screen_get_monitor_at_window (
			panel_button_screen, GTK_WIDGET (priv->panel_button)->window),
		& monitor_geom);

	orient = panel_applet_get_orient (priv->panel_applet);

	switch (orient) {
		case PANEL_APPLET_ORIENT_UP:
			slab_geom.x = button_geom.x;
			slab_geom.y = button_geom.y - slab_geom.height;
			break;

		case PANEL_APPLET_ORIENT_DOWN:
			slab_geom.x = button_geom.x;
			slab_geom.y = button_geom.y + button_geom.height;
			break;

		case PANEL_APPLET_ORIENT_RIGHT:
			slab_geom.x = button_geom.x + button_geom.width;
			slab_geom.y = button_geom.y;
			break;

		case PANEL_APPLET_ORIENT_LEFT:
			slab_geom.x = button_geom.x - slab_geom.width;
			slab_geom.y = button_geom.y;
			break;

		default:
			slab_geom.x = 0;
			slab_geom.y = 0;
			break;
	}

	if (orient == PANEL_APPLET_ORIENT_UP || orient == PANEL_APPLET_ORIENT_DOWN) {
		if ((slab_geom.x + slab_geom.width) > (monitor_geom.x + monitor_geom.width))
			slab_geom.x = button_geom.x + button_geom.width - slab_geom.width;
	}
	else {
		if ((slab_geom.y + slab_geom.height) > (monitor_geom.y + monitor_geom.height))
			slab_geom.y = button_geom.y + button_geom.height - slab_geom.height;
	}

	gtk_window_move (GTK_WINDOW (priv->slab_window), slab_geom.x, slab_geom.y);
}

static void
slab_window_map_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	grab_pointer_and_keyboard (MAIN_MENU_UI (user_data), gdk_event_get_time (event));
}

static void
slab_window_unmap_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	if (priv->ptr_is_grabbed) {
		gdk_pointer_ungrab (gdk_event_get_time (event));
		priv->ptr_is_grabbed = FALSE;
	}

	if (priv->kbd_is_grabbed) {
		gdk_keyboard_ungrab (gdk_event_get_time (event));
		priv->kbd_is_grabbed = FALSE;
	}

	gtk_grab_remove (widget);
}

static gboolean
slab_window_grab_broken_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GdkEventGrabBroken *grab_event = (GdkEventGrabBroken *) event;
	gpointer window_data;


	if (grab_event->grab_window) {
		gdk_window_get_user_data (grab_event->grab_window, & window_data);

		if (GTK_IS_WIDGET (window_data))
			g_signal_connect (
				G_OBJECT (window_data), "event",
				G_CALLBACK (grabbing_window_event_cb), user_data);
	}

	return FALSE;
}

static void
search_entry_activate_cb (GtkEntry *entry, gpointer user_data)
{
	const gchar *entry_text = gtk_entry_get_text (entry);

	if (entry_text && strlen (entry_text) >= 1)
		launch_search (MAIN_MENU_UI (user_data));
}

static void
page_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	gint page_type;


	page_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "page-type"));

	gtk_notebook_set_current_page (priv->file_section, priv->notebook_page_ids [page_type]);

	libslab_set_gconf_value (CURRENT_PAGE_GCONF_KEY, GINT_TO_POINTER (page_type));
}

static void
tile_table_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	gint table_id;


	connect_to_tile_triggers (this, TILE_TABLE (g_obj));

	table_id = GPOINTER_TO_INT (g_object_get_data (g_obj, "table-id"));

	switch (table_id) {
		case USER_APPS_TABLE:
			tile_table_reload (priv->file_tables [RCNT_APPS_TABLE]);
			break;

		case USER_DOCS_TABLE:
			tile_table_reload (priv->file_tables [RCNT_DOCS_TABLE]);
			break;

		default:
			break;
	}

	set_table_section_visible (this, TILE_TABLE (g_obj));

	update_limits (this);
}

static void
gtk_table_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	gint table_id = GPOINTER_TO_INT (g_object_get_data (g_obj, "table-id"));

	if (table_id == USER_APPS_TABLE || table_id == USER_DOCS_TABLE)
		update_limits (MAIN_MENU_UI (user_data));
}

static void
tile_action_triggered_cb (Tile *tile, TileEvent *event, TileAction *action, gpointer user_data)
{
	if (! TILE_ACTION_CHECK_FLAG (action, TILE_ACTION_OPENS_NEW_WINDOW))
		return;

	hide_slab_if_urgent_close (MAIN_MENU_UI (user_data));
}

static void
more_buttons_clicked_cb (GtkButton *button, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	DoubleClickDetector *detector;
	GTimeVal current_time;
	guint32 current_time_millis;

	GnomeDesktopItem *ditem;
	gchar            *ditem_id;

	gchar *cmd_template;
	gchar *cmd;
	gchar *dir;
	gchar *uri;


	detector = DOUBLE_CLICK_DETECTOR (
		g_object_get_data (G_OBJECT (button), "double-click-detector"));

	g_get_current_time (& current_time);

	current_time_millis = 1000 * current_time.tv_sec + current_time.tv_usec / 1000;

	if (! double_click_detector_is_double_click (detector, current_time_millis, TRUE)) {
		if (GTK_WIDGET (button) == priv->more_buttons [APPS_PAGE])
			ditem_id = libslab_get_gconf_value (APP_BROWSER_GCONF_KEY);
		else if (GTK_WIDGET (button) == priv->more_buttons [DOCS_PAGE]) {
			dir = g_build_filename (g_get_home_dir (), "Documents", NULL);

			if (! g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
				dir = g_strdup (g_get_home_dir ());

			uri = g_filename_to_uri (dir, NULL, NULL);

			cmd_template = (gchar *) libslab_get_gconf_value (FILE_MGR_OPEN_GCONF_KEY);
			cmd = libslab_string_replace_once (cmd_template, "FILE_URI", uri);

			libslab_spawn_command (cmd);

			g_free (cmd);
			g_free (cmd_template);
			g_free (uri);
			g_free (dir);

			ditem_id = NULL;

			hide_slab_if_urgent_close (this);
		}
		else
			ditem_id = libslab_get_gconf_value (FILE_BROWSER_GCONF_KEY);

		ditem = libslab_gnome_desktop_item_new_from_unknown_id (ditem_id);

		if (ditem) {
			libslab_gnome_desktop_item_launch_default (ditem);

			hide_slab_if_urgent_close (this);
		}
	}
}

static void
search_cmd_notify_cb (GConfClient *client, guint conn_id,
                      GConfEntry *entry, gpointer user_data)
{
	set_search_section_visible (MAIN_MENU_UI (user_data));
}

static void
current_page_notify_cb (GConfClient *client, guint conn_id,
                        GConfEntry *entry, gpointer user_data)
{
	select_page (MAIN_MENU_UI (user_data));
}

static void
lockdown_notify_cb (GConfClient *client, guint conn_id,
                    GConfEntry *entry, gpointer user_data)
{
	apply_lockdown_settings (MAIN_MENU_UI (user_data));
}

static void
panel_menu_open_cb (BonoboUIComponent *component, gpointer user_data, const gchar *verb)
{
	gtk_toggle_button_set_active (PRIVATE (user_data)->panel_button, TRUE);
}

static void
panel_menu_about_cb (BonoboUIComponent *component, gpointer user_data, const gchar *verb)
{
	MainMenuUI *this        = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);


	if (! priv->panel_about_dialog) {
		priv->panel_about_dialog = gtk_about_dialog_new ();

		g_object_set (priv->panel_about_dialog,
			"name", _("GNOME Main Menu"),
			"comments", _("The GNOME Main Menu"),
			"version", VERSION,
			"authors", main_menu_authors,
			"artists", main_menu_artists,
			"logo-icon-name", "gnome-fs-client",
			"copyright", "Copyright \xc2\xa9 2005-2007 Novell, Inc.",
			NULL);

		gtk_widget_show (priv->panel_about_dialog);

		g_signal_connect (G_OBJECT (priv->panel_about_dialog), "response",
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	}

	gtk_window_present (GTK_WINDOW (priv->panel_about_dialog));
}

static void
panel_applet_change_orient_cb (PanelApplet *applet, PanelAppletOrient orient, gpointer user_data)
{
	reorient_panel_button (MAIN_MENU_UI (user_data));
}

static void
panel_applet_change_background_cb (PanelApplet *applet, PanelAppletBackgroundType type, GdkColor *color,
                                   GdkPixmap *pixmap, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GtkRcStyle *rc_style;
	GtkStyle   *style;


/* reset style */

	gtk_widget_set_style (GTK_WIDGET (priv->panel_applet), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (priv->panel_applet), rc_style);
	gtk_rc_style_unref (rc_style);

	switch (type) {
		case PANEL_NO_BACKGROUND:
			break;

		case PANEL_COLOR_BACKGROUND:
			gtk_widget_modify_bg (
				GTK_WIDGET (priv->panel_applet), GTK_STATE_NORMAL, color);

			break;

		case PANEL_PIXMAP_BACKGROUND:
			style = gtk_style_copy (GTK_WIDGET (priv->panel_applet)->style);

			if (style->bg_pixmap [GTK_STATE_NORMAL])
				g_object_unref (style->bg_pixmap [GTK_STATE_NORMAL]);

			style->bg_pixmap [GTK_STATE_NORMAL] = g_object_ref (pixmap);

			gtk_widget_set_style (GTK_WIDGET (priv->panel_applet), style);

			g_object_unref (style);

			break;
	}
}

static void
slab_window_tomboy_bindkey_cb (gchar *key_string, gpointer user_data)
{
	gtk_toggle_button_set_active (PRIVATE (user_data)->panel_button, TRUE);
}

static void
search_tomboy_bindkey_cb (gchar *key_string, gpointer user_data)
{
	launch_search (MAIN_MENU_UI (user_data));
}

static gboolean
grabbing_window_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if (event->type == GDK_UNMAP || event->type == GDK_SELECTION_CLEAR)
		grab_pointer_and_keyboard (MAIN_MENU_UI (user_data), gdk_event_get_time (event));

	return FALSE;
}

static GdkFilterReturn
slab_gdk_message_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	XEvent *xevent = (XEvent *) gdk_xevent;
	gboolean active;


	if (xevent->type != ClientMessage)
		return GDK_FILTER_CONTINUE;

	if (xevent->xclient.data.l [0] == slab_action_main_menu_atom) {
		active = gtk_toggle_button_get_active (priv->panel_button);

		gtk_toggle_button_set_active (priv->panel_button, ! active);
	}
	else
		return GDK_FILTER_CONTINUE;

	return GDK_FILTER_REMOVE;
}

static void
user_app_agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	tile_table_reload (PRIVATE (user_data)->file_tables [RCNT_APPS_TABLE]);
}

static void
user_doc_agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	tile_table_reload (PRIVATE (user_data)->file_tables [RCNT_DOCS_TABLE]);
}

static void
volume_monitor_mount_cb (GnomeVFSVolumeMonitor *mon, GnomeVFSVolume *vol, gpointer data)
{
	MainMenuUIPrivate *priv = PRIVATE (data);

	g_list_foreach (priv->mounts, (GFunc) gnome_vfs_volume_unref, NULL);
	g_list_free (priv->mounts);
	priv->mounts = gnome_vfs_volume_monitor_get_mounted_volumes (mon);
}
