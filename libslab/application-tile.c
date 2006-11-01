/*
 * This file is part of libtile.
 *
 * Copyright (c) 2006 Novell, Inc.
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

#include "application-tile.h"

#include <string.h>
#include <glib/gi18n.h>
#include <glib/gfileutils.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-monitor.h>

#include "slab-gnome-util.h"

#include "themed-icon.h"

G_DEFINE_TYPE (ApplicationTile, application_tile, NAMEPLATE_TILE_TYPE)

typedef enum {
	APP_IN_USER_STARTUP_DIR,
	APP_NOT_IN_STARTUP_DIR,
	APP_NOT_ELIGIBLE
} StartupStatus;

static void application_tile_get_property (GObject *, guint,       GValue *, GParamSpec *);
static void application_tile_set_property (GObject *, guint, const GValue *, GParamSpec *);
static void application_tile_finalize     (GObject *);

static void application_tile_setup (ApplicationTile *);

static GtkWidget *create_header    (const gchar *);
static GtkWidget *create_subheader (const gchar *);

static void header_size_allocate_cb (GtkWidget *, GtkAllocation *, gpointer);

static void start_trigger     (Tile *, TileEvent *, TileAction *);
static void help_trigger      (Tile *, TileEvent *, TileAction *);
static void user_apps_trigger (Tile *, TileEvent *, TileAction *);
static void startup_trigger   (Tile *, TileEvent *, TileAction *);
static void upgrade_trigger   (Tile *, TileEvent *, TileAction *);
static void uninstall_trigger (Tile *, TileEvent *, TileAction *);

static void add_to_user_list         (ApplicationTile *);
static void remove_from_user_list    (ApplicationTile *);
static void add_to_startup_list      (ApplicationTile *);
static void remove_from_startup_list (ApplicationTile *);

static gboolean verify_package_management_command (gchar *);
static void run_package_management_command (ApplicationTile *, gchar *);

static gboolean is_desktop_item_in_user_list (const gchar *);
static void     update_user_list_menu_item (ApplicationTile *);

static StartupStatus get_desktop_item_startup_status (GnomeDesktopItem *);
static void          update_startup_menu_item (ApplicationTile *);

static void gconf_user_list_change_cb (GConfClient *, guint, GConfEntry *, gpointer);

typedef struct {
	GnomeDesktopItem *desktop_item;

	gchar       *image_id;
	gboolean     image_is_broken;
	GtkIconSize  image_size;

	gboolean is_in_user_list;
	StartupStatus startup_status;

	GConfClient *gconf_client;
	guint        gconf_conn_id;
} ApplicationTilePrivate;

#define APPLICATION_TILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), APPLICATION_TILE_TYPE, ApplicationTilePrivate))

enum {
	PROP_0,
	PROP_APPLICATION_NAME,
	PROP_APPLICATION_DESCRIPTION
};

static void
application_tile_class_init (ApplicationTileClass *app_tile_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (app_tile_class);

	g_obj_class->get_property = application_tile_get_property;
	g_obj_class->set_property = application_tile_set_property;
	g_obj_class->finalize     = application_tile_finalize;

	g_type_class_add_private (app_tile_class, sizeof (ApplicationTilePrivate));

	g_object_class_install_property (
		g_obj_class, PROP_APPLICATION_NAME,
		g_param_spec_string (
			"application-name", "application-name",
			"the name of the application", NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		g_obj_class, PROP_APPLICATION_DESCRIPTION,
		g_param_spec_string (
			"application-description", "application-description",
			"the name of the application", NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

GtkWidget *
application_tile_new (const gchar *desktop_item_id)
{
	return application_tile_new_full (desktop_item_id, GTK_ICON_SIZE_DND);
}

GtkWidget *
application_tile_new_full (const gchar *desktop_item_id, GtkIconSize image_size)
{
	ApplicationTile        *this;
	ApplicationTilePrivate *priv;

	const gchar *uri = NULL;

	GnomeDesktopItem *desktop_item;


	desktop_item = load_desktop_item_from_unknown (desktop_item_id);

	if (desktop_item)
		uri = gnome_desktop_item_get_location (desktop_item);

	if (! desktop_item || ! uri)
		return NULL;

	this = g_object_new (APPLICATION_TILE_TYPE, "tile-uri", uri, NULL);
	priv = APPLICATION_TILE_GET_PRIVATE (this);

	priv->image_size   = image_size;
	priv->desktop_item = desktop_item;

	application_tile_setup (this);

	return GTK_WIDGET (this);
}

static void
application_tile_init (ApplicationTile *tile)
{
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (tile);

	priv->desktop_item    = NULL;
	priv->image_id        = NULL;
	priv->image_is_broken = TRUE;

	priv->is_in_user_list = FALSE;

	priv->gconf_client  = NULL;
	priv->gconf_conn_id = 0;
}

static void
application_tile_finalize (GObject *g_object)
{
	ApplicationTile *tile = APPLICATION_TILE (g_object);
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (g_object);

	if (tile->name)
		g_free (tile->name);
	if (tile->description)
		g_free (tile->description);

	if (priv->desktop_item)
		gnome_desktop_item_unref (priv->desktop_item);
	if (priv->image_id)
		g_free (priv->image_id);

	gconf_client_notify_remove (priv->gconf_client, priv->gconf_conn_id);
	g_object_unref (priv->gconf_client);

	(* G_OBJECT_CLASS (application_tile_parent_class)->finalize) (g_object);
}

static void
application_tile_get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *param_spec)
{
	ApplicationTile *tile = APPLICATION_TILE (g_obj);

	switch (prop_id) {
		case PROP_APPLICATION_NAME:
			g_value_set_string (value, tile->name);
			break;

		case PROP_APPLICATION_DESCRIPTION:
			g_value_set_string (value, tile->description);
			break;

		default:
			break;
	}
}

static void
application_tile_set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *param_spec)
{
	ApplicationTile *tile = APPLICATION_TILE (g_obj);

	switch (prop_id) {
		case PROP_APPLICATION_NAME:
			tile->name = g_strdup (g_value_get_string (value));
			break;

		case PROP_APPLICATION_DESCRIPTION:
			tile->description = g_strdup (g_value_get_string (value));
			break;

		default:
			break;
	}
}

static void
application_tile_setup (ApplicationTile *this)
{
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	GtkWidget *image;
	GtkWidget *header;
	GtkWidget *subheader;
	GtkMenu   *context_menu;
	AtkObject *accessible;

	TileAction  **actions;
	TileAction   *action;
	GtkWidget    *menu_item;
	GtkContainer *menu_ctnr;

	const gchar *name;
	const gchar *desc;

	gchar *markup;

	GError *error = NULL;


	if (! priv->desktop_item) {
		priv->desktop_item = load_desktop_item_from_unknown (TILE (this)->uri);

		if (! priv->desktop_item)
			return;
	}

	priv->image_id = g_strdup (gnome_desktop_item_get_localestring (priv->desktop_item, "Icon"));
	image = themed_icon_new (priv->image_id, priv->image_size);

	name = gnome_desktop_item_get_localestring (priv->desktop_item, "Name");
	desc = gnome_desktop_item_get_localestring (priv->desktop_item, "GenericName");

	accessible = gtk_widget_get_accessible (GTK_WIDGET (this));
	if (name)
	  atk_object_set_name (accessible, name);
	if (desc)
	  atk_object_set_description (accessible, desc);

	header    = create_header    (name);
	subheader = create_subheader (desc);

	context_menu = GTK_MENU (gtk_menu_new ());

	g_object_set (
		G_OBJECT (this),
		"nameplate-image",         image,
		"nameplate-header",        header,
		"nameplate-subheader",     subheader,
		"context-menu",            context_menu,
		"application-name",        name,
		"application-description", desc,
		NULL);

	priv->is_in_user_list = is_desktop_item_in_user_list (TILE (this)->uri);
	priv->startup_status  = get_desktop_item_startup_status (priv->desktop_item);

	actions = g_new0 (TileAction *, 6);

	TILE (this)->actions   = actions;
	TILE (this)->n_actions = 6;

	menu_ctnr = GTK_CONTAINER (TILE (this)->context_menu);

/* make start action */

	markup = g_markup_printf_escaped (_("<b>Start %s</b>"), this->name);
	action = tile_action_new (TILE (this), start_trigger, markup, TILE_ACTION_OPENS_NEW_WINDOW);
	actions [APPLICATION_TILE_ACTION_START] = action;
	g_free (markup);

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));

	gtk_container_add (menu_ctnr, menu_item);

	TILE (this)->default_action = action;

