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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "main-menu-common.h"

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <X11/Xlib.h>

#include "application-tile.h"
#include "nameplate-tile.h"
#include "system-tile.h"
#include "document-tile.h"

#include "egg-recent-item.h"

#include "double-click-detector.h"
#include "slab-gnome-util.h"
#include "gnome-utils.h"

#include "hard-drive-status-tile.h"
#include "network-status-tile.h"
#include "slab-window.h"
#include "tomboykeybinder.h"

#include "file-area-widget.h"

#define N_FILE_AREA_COLS             2
#define FILE_AREA_TABLE_COL_SPACINGS 6
#define FILE_AREA_TABLE_ROW_SPACINGS 6
#define N_FILE_CLASS                 3
#define TILE_WIDTH_SCALE             6

#define SYSTEM_TILE_SPACING 0

#define DISABLE_LOCK_SCREEN_GCONF_KEY  "/apps/panel/global/disable_lock_screen"
#define DISABLE_LOG_OUT_GCONF_KEY      "/apps/panel/global/disable_log_out"

static const gchar *main_menu_authors[] = {
	"Rodney Dawes <dobey@novell.com>",
	"Jim Krehl <jimmyk@novell.com>",
	"Scott Reeves <sreeves@novell.com>",
	"Dan Winship <danw@novell.com>",
	NULL
};

static const gchar *main_menu_artists[] = {
	"Garrett LeSage <garrett@novell.com>",
	"Jakub Steiner <jimmac@novell.com>",
	NULL
};

static void main_menu_ui_class_init (MainMenuUIClass *);
static void main_menu_ui_init (MainMenuUI *);
static void main_menu_ui_dispose (GObject *);

static void bonobo_menu_menu_open_cb (BonoboUIComponent *, gpointer, const gchar *);
static void bonobo_menu_menu_about_cb (BonoboUIComponent *, gpointer, const gchar *);
static void applet_change_orient_cb (PanelApplet *, PanelAppletOrient, gpointer);
static void applet_change_background_cb (PanelApplet *, PanelAppletBackgroundType, GdkColor * color,
	GdkPixmap * pixmap, gpointer);

static void tomboy_bindkey_cb (gchar *, gpointer);

static void build_main_menu_window (MainMenuUI *);
static void build_panel_button (MainMenuUI *);

static void reorient_panel_button (MainMenuUI *);

static void hide_main_window (MainMenuUI *);
static void show_main_window (MainMenuUI *, guint32);
static void position_main_window (MainMenuUI *);

static gboolean main_window_key_press_cb (GtkWidget *, GdkEventKey *, gpointer);
static gboolean main_window_button_press_cb (GtkWidget *, GdkEventButton *, gpointer);
static void main_window_map_cb (GtkWidget *, GdkEvent *, gpointer);
static void main_window_unmap_cb (GtkWidget *, GdkEvent *, gpointer);
static void main_window_resize_cb (GtkWidget *, GtkAllocation *, gpointer);
static gboolean main_window_grab_broken_cb (GtkWidget *, GdkEvent *, gpointer);
static gboolean grabbing_window_event_cb (GtkWidget *, GdkEvent *, gpointer);

static void tile_drag_data_rcv_cb (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *,
	guint, guint, gpointer);

static gboolean panel_button_button_press_cb (GtkWidget *, GdkEventButton *, gpointer);
static gboolean panel_button_enter_notify_cb (GtkWidget *, GdkEventButton *, gpointer);

static GtkWidget *create_search_widget (MainMenuUI *, GtkSizeGroup *);

static GtkWidget *create_system_table_widget (MainMenuUI *, GtkSizeGroup *, GtkSizeGroup *);

static void bind_search_key (void);

static void grab_focus (MainMenuUI *, GdkEvent *);

static GtkWidget *get_section_header_label (const gchar *);
static void section_header_label_style_set (GtkWidget *, GtkStyle *, gpointer);

static void search_entry_activate_cb (GtkEntry *, gpointer);

static void tile_action_triggered_cb (Tile *, TileEvent *, TileAction *, gpointer);

