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
#include <eel/eel-alert-dialog.h>
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
#include "bookmark-agent.h"

#define GCONF_SEND_TO_CMD_KEY       "/desktop/gnome/applications/main-menu/file-area/file_send_to_cmd"
#define GCONF_ENABLE_DELETE_KEY_DIR "/apps/nautilus/preferences"
#define GCONF_ENABLE_DELETE_KEY     GCONF_ENABLE_DELETE_KEY_DIR "/enable_delete"
#define GCONF_CONFIRM_DELETE_KEY    GCONF_ENABLE_DELETE_KEY_DIR "/confirm_trash"

G_DEFINE_TYPE (DocumentTile, document_tile, NAMEPLATE_TILE_TYPE)

static void document_tile_finalize (GObject *);
static void document_tile_style_set (GtkWidget *, GtkStyle *);

static void document_tile_private_setup (DocumentTile *);
static void load_image (DocumentTile *);

static GtkWidget *create_header (const gchar *);

static char      *create_subheader_string (time_t date);
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

static void agent_notify_cb (GObject *, GParamSpec *, gpointer);

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

	BookmarkAgent       *agent;
	BookmarkStoreStatus  store_status;
	gboolean             is_bookmarked;
	gulong               notify_signal_id;
} DocumentTilePrivate;

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

	gchar *time_str;

	gchar *markup;

	AtkObject *accessible;
	
	gchar *filename;
	gchar *tooltip_text;

	libslab_checkpoint ("document_tile_new(): start");
  
	uri = g_strdup (in_uri);

	image = gtk_image_new ();

	markup = g_path_get_basename (uri);
	basename = gnome_vfs_unescape_string (markup, NULL);
	g_free (markup);

	header = create_header (basename);

	time_str = create_subheader_string (modified);
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

	TILE (this)->actions = g_new0 (TileAction *, 7);
	TILE (this)->n_actions = 7;

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

	accessible = gtk_widget_get_accessible (GTK_WIDGET (this));
	if (basename)
	  atk_object_set_name (accessible, basename);
	if (time_str)
	  atk_object_set_description (accessible, time_str);

	g_free (basename);
	g_free (time_str);

	libslab_checkpoint ("document_tile_new(): end");

	return GTK_WIDGET (this);
}

static void
document_tile_private_setup (DocumentTile *this)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (this);

	GnomeVFSResult result;
	GnomeVFSFileInfo *info;

	GConfClient *client;


	info = gnome_vfs_file_info_new ();

	result = gnome_vfs_get_file_info (TILE (this)->uri, info,
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
		connect_gconf_notify (GCONF_ENABLE_DELETE_KEY, gconf_enable_delete_cb, this);

	g_object_unref (client);

	priv->agent = bookmark_agent_get_instance (BOOKMARK_STORE_USER_DOCS);

	priv->notify_signal_id = g_signal_connect (
		G_OBJECT (priv->agent), "notify", G_CALLBACK (agent_notify_cb), this);
}

static void
document_tile_init (DocumentTile *tile)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (tile);

	priv->basename         = NULL;
	priv->mime_type        = NULL;
	priv->modified         = 0;

	priv->default_app      = NULL;

	priv->header_bin       = NULL;

	priv->renaming         = FALSE;
	priv->image_is_broken  = TRUE;

	priv->delete_enabled   = FALSE;
	priv->gconf_conn_id    = 0;

	priv->agent            = NULL;
	priv->store_status     = BOOKMARK_STORE_DEFAULT;
	priv->is_bookmarked    = FALSE;
	priv->notify_signal_id = 0;
}

static void
document_tile_finalize (GObject *g_object)
{
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (g_object);

	GConfClient *client;

	g_free (priv->basename);
	g_free (priv->mime_type);

	gnome_vfs_mime_application_free (priv->default_app);

	if (priv->notify_signal_id)
		g_signal_handler_disconnect (priv->agent, priv->notify_signal_id);

	g_object_unref (G_OBJECT (priv->agent));

	client = gconf_client_get_default ();

	gconf_client_notify_remove (client, priv->gconf_conn_id);
	gconf_client_remove_dir (client, GCONF_ENABLE_DELETE_KEY_DIR, NULL);

	g_object_unref (client);

	G_OBJECT_CLASS (document_tile_parent_class)->finalize (g_object);
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
	GnomeThumbnailFactory *thumbnail_factory;

	libslab_checkpoint ("document-tile.c: load_image(): start for %s", TILE (tile)->uri);

	if (! priv->mime_type || ! strstr (TILE (tile)->uri, "file://")) {
		icon_id = "gnome-fs-regular";
		free_icon_id = FALSE;

		goto exit;
	}

	thumbnail_factory = libslab_thumbnail_factory_get ();

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

	libslab_checkpoint ("document-tile.c: load_image(): end");
}