/* insert separator */

	gtk_container_add (menu_ctnr, gtk_separator_menu_item_new ());

/* make help action */

	if (gnome_desktop_item_get_string (priv->desktop_item, "DocPath")) {
		action = tile_action_new (
			TILE (this), help_trigger, _("Help"),
			TILE_ACTION_OPENS_NEW_WINDOW | TILE_ACTION_OPENS_HELP);

		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
	}
	else {
		action = NULL;
		menu_item = gtk_menu_item_new_with_label (_("Help Unavailable"));
		gtk_widget_set_sensitive (menu_item, FALSE);
	}

	actions [APPLICATION_TILE_ACTION_HELP] = action;

	gtk_container_add (menu_ctnr, menu_item);

/* insert separator */

	gtk_container_add (menu_ctnr, gtk_separator_menu_item_new ());

/* make "add/remove to favorites" action */

	action = tile_action_new (TILE (this), user_apps_trigger, NULL, 0);
	actions [APPLICATION_TILE_ACTION_UPDATE_MAIN_MENU] = action;

	update_user_list_menu_item (this);

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));

	gtk_container_add (menu_ctnr, menu_item);

/* make "add/remove to startup" action */

	if (priv->startup_status != APP_NOT_ELIGIBLE) {
		action = tile_action_new (TILE (this), startup_trigger, NULL, 0);
		actions [APPLICATION_TILE_ACTION_UPDATE_STARTUP] = action;

		update_startup_menu_item (this);

		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));

		gtk_container_add (menu_ctnr, menu_item);
	}