typedef struct
{
	PanelApplet *applet;
	MainMenuConf *conf;
	MainMenuEngine *engine;

	GtkWidget *about_dialog;

	GtkWidget *main_window;
	GtkWidget *panel_button;

	GtkWidget *search_section;
	GtkEntry *search_entry;

	FileAreaWidget *file_area;

	gboolean ptr_is_grabbed;
	gboolean kbd_is_grabbed;
} MainMenuUIPrivate;

#define MAIN_MENU_UI_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_UI_TYPE, MainMenuUIPrivate))

static const BonoboUIVerb applet_bonobo_verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("MainMenuOpen", bonobo_menu_menu_open_cb),
	BONOBO_UI_UNSAFE_VERB ("MainMenuAbout", bonobo_menu_menu_about_cb),
	BONOBO_UI_VERB_END
};

static Atom atom_slab_main_menu_action = None;

GType
main_menu_ui_get_type (void)
{
	static GType object_type = 0;

	if (!object_type)
	{
		static const GTypeInfo object_info = {
			sizeof (MainMenuUIClass),
			NULL,
			NULL,
			(GClassInitFunc) main_menu_ui_class_init,
			NULL,
			NULL,
			sizeof (MainMenuUI),
			0,
			(GInstanceInitFunc) main_menu_ui_init
		};

		object_type = g_type_register_static (G_TYPE_OBJECT, "MainMenuUI", &object_info, 0);
	}

	return object_type;
}

static void
main_menu_ui_class_init (MainMenuUIClass * main_menu_ui_class)
{
	GObjectClass *g_obj_class = (GObjectClass *) main_menu_ui_class;

	g_obj_class->dispose = main_menu_ui_dispose;

	g_type_class_add_private (main_menu_ui_class, sizeof (MainMenuUIPrivate));
}

static void
main_menu_ui_init (MainMenuUI * main_menu_ui)
{
}

static GdkFilterReturn
slab_action_filter (GdkXEvent * gdk_xevent, GdkEvent * event, gpointer user_data)
{
	MainMenuUI *ui = (MainMenuUI *) user_data;
	XEvent *xevent = (XEvent *) gdk_xevent;

	if (xevent->type != ClientMessage)
		return GDK_FILTER_CONTINUE;

	if (xevent->xclient.data.l[0] == atom_slab_main_menu_action)
		show_main_window (ui, GDK_CURRENT_TIME);
	else
		return GDK_FILTER_CONTINUE;

	return GDK_FILTER_REMOVE;
}

MainMenuUI *
main_menu_ui_new (PanelApplet * applet, MainMenuConf * conf, MainMenuEngine * engine)
{
	MainMenuUI *ui;
	MainMenuUIPrivate *priv;
	GdkAtom gdk_atom_slab_action;

	ui = g_object_new (MAIN_MENU_UI_TYPE, NULL);
	priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	priv->applet = applet;
	priv->conf = conf;
	priv->engine = engine;

	build_main_menu_window (ui);
	build_panel_button (ui);

	g_signal_connect (G_OBJECT (priv->applet), "change_orient",
		G_CALLBACK (applet_change_orient_cb), ui);

	g_signal_connect (G_OBJECT (priv->applet), "change_background",
		G_CALLBACK (applet_change_background_cb), ui);

	tomboy_keybinder_init ();
	tomboy_keybinder_bind ("<Ctrl>Escape", tomboy_bindkey_cb, ui);
	bind_search_key ();

	/* install message client handler to get messages through the X root window */
	gdk_atom_slab_action = gdk_atom_intern ("_SLAB_ACTION", FALSE);
	atom_slab_main_menu_action =
		XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
		"_SLAB_ACTION_MAIN_MENU", FALSE);
	gdk_display_add_client_message_filter (gdk_display_get_default (), gdk_atom_slab_action,
		slab_action_filter, ui);

	panel_applet_setup_menu_from_file (priv->applet, NULL, "GNOME_MainMenu_ContextMenu.xml",
		NULL, applet_bonobo_verbs, ui);

	return ui;
}

