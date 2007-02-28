/*
 * This file is part of libtile.
 *
 * Copyright (c) 2006, 2007 Novell, Inc.
 *
 * Libtile is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libtile is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "document-tile.h"

#include <glib/gi18n.h>
#include <string.h>
#include <libgnomeui/gnome-icon-lookup.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-find-directory.h>

#include "slab-gnome-util.h"
#include "gnome-utils.h"
#include "libslab-utils.h"
#include "recent-files.h"

#define GCONF_SEND_TO_CMD_KEY       "/desktop/gnome/applications/main-menu/file-area/file_send_to_cmd"
#define GCONF_ENABLE_DELETE_KEY_DIR "/apps/nautilus/preferences"
#define GCONF_ENABLE_DELETE_KEY     GCONF_ENABLE_DELETE_KEY_DIR "/enable_delete"

G_DEFINE_TYPE (DocumentTile, document_tile, NAMEPLATE_TILE_TYPE)

static void document_tile_finalize (GObject *);
static void document_tile_style_set (GtkWidget *, GtkStyle *);

static void document_tile_private_setup (DocumentTile *);
static void load_image (DocumentTile *);

static GtkWidget *create_header (const gchar *);
static GtkWidget *create_subheader (const gchar *);
static void update_user_list_menu_item (DocumentTile *);

static void header_size_allocate_cb (GtkWidget *, GtkAllocation *, gpointer);

static void open_with_default_trigger    (Tile *, TileEvent *, TileAction *);
static void open_in_file_manager_trigger (Tile *, TileEvent *, TileAction *);
static void rename_trigger               (Tile *, TileEvent *, TileAction *);
static void move_to_trash_trigger        (Tile *, TileEvent *, TileAction *);
static void delete_trigger               (Tile *, TileEvent *, TileAction *);
static void user_docs_trigger            (Tile *, TileEvent *, TileAction *);
static void send_to_trigger              (Tile *, TileEvent *, TileAction *);

static void rename_entry_activate_cb (GtkEntry *, gpointer);
static gboolean rename_entry_key_release_cb (GtkWidget *, GdkEventKey *, gpointer);

static void gconf_enable_delete_cb (GConfClient *, guint, GConfEntry *, gpointer);

static void docs_store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                                   GnomeVFSMonitorEventType, gpointer);

typedef struct
{
	gchar *basename;
	gchar *mime_type;
	time_t modified;
	
	GnomeVFSMimeApplication *default_app;
	
	GtkBin *header_bin;
	
	gboolean renaming;
	gboolean image_is_broken;
	
	gboolean delete_enabled;
	guint gconf_conn_id;

	gboolean is_in_user_list;

	GnomeVFSMonitorHandle *user_monitor;
	MainMenuRecentMonitor *recent_monitor;
} DocumentTilePrivate;

static GnomeThumbnailFactory *thumbnail_factory = NULL;

#define DOCUMENT_TILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DOCUMENT_TILE_TYPE, DocumentTilePrivate))

static void document_tile_class_init (DocumentTileClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);

	g_obj_class->finalize = document_tile_finalize;

	widget_class->style_set = document_tile_style_set;

	g_type_class_add_private (this_class, sizeof (DocumentTilePrivate));
}

GtkWidget *
document_tile_new (const gchar *in_uri, const gchar *mime_type, time_t modified)
{
	DocumentTile *this;
	DocumentTilePrivate *priv;

	gchar *uri;
	GtkWidget *image;
	GtkWidget *header;
	GtkWidget *subheader;
	GtkMenu *context_menu;

	GtkContainer *menu_ctnr;
	GtkWidget *menu_item;

	TileAction *action;

	gchar *basename;

	GDate *time_stamp;
	gchar *time_str;

	gchar *markup;

	AtkObject *accessible;
	
	gchar *filename;
	gchar *tooltip_text;
  
	uri = g_strdup (in_uri);

	image = gtk_image_new ();

	markup = g_path_get_basename (uri);
	basename = gnome_vfs_unescape_string (markup, NULL);
	g_free (markup);

	header = create_header (basename);

	time_stamp = g_date_new ();
	g_date_set_time (time_stamp, modified);

	time_str = g_new0 (gchar, 256);

	g_date_strftime (time_str, 256, _("Edited %m/%d/%Y"), time_stamp);
	g_date_free (time_stamp);

	subheader = create_subheader (time_str);

	filename = g_filename_from_uri (uri, NULL, NULL);

  	if (filename)
		tooltip_text = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	else
		tooltip_text = NULL;

	g_free (filename);
	
	context_menu = GTK_MENU (gtk_menu_new ());

	this = g_object_new (DOCUMENT_TILE_TYPE, "tile-uri", uri, "nameplate-image", image,
		"nameplate-header", header, "nameplate-subheader", subheader, 
		"nameplate-tooltip", tooltip_text, "context-menu", context_menu, NULL);

	g_free (uri);
	if (tooltip_text)
		g_free (tooltip_text);

	priv = DOCUMENT_TILE_GET_PRIVATE (this);
	priv->basename    = g_strdup (basename);
	priv->mime_type   = g_strdup (mime_type);
	priv->modified    = modified;
	priv->header_bin  = GTK_BIN (header);

	document_tile_private_setup (this);

	TILE (this)->actions = g_new0 (TileAction *, 6);
	TILE (this)->n_actions = 6;

	menu_ctnr = GTK_CONTAINER (TILE (this)->context_menu);

	/* make open with default action */

	if (priv->default_app) {
		markup = g_markup_printf_escaped (_("<b>Open with \"%s\"</b>"),
			priv->default_app->name);
		action = tile_action_new (TILE (this), open_with_default_trigger, markup,
			TILE_ACTION_OPENS_NEW_WINDOW);
		g_free (markup);

		TILE (this)->default_action = action;

		menu_item = GTK_WIDGET (GTK_WIDGET (tile_action_get_menu_item (action)));
	}
	else {
		action = NULL;
		menu_item = gtk_menu_item_new_with_label (_("Open with Default Application"));
		gtk_widget_set_sensitive (menu_item, FALSE);
	}

	TILE (this)->actions[DOCUMENT_TILE_ACTION_OPEN_WITH_DEFAULT] = action;

	gtk_container_add (menu_ctnr, menu_item);

	/* make open in nautilus action */

	action = tile_action_new (TILE (this), open_in_file_manager_trigger,
		_("Open in File Manager"), TILE_ACTION_OPENS_NEW_WINDOW);
	TILE (this)->actions[DOCUMENT_TILE_ACTION_OPEN_IN_FILE_MANAGER] = action;

	if (!TILE (this)->default_action)
		TILE (this)->default_action = action;

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
	gtk_container_add (menu_ctnr, menu_item);

	/* insert separator */

	menu_item = gtk_separator_menu_item_new ();
	gtk_container_add (menu_ctnr, menu_item);

	/* make rename action */

	action = tile_action_new (TILE (this), rename_trigger, _("Rename..."), 0);
	TILE (this)->actions[DOCUMENT_TILE_ACTION_RENAME] = action;

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
	gtk_container_add (menu_ctnr, menu_item);

	/* make send to action */

	/* Only allow Send To for local files, ideally this would use something
	 * equivalent to gnome_vfs_uri_is_local, but that method will stat the file and
	 * that can hang in some conditions. */

	if (!strncmp (TILE (this)->uri, "file://", 7))
	{
		action = tile_action_new (TILE (this), send_to_trigger, _("Send To..."),
			TILE_ACTION_OPENS_NEW_WINDOW);

		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
	}
	else
	{
		action = NULL;

		menu_item = gtk_menu_item_new_with_label (_("Send To..."));
		gtk_widget_set_sensitive (menu_item, FALSE);
	}

	TILE (this)->actions[DOCUMENT_TILE_ACTION_SEND_TO] = action;

	gtk_container_add (menu_ctnr, menu_item);

	/* make "add/remove to favorites" action */

	action = tile_action_new (TILE (this), user_docs_trigger, NULL, 0);
	TILE (this)->actions [DOCUMENT_TILE_ACTION_UPDATE_MAIN_MENU] = action;

	update_user_list_menu_item (this);

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));

	gtk_container_add (menu_ctnr, menu_item);

	/* insert separator */

	menu_item = gtk_separator_menu_item_new ();
	gtk_container_add (menu_ctnr, menu_item);

	/* make move to trash action */

	action = tile_action_new (TILE (this), move_to_trash_trigger, _("Move to Trash"), 0);
	TILE (this)->actions[DOCUMENT_TILE_ACTION_MOVE_TO_TRASH] = action;

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
	gtk_container_add (menu_ctnr, menu_item);

	/* make delete action */

	if (priv->delete_enabled)
	{
		action = tile_action_new (TILE (this), delete_trigger, _("Delete"), 0);
		TILE (this)->actions[DOCUMENT_TILE_ACTION_DELETE] = action;

		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
		gtk_container_add (menu_ctnr, menu_item);
	}

	gtk_widget_show_all (GTK_WIDGET (TILE (this)->context_menu));

	load_image (this);

	accessible = gtk_widget_get_accessible (GTK_WIDGET (this));
	if (basename)
	  atk_object_set_name (accessible, basename);
	if (time_str)
	  atk_object_set_description (accessible, time_str);

	g_free (basename);
	g_free (time_str);

	return GTK_WIDGET (this);
}