/* make upgrade action */

	if (verify_package_management_command (SLAB_UPGRADE_PACKAGE_KEY)) {
		action = tile_action_new (TILE (this), upgrade_trigger, _("Upgrade"), 0);
		actions [APPLICATION_TILE_ACTION_UPGRADE_PACKAGE] = action;
		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
		gtk_container_add (menu_ctnr, menu_item);
	}

/* make uninstall action */

	if (verify_package_management_command (SLAB_UNINSTALL_PACKAGE_KEY)) {
		action = tile_action_new (TILE (this), uninstall_trigger, _("Uninstall"), 0);
		actions [APPLICATION_TILE_ACTION_UNINSTALL_PACKAGE] = action;
		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
		gtk_container_add (menu_ctnr, menu_item);
	}

	priv->gconf_client = gconf_client_get_default ();
	priv->gconf_conn_id = gconf_client_notify_add (
		priv->gconf_client, SLAB_USER_SPECIFIED_APPS_KEY,
		gconf_user_list_change_cb, this, NULL, &error);

	if (error) {
		g_warning ("error monitoring %s [%s]\n", SLAB_USER_SPECIFIED_APPS_KEY, error->message);

		g_error_free (error);
	}

	gtk_widget_show_all (GTK_WIDGET (TILE (this)->context_menu));
}