static void
main_menu_ui_dispose (GObject * obj)
{
}

void
main_menu_ui_release (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	gtk_widget_show_all (GTK_WIDGET (priv->applet));
}

void
main_menu_ui_close (MainMenuUI * ui, gboolean assured)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	if (assured || priv->conf->urgent_close)
		hide_main_window (ui);
}

void
main_menu_ui_reset_search (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	gtk_entry_set_text (priv->search_entry, "");
}

static void
bonobo_menu_menu_open_cb (BonoboUIComponent * component, gpointer user_data, const gchar * verb)
{
	show_main_window (MAIN_MENU_UI (user_data), GDK_CURRENT_TIME);
}

static void
bonobo_menu_menu_about_cb (BonoboUIComponent * component, gpointer user_data, const gchar * verb)
{
	MainMenuUI *main_menu_ui = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (main_menu_ui);

	if (! priv->about_dialog) {
		priv->about_dialog = gtk_about_dialog_new ();

		g_object_set (priv->about_dialog,
			"name", _("GNOME Main Menu"),
			"comments", _("The GNOME Main Menu"),
			"version", VERSION,
			"authors", main_menu_authors,
			"artists", main_menu_artists,
			"logo-icon-name", "gnome-fs-client",
			"copyright", "Copyright \xc2\xa9 2005-2006 Novell, Inc.",
			NULL);

		gtk_widget_show (priv->about_dialog);

		g_signal_connect (G_OBJECT (priv->about_dialog), "delete-event",
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	}

	if (priv->about_dialog)
		gtk_window_present (GTK_WINDOW (priv->about_dialog));
}

static void
applet_change_orient_cb (PanelApplet * applet, PanelAppletOrient orient, gpointer user_data)
{
	reorient_panel_button (MAIN_MENU_UI (user_data));
}

static void
applet_change_background_cb (PanelApplet * applet, PanelAppletBackgroundType type, GdkColor * color,
	GdkPixmap * pixmap, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	GtkRcStyle *rc_style;
	GtkStyle *style;

	/* reset style */
	gtk_widget_set_style (GTK_WIDGET (priv->applet), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (priv->applet), rc_style);
	gtk_rc_style_unref (rc_style);

	switch (type)
	{
	case PANEL_NO_BACKGROUND:
		break;
	case PANEL_COLOR_BACKGROUND:
		gtk_widget_modify_bg (GTK_WIDGET (priv->applet), GTK_STATE_NORMAL, color);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		style = gtk_style_copy (GTK_WIDGET (priv->applet)->style);
		if (style->bg_pixmap[GTK_STATE_NORMAL])
			g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
		style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
		gtk_widget_set_style (GTK_WIDGET (priv->applet), style);
		g_object_unref (style);
		break;
	}

	/* Alas, I don't know why this is necessary, but otherwise we get garbage
	 * the first time it draws.
	 */
	reorient_panel_button (ui);
}

static void
tomboy_bindkey_cb (gchar * key_string, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);

	show_main_window (ui, tomboy_keybinder_get_current_event_time ());
}