static void
document_tile_private_setup (DocumentTile *tile)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	GnomeVFSResult result;
	GnomeVFSFileInfo *info;

	GConfClient *client;

	info = gnome_vfs_file_info_new ();

	result = gnome_vfs_get_file_info (TILE (tile)->uri, info,
		GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);

	if (result == GNOME_VFS_OK)
		priv->default_app = gnome_vfs_mime_get_default_application (priv->mime_type);
	else
		priv->default_app = NULL;

	priv->renaming = FALSE;

	gnome_vfs_file_info_unref (info);

	priv->delete_enabled =
		(gboolean) GPOINTER_TO_INT (get_gconf_value (GCONF_ENABLE_DELETE_KEY));

	client = gconf_client_get_default ();

	gconf_client_add_dir (client, GCONF_ENABLE_DELETE_KEY_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
	priv->gconf_conn_id =
		connect_gconf_notify (GCONF_ENABLE_DELETE_KEY, gconf_enable_delete_cb, tile);

	g_object_unref (client);

	priv->is_in_user_list = libslab_user_docs_store_has_uri (TILE (tile)->uri);

	priv->user_monitor   = libslab_add_docs_monitor (docs_store_monitor_cb, tile);
	priv->recent_monitor = main_menu_recent_monitor_new ();
}