/* Next function taken from e-data-server-util.c in evolution-data-server */
/** 
 * e_strftime:
 * @s: The string array to store the result in.
 * @max: The size of array @s.
 * @fmt: The formatting to use on @tm.
 * @tm: The time value to format.
 *
 * This function is a wrapper around the strftime(3) function, which
 * converts the &percnt;l and &percnt;k (12h and 24h) format variables if necessary.
 *
 * Returns: The number of characters placed in @s.
 **/
static size_t
e_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
#ifdef HAVE_LKSTRFTIME
	return strftime(s, max, fmt, tm);
#else
	char *c, *ffmt, *ff;
	size_t ret;

	ffmt = g_strdup(fmt);
	ff = ffmt;
	while ((c = strstr(ff, "%l")) != NULL) {
		c[1] = 'I';
		ff = c;
	}

	ff = ffmt;
	while ((c = strstr(ff, "%k")) != NULL) {
		c[1] = 'H';
		ff = c;
	}

#ifdef G_OS_WIN32
	/* The Microsoft strftime() doesn't have %e either */
	ff = ffmt;
	while ((c = strstr(ff, "%e")) != NULL) {
		c[1] = 'd';
		ff = c;
	}
#endif

	ret = strftime(s, max, ffmt, tm);
	g_free(ffmt);
	return ret;
#endif
}

/* Next two functions taken from e-util.c in evolution */
/**
 * Function to do a last minute fixup of the AM/PM stuff if the locale
 * and gettext haven't done it right. Most English speaking countries
 * except the USA use the 24 hour clock (UK, Australia etc). However
 * since they are English nobody bothers to write a language
 * translation (gettext) file. So the locale turns off the AM/PM, but
 * gettext does not turn on the 24 hour clock. Leaving a mess.
 *
 * This routine checks if AM/PM are defined in the locale, if not it
 * forces the use of the 24 hour clock.
 *
 * The function itself is a front end on strftime and takes exactly
 * the same arguments.
 *
 * TODO: Actually remove the '%p' from the fixed up string so that
 * there isn't a stray space.
 **/

static size_t
e_strftime_fix_am_pm(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	char buf[10];
	char *sp;
	char *ffmt;
	size_t ret;

	if (strstr(fmt, "%p")==NULL && strstr(fmt, "%P")==NULL) {
		/* No AM/PM involved - can use the fmt string directly */
		ret=e_strftime(s, max, fmt, tm);
	} else {
		/* Get the AM/PM symbol from the locale */
		e_strftime (buf, 10, "%p", tm);

		if (buf[0]) {
			/**
			 * AM/PM have been defined in the locale
			 * so we can use the fmt string directly
			 **/
			ret=e_strftime(s, max, fmt, tm);
		} else {
			/**
			 * No AM/PM defined by locale
			 * must change to 24 hour clock
			 **/
			ffmt=g_strdup(fmt);
			for (sp=ffmt; (sp=strstr(sp, "%l")); sp++) {
				/**
				 * Maybe this should be 'k', but I have never
				 * seen a 24 clock actually use that format
				 **/
				sp[1]='H';
			}
			for (sp=ffmt; (sp=strstr(sp, "%I")); sp++) {
				sp[1]='H';
			}
			ret=e_strftime(s, max, ffmt, tm);
			g_free(ffmt);
		}
	}

	return(ret);
}

static size_t 
e_utf8_strftime_fix_am_pm(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	size_t sz, ret;
	char *locale_fmt, *buf;

	locale_fmt = g_locale_from_utf8(fmt, -1, NULL, &sz, NULL);
	if (!locale_fmt)
		return 0;

	ret = e_strftime_fix_am_pm(s, max, locale_fmt, tm);
	if (!ret) {
		g_free (locale_fmt);
		return 0;
	}

	buf = g_locale_to_utf8(s, ret, NULL, &sz, NULL);
	if (!buf) {
		g_free (locale_fmt);
		return 0;
	}

	if (sz >= max) {
		char *tmp = buf + max - 1;
		tmp = g_utf8_find_prev_char(buf, tmp);
		if (tmp)
			sz = tmp - buf;
		else
			sz = 0;
	}
	memcpy(s, buf, sz);
	s[sz] = '\0';
	g_free(locale_fmt);
	g_free(buf);
	return sz;
}