static void
build_main_menu_window (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	GtkWidget *window;

	GtkWidget *left_pane;

	GtkSizeGroup *search_show_label_group;

	GtkWidget *search_widget;

	GtkWidget *right_pane;

	GtkSizeGroup *right_pane_group;
	GtkSizeGroup *icon_group;
	GtkSizeGroup *label_group;

	GtkWidget *system_header;
	GtkWidget *status_header;
	GtkWidget *system_table_widget;
	GtkWidget *status_section;
	GtkWidget *hd_tile;
	GtkWidget *net_tile;

	priv->file_area = FILE_AREA_WIDGET (file_area_widget_new (ui, priv->engine));

	search_show_label_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	search_widget = create_search_widget (ui, search_show_label_group);

	gtk_size_group_add_widget (search_show_label_group, priv->file_area->selector_label);

	window = slab_window_new ();

	left_pane = gtk_vbox_new (FALSE, 12);

	if (priv->conf->lock_down_conf->search_area_visible)
		gtk_box_pack_start (GTK_BOX (left_pane), search_widget, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (left_pane), GTK_WIDGET (priv->file_area), FALSE, FALSE, 0);

	right_pane_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	system_header = get_section_header_label (_("System"));
	gtk_misc_set_alignment (GTK_MISC (system_header), 0.0, 0.5);
	gtk_size_group_add_widget (right_pane_group, system_header);

	status_header = get_section_header_label (_("Status"));
	gtk_misc_set_alignment (GTK_MISC (status_header), 0.0, 0.5);
	gtk_size_group_add_widget (right_pane_group, status_header);

	icon_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	label_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	system_table_widget = create_system_table_widget (ui, icon_group, label_group);
	gtk_size_group_add_widget (right_pane_group, system_table_widget);

	status_section = gtk_vbox_new (TRUE, 6);

	hd_tile = hard_drive_status_tile_new ();

	if (hd_tile)
	{
		g_signal_connect (G_OBJECT (hd_tile), "tile-action-triggered",
			G_CALLBACK (tile_action_triggered_cb), ui);

		gtk_box_pack_start (GTK_BOX (status_section), hd_tile, FALSE, FALSE, 0);
	}

	net_tile = network_status_tile_new ();

	if (net_tile)
	{
		g_signal_connect (G_OBJECT (net_tile), "tile-action-triggered",
			G_CALLBACK (tile_action_triggered_cb), ui);

		gtk_box_pack_start (GTK_BOX (status_section), net_tile, FALSE, FALSE, 0);
	}

	right_pane = gtk_vbox_new (FALSE, 12);

	if (priv->conf->lock_down_conf->system_area_visible)
	{
		gtk_box_pack_start (GTK_BOX (right_pane), system_header, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (right_pane), system_table_widget, FALSE, FALSE, 0);
	}

	if (priv->conf->lock_down_conf->status_area_visible)
	{
		gtk_box_pack_end (GTK_BOX (right_pane), status_section, FALSE, FALSE, 0);
		gtk_box_pack_end (GTK_BOX (right_pane), status_header, FALSE, FALSE, 0);
	}

	slab_window_set_contents (SLAB_WINDOW (window), left_pane, right_pane);

	g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete),
		NULL);

	g_signal_connect (G_OBJECT (window), "key-press-event",
		G_CALLBACK (main_window_key_press_cb), ui);

	g_signal_connect (G_OBJECT (window), "button_press_event",
		G_CALLBACK (main_window_button_press_cb), ui);

	g_signal_connect (G_OBJECT (window), "size_allocate", G_CALLBACK (main_window_resize_cb),
		ui);

	g_signal_connect (G_OBJECT (window), "grab-broken-event",
		G_CALLBACK (main_window_grab_broken_cb), ui);

	g_signal_connect (G_OBJECT (window), "map_event", G_CALLBACK (main_window_map_cb), ui);

	g_signal_connect (G_OBJECT (window), "unmap_event", G_CALLBACK (main_window_unmap_cb), ui);

	gtk_drag_dest_set (GTK_WIDGET (window), GTK_DEST_DEFAULT_ALL, NULL, 0,
		GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_add_uri_targets (GTK_WIDGET (window));

	g_signal_connect (G_OBJECT (window), "drag-data-received",
		G_CALLBACK (tile_drag_data_rcv_cb), ui);

	priv->main_window = window;
	priv->search_section = search_widget;
}

static void
build_panel_button (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	priv->panel_button = gtk_toggle_button_new ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->panel_button), FALSE);
	gtk_button_set_relief (GTK_BUTTON (priv->panel_button), GTK_RELIEF_NONE);

	reorient_panel_button (ui);

	g_object_set_data (G_OBJECT (priv->panel_button), "double-click-detector",
		double_click_detector_new ());

	g_signal_connect (G_OBJECT (priv->panel_button), "button_press_event",
		G_CALLBACK (panel_button_button_press_cb), ui);

	g_signal_connect (G_OBJECT (priv->panel_button), "enter-notify-event",
		G_CALLBACK (panel_button_enter_notify_cb), NULL);

	gtk_drag_dest_set (GTK_WIDGET (priv->panel_button), GTK_DEST_DEFAULT_ALL, NULL, 0,
		GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_add_uri_targets (GTK_WIDGET (priv->panel_button));

	g_signal_connect (G_OBJECT (priv->panel_button), "drag-data-received",
		G_CALLBACK (tile_drag_data_rcv_cb), ui);

	gtk_container_add (GTK_CONTAINER (priv->applet), priv->panel_button);
}