static void
document_tile_init (DocumentTile *tile)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	priv->basename = NULL;
	priv->mime_type = NULL;
	priv->default_app = NULL;
	priv->header_bin = NULL;
	priv->renaming = FALSE;
	priv->image_is_broken = TRUE;
	priv->delete_enabled = FALSE;
	priv->gconf_conn_id = 0;
	priv->is_in_user_list = FALSE;
	priv->user_monitor = NULL;
	priv->recent_monitor = NULL;
}

static void
document_tile_finalize (GObject *g_object)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (g_object);

	GConfClient *client;

	g_free (priv->basename);
	g_free (priv->mime_type);

	gnome_vfs_mime_application_free (priv->default_app);

	client = gconf_client_get_default ();

	gconf_client_notify_remove (client, priv->gconf_conn_id);
	gconf_client_remove_dir (client, GCONF_ENABLE_DELETE_KEY_DIR, NULL);

	g_object_unref (client);

	g_object_unref (priv->recent_monitor);

	if (priv->user_monitor)
		gnome_vfs_monitor_cancel (priv->user_monitor);

	(* G_OBJECT_CLASS (document_tile_parent_class)->finalize) (g_object);
}

static void
document_tile_style_set (GtkWidget *widget, GtkStyle *prev_style)
{
	load_image (DOCUMENT_TILE (widget));
}