static GtkWidget *
create_header (const gchar *name)
{
	GtkWidget *header;


	header = gtk_label_new (name);
	gtk_label_set_ellipsize (GTK_LABEL (header), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	g_signal_connect (
		G_OBJECT (header),
		"size-allocate",
		G_CALLBACK (header_size_allocate_cb),
		NULL);

	return header;
}

static GtkWidget *
create_subheader (const gchar *desc)
{
	GtkWidget *subheader;


	subheader = gtk_label_new (desc);
	gtk_label_set_ellipsize (GTK_LABEL (subheader), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (subheader), 0.0, 0.5);
	gtk_widget_modify_fg (
		subheader,
		GTK_STATE_NORMAL,
		& subheader->style->fg [GTK_STATE_INSENSITIVE]);

	return subheader;
}

static void
start_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	open_desktop_item_exec (APPLICATION_TILE_GET_PRIVATE (tile)->desktop_item);
}

static void
help_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	open_desktop_item_help (APPLICATION_TILE_GET_PRIVATE (tile)->desktop_item);
}

static void
user_apps_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	ApplicationTile *this = APPLICATION_TILE (tile);
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	if (priv->is_in_user_list)
		remove_from_user_list (this);
	else
		add_to_user_list (this);

	update_user_list_menu_item (this);
}

static void
add_to_user_list (ApplicationTile *this)
{
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	GSList *app_list;
	gchar  *loc;

	GConfClient *gconf_client;
	GError      *error;


	loc = (gchar *) gnome_desktop_item_get_location (priv->desktop_item);

	app_list = get_slab_gconf_slist (SLAB_USER_SPECIFIED_APPS_KEY);
	app_list = g_slist_append (app_list, loc);

	gconf_client = gconf_client_get_default ();
	error        = NULL;

	gconf_client_set_list (gconf_client, SLAB_USER_SPECIFIED_APPS_KEY, GCONF_VALUE_STRING,
		app_list, &error);

	if (error)
		g_warning (
			"error adding %s to %s [%s]\n",
			loc, SLAB_USER_SPECIFIED_APPS_KEY, error->message);

	priv->is_in_user_list = TRUE;
}

static void
remove_from_user_list (ApplicationTile *this)
{
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	GSList      *app_list;
	GSList      *new_app_list;
	const gchar *loc;
	gint         offset;

	GConfClient *gconf_client;
	GError      *error;

	GSList *node;


	app_list = get_slab_gconf_slist (SLAB_USER_SPECIFIED_APPS_KEY);

	if (! app_list)
		return;

	loc = gnome_desktop_item_get_location (priv->desktop_item);

	new_app_list = NULL;

	for (node = app_list; node; node = node->next) {
		offset = strlen (loc) - strlen ((gchar *) node->data);

		if (offset < 0)
			offset = 0;

		if (strcmp (& loc [offset], (gchar *) node->data))
			new_app_list = g_slist_append (new_app_list, node->data);
	}

	gconf_client = gconf_client_get_default ();
	error = NULL;

	gconf_client_set_list (gconf_client, SLAB_USER_SPECIFIED_APPS_KEY, GCONF_VALUE_STRING,
		new_app_list, &error);

	if (error)
		g_warning (
			"error removing %s from %s [%s]\n",
			loc, SLAB_USER_SPECIFIED_APPS_KEY, error->message);

	priv->is_in_user_list = FALSE;
}

static void
upgrade_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	run_package_management_command (APPLICATION_TILE (tile), SLAB_UPGRADE_PACKAGE_KEY);
}

static void
uninstall_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	run_package_management_command (APPLICATION_TILE (tile), SLAB_UNINSTALL_PACKAGE_KEY);
}

static gboolean
verify_package_management_command (gchar *gconf_key)
{
	gchar *cmd;
	gchar *path;
	gchar *args;

	gboolean retval;

	cmd = get_slab_gconf_string (gconf_key);

	args = strchr (cmd, ' ');

	if (args)
		*args = '\0';

	path = g_find_program_in_path (cmd);

	retval = (path != NULL);

	g_free (cmd);
	g_free (path);

	return retval;
}