static void
reorient_panel_button (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	PanelAppletOrient orientation;

	GtkWidget *button;
	GtkWidget *child;

	GtkWidget *box;
	GtkWidget *icon;
	GtkWidget *label;

	orientation = panel_applet_get_orient (priv->applet);

	button = priv->panel_button;

	if (!GTK_IS_BUTTON (button))
		return;

	if ((child = gtk_bin_get_child (GTK_BIN (button))))
		gtk_container_remove (GTK_CONTAINER (button), child);

	icon = gtk_image_new_from_icon_name ("gnome-fs-client", GTK_ICON_SIZE_MENU);
	label = gtk_label_new (_("Computer"));

	switch (orientation)
	{
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
		box = gtk_hbox_new (FALSE, 6);
		gtk_box_pack_start (GTK_BOX (box), icon, 0, TRUE, TRUE);
		gtk_box_pack_start (GTK_BOX (box), label, 0, TRUE, TRUE);

		break;

	case PANEL_APPLET_ORIENT_LEFT:
		gtk_label_set_angle (GTK_LABEL (label), 90.0);

		box = gtk_vbox_new (FALSE, 6);
		gtk_box_pack_start (GTK_BOX (box), label, 0, TRUE, TRUE);
		gtk_box_pack_start (GTK_BOX (box), icon, 0, TRUE, TRUE);

		break;

	case PANEL_APPLET_ORIENT_RIGHT:
		gtk_label_set_angle (GTK_LABEL (label), 270.0);

		box = gtk_vbox_new (FALSE, 6);
		gtk_box_pack_start (GTK_BOX (box), icon, 0, TRUE, TRUE);
		gtk_box_pack_start (GTK_BOX (box), label, 0, TRUE, TRUE);

		break;

	default:
		box = NULL;

		g_assert_not_reached ();

		break;
	}

	gtk_container_add (GTK_CONTAINER (button), box);
	gtk_widget_show_all (button);
}

static gboolean
main_window_key_press_cb (GtkWidget * widget, GdkEventKey * event, gpointer user_data)
{
	switch (event->keyval)
	{
	case GDK_Escape:
		hide_main_window (MAIN_MENU_UI (user_data));

		return TRUE;

	case GDK_W:
	case GDK_w:
		if (event->state & GDK_CONTROL_MASK)
		{
			hide_main_window (MAIN_MENU_UI (user_data));

			return TRUE;
		}

	default:
		return FALSE;
	}
}

static void
main_window_map_cb (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);

	grab_focus (ui, event);
}

static void
main_window_unmap_cb (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (user_data);

	if (priv->ptr_is_grabbed)
	{
		gdk_pointer_ungrab (gdk_event_get_time (event));
		priv->ptr_is_grabbed = FALSE;
	}

	if (priv->kbd_is_grabbed)
	{
		gdk_keyboard_ungrab (gdk_event_get_time (event));
		priv->kbd_is_grabbed = FALSE;
	}

	gtk_grab_remove (widget);
}

static void
main_window_resize_cb (GtkWidget * widget, GtkAllocation * alloc, gpointer user_data)
{
	position_main_window (MAIN_MENU_UI (user_data));
}

static gboolean
main_window_grab_broken_cb (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	GdkEventGrabBroken *grab_event = (GdkEventGrabBroken *) event;
	gpointer window_data;

	if (grab_event->grab_window)
	{
		gdk_window_get_user_data (grab_event->grab_window, &window_data);

		if (GTK_IS_WIDGET (window_data))
			g_signal_connect (G_OBJECT (window_data), "event",
				G_CALLBACK (grabbing_window_event_cb), user_data);
	}

	return FALSE;
}