static void
load_image (DocumentTile *tile)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	GdkPixbuf *thumb;
	gchar *thumb_path;

	gchar *icon_id = NULL;
	gboolean free_icon_id = TRUE;

	if (! priv->mime_type || ! strstr (TILE (tile)->uri, "file://")) {
		icon_id = "gnome-fs-regular";
		free_icon_id = FALSE;

		goto exit;
	}

	if (! thumbnail_factory)
		thumbnail_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);

	thumb_path = gnome_thumbnail_factory_lookup (thumbnail_factory, TILE (tile)->uri, priv->modified);

	if (!thumb_path) {
		if (
			gnome_thumbnail_factory_can_thumbnail (
				thumbnail_factory, TILE (tile)->uri, priv->mime_type, priv->modified)
		) {
			thumb = gnome_thumbnail_factory_generate_thumbnail (
				thumbnail_factory, TILE (tile)->uri, priv->mime_type);

			if (thumb) {
				gnome_thumbnail_factory_save_thumbnail (
					thumbnail_factory, thumb, TILE (tile)->uri, priv->modified);

				icon_id = gnome_thumbnail_factory_lookup (
					thumbnail_factory, TILE (tile)->uri, priv->modified);

				g_object_unref (thumb);
			}
			else
				gnome_thumbnail_factory_create_failed_thumbnail (
					thumbnail_factory, TILE (tile)->uri, priv->modified);
		}
	}
	else
		icon_id = thumb_path;

	if (! icon_id)
		icon_id = gnome_icon_lookup (
			gtk_icon_theme_get_default (), thumbnail_factory,
			TILE (tile)->uri, NULL, NULL, priv->mime_type, 0, NULL);

exit:

	priv->image_is_broken = slab_load_image (
		GTK_IMAGE (NAMEPLATE_TILE (tile)->image), GTK_ICON_SIZE_DND, icon_id);

	if (free_icon_id && icon_id)
		g_free (icon_id);
}

static GtkWidget *
create_header (const gchar *name)
{
	GtkWidget *header_bin;
	GtkWidget *header;

	header = gtk_label_new (name);
	gtk_label_set_ellipsize (GTK_LABEL (header), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	header_bin = gtk_alignment_new (0.0, 0.5, 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (header_bin), header);

	g_signal_connect (G_OBJECT (header), "size-allocate", G_CALLBACK (header_size_allocate_cb),
		NULL);

	return header_bin;
}

static GtkWidget *
create_subheader (const gchar *desc)
{
	GtkWidget *subheader;

	subheader = gtk_label_new (desc);
	gtk_label_set_ellipsize (GTK_LABEL (subheader), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (subheader), 0.0, 0.5);
	gtk_widget_modify_fg (subheader, GTK_STATE_NORMAL,
		&subheader->style->fg[GTK_STATE_INSENSITIVE]);

	return subheader;
}

static void
update_user_list_menu_item (DocumentTile *this)
{
	TileAction *action = TILE (this)->actions [DOCUMENT_TILE_ACTION_UPDATE_MAIN_MENU];
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (this);

	if (! action)
		return;

	if (priv->is_in_user_list)
		tile_action_set_menu_item_label (action, _("Remove from Favorites"));
	else
		tile_action_set_menu_item_label (action, _("Add to Favorites"));
}

static void
header_size_allocate_cb (GtkWidget *widget, GtkAllocation *alloc, gpointer user_data)
{
	gtk_widget_set_size_request (widget, alloc->width, -1);
}

static void
rename_entry_activate_cb (GtkEntry *entry, gpointer user_data)
{
	DocumentTile *tile = DOCUMENT_TILE (user_data);
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	GnomeVFSURI *src_uri;
	GnomeVFSURI *dst_uri;

	gchar *dirname;
	gchar *dst_path;
	gchar *dst_uri_str;

	GtkWidget *child;
	GtkWidget *header;

	GnomeVFSResult retval;


	if (strlen (gtk_entry_get_text (entry)) < 1)
		return;

	src_uri = gnome_vfs_uri_new (TILE (tile)->uri);

	dirname = gnome_vfs_uri_extract_dirname (src_uri);

	dst_path = g_build_filename (dirname, gtk_entry_get_text (entry), NULL);

	dst_uri = gnome_vfs_uri_new (dst_path);

	retval = gnome_vfs_xfer_uri (src_uri, dst_uri, GNOME_VFS_XFER_REMOVESOURCE,
		GNOME_VFS_XFER_ERROR_MODE_ABORT, GNOME_VFS_XFER_OVERWRITE_MODE_SKIP, NULL, NULL);

	dst_uri_str = gnome_vfs_uri_to_string (dst_uri, GNOME_VFS_URI_HIDE_NONE);

	if (retval == GNOME_VFS_OK) {
		main_menu_rename_recent_file (priv->recent_monitor, TILE (tile)->uri, dst_uri_str);

		g_free (priv->basename);
		priv->basename = g_strdup (gtk_entry_get_text (entry));
	}
	else
		g_warning ("unable to move [%s] to [%s]\n", TILE (tile)->uri, dst_uri_str);

	header = gtk_label_new (priv->basename);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	child = gtk_bin_get_child (priv->header_bin);

	if (child)
		gtk_widget_destroy (child);

	gtk_container_add (GTK_CONTAINER (priv->header_bin), header);

	gtk_widget_show (header);

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dst_uri);

	g_free (dirname);
	g_free (dst_path);
	g_free (dst_uri_str);
}