static void
run_package_management_command (ApplicationTile *tile, gchar *gconf_key)
{
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (tile);

	gchar *cmd_precis;
	gchar *package_name;

	GString *cmd;
	gint pivot;
	gchar **argv;

	GError *error = NULL;

	package_name = get_package_name_from_desktop_item (priv->desktop_item);

	if (!package_name)
		return;

	cmd_precis = get_slab_gconf_string (gconf_key);

	g_assert (cmd_precis);

	pivot = strstr (cmd_precis, "PACKAGE_NAME") - cmd_precis;

	cmd = g_string_new_len (cmd_precis, pivot);
	g_string_append (cmd, package_name);
	g_string_append (cmd, & cmd_precis [pivot + 12]);

	argv = g_strsplit (cmd->str, " ", -1);

	g_string_free (cmd, TRUE);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

	if (error) {
		g_warning ("error: [%s]\n", error->message);

		g_error_free (error);
	}

	g_free (cmd_precis);
	g_free (package_name);
	g_strfreev (argv);
}

static void
startup_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	ApplicationTile *this = APPLICATION_TILE (tile);
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	switch (priv->startup_status) {
		case APP_IN_USER_STARTUP_DIR:
			remove_from_startup_list (this);
			break;

		case APP_NOT_IN_STARTUP_DIR:
			add_to_startup_list (this);
			break;

		default:
			break;
	}

	update_startup_menu_item (this);
}