static gboolean
grabbing_window_event_cb (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	if (event->type == GDK_UNMAP || event->type == GDK_SELECTION_CLEAR)
		grab_focus (MAIN_MENU_UI (user_data), event);

	return FALSE;
}

static void
tile_drag_data_rcv_cb (GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y,
	GtkSelectionData * selection, guint info, guint time, gpointer user_data)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (user_data);

	gchar **uris;

	gint uri_len;

	gint i;

	if (gtk_drag_get_source_widget (drag_context))
		return;

	if (!selection)
		return;

	uris = gtk_selection_data_get_uris (selection);

	if (!uris)
		return;

	for (i = 0; uris && uris[i]; ++i)
	{
		uri_len = strlen (uris[i]);

		if (!strcmp (&uris[i][uri_len - 8], ".desktop"))
			main_menu_engine_add_user_app (priv->engine, uris[i]);
	}
}

static gboolean
main_window_button_press_cb (GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	GdkWindow *ptr_window = gdk_window_at_pointer (NULL, NULL);

	if (priv->main_window->window != ptr_window)
	{
		if (priv->conf->urgent_close)
			hide_main_window (ui);

		return TRUE;
	}

	return FALSE;
}

static gboolean
panel_button_button_press_cb (GtkWidget * button, GdkEventButton * event, gpointer user_data)
{
	DoubleClickDetector *detector;

	if (event->button != 1)
	{
		g_signal_stop_emission_by_name (button, "button_press_event");

		return FALSE;
	}

	detector = DOUBLE_CLICK_DETECTOR (
		g_object_get_data (G_OBJECT (button), "double-click-detector"));

	if (double_click_detector_is_double_click (detector, event->time, TRUE))
		return TRUE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		hide_main_window (MAIN_MENU_UI (user_data));
	else
		show_main_window (MAIN_MENU_UI (user_data), event->time);

	return TRUE;
}

static gboolean
panel_button_enter_notify_cb (GtkWidget * button, GdkEventButton * event, gpointer user_data)
{
	return TRUE;
}

static void
hide_main_window (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->panel_button), FALSE);

	gtk_widget_hide (priv->main_window);
}

static void
show_main_window (MainMenuUI * ui, guint32 timestamp)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	position_main_window (ui);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->panel_button), TRUE);

	gtk_widget_show_all (priv->main_window);
	gtk_window_present_with_time (GTK_WINDOW (priv->main_window), timestamp);

	if (!main_menu_engine_search_available (priv->engine))
		gtk_widget_hide_all (priv->search_section);
	else
		gtk_widget_grab_focus (GTK_WIDGET (priv->search_entry));
}

static void
position_main_window (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	GtkWidget *panel_widget;
	GtkWidget *main_menu_widget;

	GtkAllocation panel_widget_alloc;
	GtkRequisition main_window_req;

	GdkScreen *screen;
	GdkRectangle monitor_dim;

	gint x_new, y_new;

	panel_widget = priv->panel_button;
	main_menu_widget = priv->main_window;

	panel_widget_alloc.width = panel_widget->allocation.width;
	panel_widget_alloc.height = panel_widget->allocation.height;

	gdk_window_get_origin (panel_widget->window, &panel_widget_alloc.x, &panel_widget_alloc.y);
	gtk_widget_size_request (main_menu_widget, &main_window_req);

	x_new = y_new = 0;

	switch (panel_applet_get_orient (priv->applet))
	{
	case PANEL_APPLET_ORIENT_UP:
		x_new = panel_widget_alloc.x;
		y_new = panel_widget_alloc.y - main_window_req.height;

		break;

	case PANEL_APPLET_ORIENT_DOWN:
		x_new = panel_widget_alloc.x;
		y_new = panel_widget_alloc.y + panel_widget_alloc.height;

		break;

	case PANEL_APPLET_ORIENT_LEFT:
		x_new = panel_widget_alloc.x - main_window_req.width;
		y_new = panel_widget_alloc.y;

		break;

	case PANEL_APPLET_ORIENT_RIGHT:
		x_new = panel_widget_alloc.x + panel_widget_alloc.width;
		y_new = panel_widget_alloc.y;

		break;

	default:
		break;
	}

	screen = gtk_widget_get_screen (panel_widget);
	gdk_screen_get_monitor_geometry (screen,
		gdk_screen_get_monitor_at_window (screen, panel_widget->window),
		&monitor_dim);

	if (x_new + main_window_req.width > monitor_dim.width)
		x_new = monitor_dim.width - main_window_req.width;

	if (x_new < 0)
		x_new = 0;

	if (y_new + main_window_req.height > monitor_dim.height)
		y_new = monitor_dim.height - main_window_req.height;

	if (y_new < 0)
		y_new = 0;

	gtk_window_move (GTK_WINDOW (main_menu_widget), x_new, y_new);
}