static gboolean
rename_entry_key_release_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	return TRUE;
}

static void
gconf_enable_delete_cb (GConfClient *client, guint conn_id, GConfEntry *entry, gpointer user_data)
{
	Tile *tile = TILE (user_data);
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (user_data);

	GtkMenuShell *menu;
	gboolean delete_enabled;

	TileAction *action;
	GtkWidget *menu_item;

	menu = GTK_MENU_SHELL (tile->context_menu);

	delete_enabled = gconf_value_get_bool (entry->value);

	if (delete_enabled == priv->delete_enabled)
		return;

	priv->delete_enabled = delete_enabled;

	if (priv->delete_enabled)
	{
		action = tile_action_new (tile, delete_trigger, _("Delete"), 0);
		tile->actions[DOCUMENT_TILE_ACTION_DELETE] = action;

		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
		gtk_menu_shell_insert (menu, menu_item, 7);

		gtk_widget_show_all (menu_item);
	}
	else
	{
		g_object_unref (tile->actions[DOCUMENT_TILE_ACTION_DELETE]);

		tile->actions[DOCUMENT_TILE_ACTION_DELETE] = NULL;
	}
}

static void
open_with_default_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	GList *uris = NULL;

	GnomeVFSResult retval;

	if (priv->default_app)
	{
		uris = g_list_append (uris, TILE (tile)->uri);

		retval = gnome_vfs_mime_application_launch (priv->default_app, uris);

		if (retval != GNOME_VFS_OK)
			g_warning
				("error: could not launch application with [%s].  GnomeVFSResult = %d\n",
				TILE (tile)->uri, retval);

		g_list_free (uris);
	}
}

static void
open_in_file_manager_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	gchar *filename;
	gchar *dirname;
	gchar *uri;

	gchar *cmd;

	filename = g_filename_from_uri (TILE (tile)->uri, NULL, NULL);
	dirname = g_path_get_dirname (filename);
	uri = g_filename_to_uri (dirname, NULL, NULL);

	if (!uri)
		g_warning ("error getting dirname for [%s]\n", TILE (tile)->uri);
	else
	{
		cmd = string_replace_once (get_slab_gconf_string (SLAB_FILE_MANAGER_OPEN_CMD),
			"FILE_URI", uri);

		spawn_process (cmd);

		g_free (cmd);
	}

	g_free (filename);
	g_free (dirname);
	g_free (uri);
}

static void
rename_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	GtkWidget *child;
	GtkWidget *entry;


	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), priv->basename);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

	child = gtk_bin_get_child (priv->header_bin);

	if (child)
		gtk_widget_destroy (child);

	gtk_container_add (GTK_CONTAINER (priv->header_bin), entry);

	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (rename_entry_activate_cb), tile);

	g_signal_connect (G_OBJECT (entry), "key_release_event",
		G_CALLBACK (rename_entry_key_release_cb), NULL);

	gtk_widget_show (entry);
	gtk_widget_grab_focus (entry);
}