static char *
create_subheader_string (time_t date)
{
	time_t nowdate = time(NULL);
	time_t yesdate;
	struct tm then, now, yesterday;
	char buf[100];
	gboolean done = FALSE;

	if (date == 0) {
		return g_strdup (_("?"));
	}

	localtime_r (&date, &then);
	localtime_r (&nowdate, &now);

	if (nowdate - date < 60 * 60 * 8 && nowdate > date) {
		e_utf8_strftime_fix_am_pm (buf, 100, _("%l:%M %p"), &then);
		done = TRUE;
	}

	if (!done) {
		if (then.tm_mday == now.tm_mday &&
		    then.tm_mon == now.tm_mon &&
		    then.tm_year == now.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 100, _("Today %l:%M %p"), &then);
			done = TRUE;
		}
	}
	if (!done) {
		yesdate = nowdate - 60 * 60 * 24;
		localtime_r (&yesdate, &yesterday);
		if (then.tm_mday == yesterday.tm_mday &&
		    then.tm_mon == yesterday.tm_mon &&
		    then.tm_year == yesterday.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 100, _("Yesterday %l:%M %p"), &then);
			done = TRUE;
		}
	}
	if (!done) {
		int i;
		for (i = 2; i < 7; i++) {
			yesdate = nowdate - 60 * 60 * 24 * i;
			localtime_r (&yesdate, &yesterday);
			if (then.tm_mday == yesterday.tm_mday &&
			    then.tm_mon == yesterday.tm_mon &&
			    then.tm_year == yesterday.tm_year) {
				e_utf8_strftime_fix_am_pm (buf, 100, _("%a %l:%M %p"), &then);
				done = TRUE;
				break;
			}
		}
	}
	if (!done) {
		if (then.tm_year == now.tm_year) {
			e_utf8_strftime_fix_am_pm (buf, 100, _("%b %d %l:%M %p"), &then);
		} else {
			e_utf8_strftime_fix_am_pm (buf, 100, _("%b %d %Y"), &then);
		}
	}

	return g_strdup (g_strstrip (buf));
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
	DocumentTilePrivate *priv = DOCUMENT_TILE_GET_PRIVATE (this);

	TileAction  *action;
	GtkMenuItem *item;


	action = TILE (this)->actions [DOCUMENT_TILE_ACTION_UPDATE_MAIN_MENU];

	if (! action)
		return;

	priv->is_bookmarked = bookmark_agent_has_item (priv->agent, TILE (this)->uri);

	if (priv->is_bookmarked)
		tile_action_set_menu_item_label (action, _("Remove from Favorites"));
	else
		tile_action_set_menu_item_label (action, _("Add to Favorites"));

	item = tile_action_get_menu_item (action);

	if (! GTK_IS_MENU_ITEM (item))
		return;

	g_object_get (G_OBJECT (priv->agent), BOOKMARK_AGENT_STORE_STATUS_PROP, & priv->store_status, NULL);

	gtk_widget_set_sensitive (GTK_WIDGET (item), (priv->store_status != BOOKMARK_STORE_DEFAULT_ONLY));
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
		bookmark_agent_move_item (priv->agent, TILE (tile)->uri, dst_uri_str);

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
		bookmark_agent_remove_item (priv->agent, TILE (tile)->uri);
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

	gchar     *prompt;
	GtkDialog *confirm_dialog;
	gint       result;

	GnomeVFSURI *src_uri;
	GList *list = NULL;

	GnomeVFSResult retval;


	if (GPOINTER_TO_INT (libslab_get_gconf_value (GCONF_CONFIRM_DELETE_KEY))) {
		prompt = g_strdup_printf (
			_("Are you sure you want to permanently delete \"%s\"?"), priv->basename);

		confirm_dialog = GTK_DIALOG (eel_alert_dialog_new (
			NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
			prompt, _("If you delete an item, it is permanently lost.")));
							
		gtk_dialog_add_button (confirm_dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
		gtk_dialog_add_button (confirm_dialog, GTK_STOCK_DELETE, GTK_RESPONSE_YES);
		gtk_dialog_set_default_response (GTK_DIALOG (confirm_dialog), GTK_RESPONSE_YES);

		result = gtk_dialog_run (confirm_dialog);

		gtk_widget_destroy (GTK_WIDGET (confirm_dialog));
		g_free (prompt);

		if (result != GTK_RESPONSE_YES)
			return;
	}

	src_uri = gnome_vfs_uri_new (TILE (tile)->uri);

	list = g_list_append (list, src_uri);

	retval = gnome_vfs_xfer_delete_list (list, GNOME_VFS_XFER_ERROR_MODE_ABORT,
		GNOME_VFS_XFER_REMOVESOURCE, NULL, NULL);

	if (retval == GNOME_VFS_OK)
		bookmark_agent_remove_item (priv->agent, TILE (tile)->uri);
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

	BookmarkItem *item;


	if (priv->is_bookmarked)
		bookmark_agent_remove_item (priv->agent, tile->uri);
	else {
		item = g_new0 (BookmarkItem, 1);
		item->uri       = tile->uri;
		item->mime_type = priv->mime_type;
		item->mtime     = priv->modified;
		item->app_name  = (gchar *) gnome_vfs_mime_application_get_name (priv->default_app);
		item->app_exec  = (gchar *) gnome_vfs_mime_application_get_exec (priv->default_app);

		bookmark_agent_add_item (priv->agent, item);
		g_free (item);
	}

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
agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	update_user_list_menu_item (DOCUMENT_TILE (user_data));
}