static GtkWidget *
create_search_widget (MainMenuUI * ui, GtkSizeGroup * label_group)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	GtkWidget *hbox;
	GtkWidget *search_label;
	GtkWidget *search_entry;

	hbox = gtk_hbox_new (FALSE, 12);

	search_label = get_section_header_label (_("Search:"));
	gtk_misc_set_alignment (GTK_MISC (search_label), 0.0, 0.5);
	gtk_size_group_add_widget (label_group, search_label);
	gtk_box_pack_start (GTK_BOX (hbox), search_label, FALSE, FALSE, 0);

	search_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), search_entry, TRUE, TRUE, 0);

	g_signal_connect (G_OBJECT (search_entry), "activate",
		G_CALLBACK (search_entry_activate_cb), ui);

	priv->search_entry = GTK_ENTRY (search_entry);

	return hbox;
}

static GtkWidget *
create_system_table_widget (MainMenuUI * ui, GtkSizeGroup * icon_group, GtkSizeGroup * label_group)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	GtkWidget *table;

	GtkWidget *tile;
	GtkWidget *alignment;

	SystemTileType type;

	gboolean disable_lock_screen;
	gboolean disable_log_out;

	GList *type_list;
	GList *node;

	type_list = main_menu_engine_get_system_list (priv->engine);

	table = gtk_vbox_new (TRUE, SYSTEM_TILE_SPACING);

	disable_lock_screen = GPOINTER_TO_INT (get_gconf_value (DISABLE_LOCK_SCREEN_GCONF_KEY));
	disable_log_out = GPOINTER_TO_INT (get_gconf_value (DISABLE_LOG_OUT_GCONF_KEY));

	for (node = type_list; node; node = node->next)
	{
		type = GPOINTER_TO_INT (node->data);

		if (disable_lock_screen && type == SYSTEM_TILE_TYPE_LOCK_SCREEN)
			continue;

		if (disable_log_out && type == SYSTEM_TILE_TYPE_LOG_OUT)
			continue;

		tile = system_tile_new_with_type (type, priv->conf);

		if (tile)
		{
			alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
			gtk_container_add (GTK_CONTAINER (alignment), GTK_WIDGET (tile));

			gtk_box_pack_start (GTK_BOX (table), alignment, TRUE, TRUE, 0);

			g_signal_connect (G_OBJECT (tile), "tile-action-triggered",
				G_CALLBACK (tile_action_triggered_cb), ui);

			gtk_size_group_add_widget (icon_group, NAMEPLATE_TILE (tile)->image);
			gtk_size_group_add_widget (label_group, NAMEPLATE_TILE (tile)->header);
		}
	}

	g_list_free (type_list);

	return table;
}

static GtkWidget *
get_section_header_label (const gchar * markup)
{
	GtkWidget *label;
	gchar *text;

	text = g_strdup_printf ("<span size=\"large\">%s</span>", markup);

	label = gtk_label_new (text);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_widget_set_name (label, "gnome-main-menu-section-header");

	g_signal_connect (G_OBJECT (label), "style-set",
		G_CALLBACK (section_header_label_style_set), NULL);

	g_free (text);

	return label;
}