static void
move_to_trash_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	GnomeVFSURI *src_uri;
	GnomeVFSURI *trash_uri;

	gchar *file_name;
	gchar *trash_uri_str;

	GnomeVFSResult retval;

	src_uri = gnome_vfs_uri_new (TILE (tile)->uri);

	gnome_vfs_find_directory (src_uri, GNOME_VFS_DIRECTORY_KIND_TRASH, &trash_uri,
		FALSE, FALSE, 0777);

	if (!trash_uri) {
		g_warning ("unable to find trash location\n");

		return;
	}

	file_name = gnome_vfs_uri_extract_short_name (src_uri);

	if (!file_name)
	{
		g_warning ("unable to extract short name from [%s]\n",
			gnome_vfs_uri_to_string (src_uri, GNOME_VFS_URI_HIDE_NONE));

		return;
	}

	trash_uri = gnome_vfs_uri_append_file_name (trash_uri, file_name);

	retval = gnome_vfs_xfer_uri (src_uri, trash_uri, GNOME_VFS_XFER_REMOVESOURCE,
		GNOME_VFS_XFER_ERROR_MODE_ABORT, GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE, NULL, NULL);

	if (retval == GNOME_VFS_OK)
		main_menu_remove_recent_file (priv->recent_monitor, TILE (tile)->uri);
	else {
		trash_uri_str = gnome_vfs_uri_to_string (trash_uri, GNOME_VFS_URI_HIDE_NONE);

		g_warning ("unable to move [%s] to the trash [%s]\n", TILE (tile)->uri,
			trash_uri_str);

		g_free (trash_uri_str);
	}

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (trash_uri);

	g_free (file_name);
}

static void
delete_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	GnomeVFSURI *src_uri;
	GList *list = NULL;

	GnomeVFSResult retval;


	src_uri = gnome_vfs_uri_new (TILE (tile)->uri);

	list = g_list_append (list, src_uri);

	retval = gnome_vfs_xfer_delete_list (list, GNOME_VFS_XFER_ERROR_MODE_ABORT,
		GNOME_VFS_XFER_REMOVESOURCE, NULL, NULL);

	if (retval == GNOME_VFS_OK)
		main_menu_remove_recent_file (priv->recent_monitor, TILE (tile)->uri);
	else
		g_warning ("unable to delete [%s]\n", TILE (tile)->uri);

	gnome_vfs_uri_unref (src_uri);
	g_list_free (list);
}

static void
user_docs_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	DocumentTile *this        = DOCUMENT_TILE             (tile);
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (this);


	if (priv->is_in_user_list)
		libslab_remove_user_doc (tile->uri);
	else
		libslab_add_user_doc (
			tile->uri, priv->mime_type, priv->modified,
			gnome_vfs_mime_application_get_name (priv->default_app),
			gnome_vfs_mime_application_get_exec (priv->default_app));

	update_user_list_menu_item (this);
}

static void
send_to_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	gchar *cmd;
	gchar **argv;

	gchar *filename;
	gchar *dirname;
	gchar *basename;

	GError *error = NULL;

	gchar *tmp;
	gint i;

	cmd = (gchar *) get_gconf_value (GCONF_SEND_TO_CMD_KEY);
	argv = g_strsplit (cmd, " ", 0);

	filename = g_filename_from_uri (TILE (tile)->uri, NULL, NULL);
	dirname = g_path_get_dirname (filename);
	basename = g_path_get_basename (filename);

	for (i = 0; argv[i]; ++i)
	{
		if (strstr (argv[i], "DIRNAME"))
		{
			tmp = string_replace_once (argv[i], "DIRNAME", dirname);
			g_free (argv[i]);
			argv[i] = tmp;
		}

		if (strstr (argv[i], "BASENAME"))
		{
			tmp = string_replace_once (argv[i], "BASENAME", basename);
			g_free (argv[i]);
			argv[i] = tmp;
		}
	}

	gdk_spawn_on_screen (gtk_widget_get_screen (GTK_WIDGET (tile)), NULL, argv, NULL,
		G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

	if (error)
		handle_g_error (&error, "error in %s", G_STRFUNC);

	g_free (cmd);
	g_free (filename);
	g_free (dirname);
	g_free (basename);
	g_strfreev (argv);
}

static void
docs_store_monitor_cb (
	GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	DocumentTile *this = DOCUMENT_TILE (user_data);
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (this);

	gboolean is_in_user_list_current;


	is_in_user_list_current = libslab_user_docs_store_has_uri (TILE (this)->uri);

	if (is_in_user_list_current == priv->is_in_user_list)
		return;

	priv->is_in_user_list = is_in_user_list_current;

	update_user_list_menu_item (this);
}