static void
add_to_startup_list (ApplicationTile *this)
{
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	gchar *desktop_item_filename;
	gchar *desktop_item_basename;
	gchar *gconf_startupdir;

	gchar *startup_dir;
	gchar *dst_filename;

	const gchar *src_uri;
	gchar *dst_uri;

	desktop_item_filename =
		g_filename_from_uri (gnome_desktop_item_get_location (priv->desktop_item), NULL,
		NULL);

	g_return_if_fail (desktop_item_filename != NULL);

	desktop_item_basename = g_path_get_basename (desktop_item_filename);

	gconf_startupdir = get_slab_gconf_string (SLAB_USER_STARTUP_DIR_KEY);
	startup_dir = g_build_filename (g_get_home_dir (), gconf_startupdir, NULL);

	if (! g_file_test (startup_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (startup_dir, 0700);

	dst_filename = g_build_filename (startup_dir, desktop_item_basename, NULL);

	src_uri = gnome_desktop_item_get_location (priv->desktop_item);
	dst_uri = g_filename_to_uri (dst_filename, NULL, NULL);

	copy_file (src_uri, dst_uri);
	priv->startup_status = APP_IN_USER_STARTUP_DIR;

	g_free (desktop_item_filename);
	g_free (desktop_item_basename);
	g_free (gconf_startupdir);
	g_free (startup_dir);
	g_free (dst_filename);
	g_free (dst_uri);
}

static void
remove_from_startup_list (ApplicationTile *this)
{
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	gchar *ditem_filename;
	gchar *ditem_basename;
	gchar *src_filename;
	gchar *gconf_startupdir;

	GnomeVFSURI *src_uri;

	GList *list = NULL;

	ditem_filename =
		g_filename_from_uri (gnome_desktop_item_get_location (priv->desktop_item), NULL,
		NULL);

	g_return_if_fail (ditem_filename != NULL);

	ditem_basename = g_path_get_basename (ditem_filename);

	gconf_startupdir = get_slab_gconf_string (SLAB_USER_STARTUP_DIR_KEY);
	src_filename = g_build_filename (g_get_home_dir (), gconf_startupdir, ditem_basename, NULL);

	src_uri = gnome_vfs_uri_new (src_filename);

	list = g_list_append (list, src_uri);

	gnome_vfs_xfer_delete_list (
		list, GNOME_VFS_XFER_ERROR_MODE_ABORT,
		GNOME_VFS_XFER_REMOVESOURCE, NULL, NULL);
	priv->startup_status = APP_NOT_IN_STARTUP_DIR;

	g_free (ditem_filename);
	g_free (ditem_basename);
	g_free (gconf_startupdir);
	g_free (src_filename);
}

GnomeDesktopItem *
application_tile_get_desktop_item (ApplicationTile *tile)
{
	return APPLICATION_TILE_GET_PRIVATE (tile)->desktop_item;
}

static gboolean
is_desktop_item_in_user_list (const gchar *uri)
{
	GSList *app_list;

	GSList *node;
	gint offset;

	app_list = get_slab_gconf_slist (SLAB_USER_SPECIFIED_APPS_KEY);

	if (! app_list)
		return FALSE;

	for (node = app_list; node; node = node->next) {
		offset = strlen (uri) - strlen ((gchar *) node->data);

		if (offset < 0)
			offset = 0;

		if (! strcmp (& uri [offset], (gchar *) node->data))
			return TRUE;
	}

	return FALSE;
}

static void
update_user_list_menu_item (ApplicationTile *this)
{
	TileAction *action = TILE (this)->actions [APPLICATION_TILE_ACTION_UPDATE_MAIN_MENU];
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	if (!action)
		return;

	if (priv->is_in_user_list)
		tile_action_set_menu_item_label (action, _("Remove from Favorites"));
	else
		tile_action_set_menu_item_label (action, _("Add to Favorites"));
}

static StartupStatus
get_desktop_item_startup_status (GnomeDesktopItem *desktop_item)
{
	gchar *filename;
	gchar *basename;
	gchar *gconf_startupdir;

	gchar *global_target;
	gchar *user_target;

	StartupStatus retval;

	filename = g_filename_from_uri (gnome_desktop_item_get_location (desktop_item), NULL, NULL);

	if (!filename)
		return APP_NOT_ELIGIBLE;

	basename = g_path_get_basename (filename);

	gconf_startupdir = get_slab_gconf_string (SLAB_GLOBAL_STARTUP_DIR_KEY);
	global_target = g_build_filename (gconf_startupdir, basename, NULL);
	g_free (gconf_startupdir);

	gconf_startupdir = get_slab_gconf_string (SLAB_USER_STARTUP_DIR_KEY);
	user_target = g_build_filename (g_get_home_dir (), gconf_startupdir, basename, NULL);

	if (g_file_test (global_target, G_FILE_TEST_EXISTS))
		retval = APP_NOT_ELIGIBLE;
	else if (g_file_test (user_target, G_FILE_TEST_EXISTS))
		retval = APP_IN_USER_STARTUP_DIR;
	else
		retval = APP_NOT_IN_STARTUP_DIR;

	g_free (user_target);
	g_free (global_target);
	g_free (gconf_startupdir);
	g_free (basename);
	g_free (filename);

	return retval;
}

static void
update_startup_menu_item (ApplicationTile *this)
{
	TileAction *action = TILE (this)->actions [APPLICATION_TILE_ACTION_UPDATE_STARTUP];
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	if (!action)
		return;

	if (priv->startup_status == APP_IN_USER_STARTUP_DIR)
		tile_action_set_menu_item_label (action, _("Remove from Startup Programs"));
	else
		tile_action_set_menu_item_label (action, _("Add to Startup Programs"));
}

static void
header_size_allocate_cb (GtkWidget *widget, GtkAllocation *alloc, gpointer user_data)
{
	gtk_widget_set_size_request (widget, alloc->width, -1);
}

static void
gconf_user_list_change_cb (GConfClient *gconf_client, guint c_id, GConfEntry *entry,
	gpointer user_data)
{
	ApplicationTile *this = APPLICATION_TILE (user_data);
	ApplicationTilePrivate *priv = APPLICATION_TILE_GET_PRIVATE (this);

	gboolean is_in_user_list_current;

	is_in_user_list_current = is_desktop_item_in_user_list (TILE (this)->uri);

	if (is_in_user_list_current == priv->is_in_user_list)
		return;

	priv->is_in_user_list = is_in_user_list_current;

	update_user_list_menu_item (this);
}