static void
section_header_label_style_set (GtkWidget * widget, GtkStyle * prev_style, gpointer user_data)
{
	if (prev_style
		&& widget->style->fg[GTK_STATE_SELECTED].green ==
		prev_style->fg[GTK_STATE_SELECTED].green)
		return;

	gtk_widget_modify_fg (widget, GTK_STATE_NORMAL, &widget->style->bg[GTK_STATE_SELECTED]);
}

static void
grab_focus (MainMenuUI * ui, GdkEvent * event)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	GdkGrabStatus status;
	guint32 time;

	time = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;

	if (priv->conf->urgent_close)
	{
		gtk_widget_grab_focus (priv->main_window);
		gtk_grab_add (priv->main_window);

		status = gdk_pointer_grab (priv->main_window->window, TRUE, GDK_BUTTON_PRESS_MASK,
			NULL, NULL, time);

		priv->ptr_is_grabbed = (status == GDK_GRAB_SUCCESS);

		status = gdk_keyboard_grab (priv->main_window->window, TRUE, time);

		priv->kbd_is_grabbed = (status == GDK_GRAB_SUCCESS);
	}
	else
	{
		if (priv->ptr_is_grabbed)
		{
			gdk_pointer_ungrab (time);
			priv->ptr_is_grabbed = FALSE;
		}

		if (priv->kbd_is_grabbed)
		{
			gdk_keyboard_ungrab (time);
			priv->kbd_is_grabbed = FALSE;
		}

		gtk_grab_remove (priv->main_window);
	}
}

static void
search_entry_activate_cb (GtkEntry * entry_widget, gpointer user_data)
{
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (user_data);

	main_menu_engine_execute_search (priv->engine, gtk_entry_get_text (entry_widget));
}

static void
tile_action_triggered_cb (Tile * tile, TileEvent * event, TileAction * action, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = MAIN_MENU_UI_GET_PRIVATE (ui);

	if (!priv->conf->urgent_close)
		return;

	if (TILE_ACTION_CHECK_FLAG (action, TILE_ACTION_OPENS_NEW_WINDOW)
		&& !TILE_ACTION_CHECK_FLAG (action, TILE_ACTION_OPENS_HELP))
		hide_main_window (ui);
}

static void
launch_search_cb (char *key, gpointer user_data)
{
	char *argv[] = { "beagle-search", NULL };

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void
bind_search_key (void)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *path, *contents, *key;
	gsize length;
	gboolean success, ctrl, alt;
	xmlChar *val;

	path = g_build_filename (g_get_home_dir (), ".beagle/config/searching.xml", NULL);
	success = g_file_get_contents (path, &contents, &length, NULL);
	g_free (path);

	if (!success)
		return;

	doc = xmlParseMemory (contents, length);
	g_free (contents);
	if (!doc)
		return;

	if (!doc->children || !doc->children->children)
	{
		xmlFreeDoc (doc);
		return;
	}

	for (node = doc->children->children; node; node = node->next)
	{
		if (!node->name || strcmp ((char *) node->name, "ShowSearchWindowBinding") != 0)
			continue;
		if (!node->children)
			break;

		val = xmlGetProp (node, (xmlChar *) "Ctrl");
		ctrl = val && !strcmp ((char *) val, "true");
		xmlFree (val);
		val = xmlGetProp (node, (xmlChar *) "Alt");
		alt = val && !strcmp ((char *) val, "true");
		xmlFree (val);

		for (node = node->children; node; node = node->next)
		{
			if (!node->name || strcmp ((char *) node->name, "Key") != 0)
				continue;
			val = xmlNodeGetContent (node);

			key = g_strdup_printf (
				"%s%s%s", ctrl ? "<Ctrl>" : "", alt ? "<Alt>" : "", val);
			xmlFree (val);

			tomboy_keybinder_bind (key, launch_search_cb, NULL);
			g_free (key);
			break;
		}
		break;
	}

	xmlFreeDoc (doc);
}
