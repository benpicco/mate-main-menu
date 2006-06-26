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

#include "main-menu-conf.h"

#include <gconf/gconf-client.h>
#include <string.h>

#include <glib/gprintf.h>

#include "slab-gnome-util.h"

#define MAIN_MENU_GCONF_DIR    "/desktop/gnome/applications/main-menu"
#define FILE_AREA_GCONF_DIR    "/desktop/gnome/applications/main-menu/file-area"
#define SYSTEM_AREA_GCONF_DIR  "/desktop/gnome/applications/main-menu/system-area"
#define LOCK_DOWN_GCONF_DIR    "/desktop/gnome/applications/main-menu/lock-down"

#define URGENT_CLOSE_KEY     MAIN_MENU_GCONF_DIR   "/urgent_close"
#define SEARCH_COMMAND_KEY   MAIN_MENU_GCONF_DIR   "/search_command"
#define APP_BROWSER_KEY      MAIN_MENU_GCONF_DIR   "/application_browser"
#define FILE_BROWSER_KEY     MAIN_MENU_GCONF_DIR   "/file_browser"
#define FILE_CLASS_KEY       FILE_AREA_GCONF_DIR   "/file_class"
#define FILE_REORDERING_KEY  FILE_AREA_GCONF_DIR   "/file_reordering"
#define APP_LIST_KEY         FILE_AREA_GCONF_DIR   "/user_specified_apps"
#define ITEM_LIMIT_KEY       FILE_AREA_GCONF_DIR   "/item_limit"
#define SYSTEM_LIST_KEY      SYSTEM_AREA_GCONF_DIR "/system_item_list"
#define HELP_ITEM_KEY        SYSTEM_AREA_GCONF_DIR "/help_item"
#define CTRL_CTR_ITEM_KEY    SYSTEM_AREA_GCONF_DIR "/control_center_item"
#define PKG_MGR_ITEM_KEY     SYSTEM_AREA_GCONF_DIR "/package_manager_item"
#define AB_LINK_VISIBLE_KEY  LOCK_DOWN_GCONF_DIR   "/application_browser_link_visible"
#define SEARCH_VISIBLE_KEY   LOCK_DOWN_GCONF_DIR   "/search_area_visible"
#define SHOWABLE_FILES_KEY   LOCK_DOWN_GCONF_DIR   "/showable_file_types"
#define STATUS_VISIBLE_KEY   LOCK_DOWN_GCONF_DIR   "/status_area_visible"
#define SYSTEM_VISIBLE_KEY   LOCK_DOWN_GCONF_DIR   "/system_area_visible"

#define VERBOSE 0

typedef struct {
	GConfClient *gconf_client;
} MainMenuConfPrivate;

#define MAIN_MENU_CONF_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_CONF_TYPE, MainMenuConfPrivate))

static void main_menu_conf_class_init (MainMenuConfClass *);
static void main_menu_conf_init (MainMenuConf *);
static void main_menu_conf_dispose (GObject *);

static void main_menu_conf_change_cb (GConfClient *, guint, GConfEntry *,
				      gpointer);

static GConfValue *load_gconf_key (MainMenuConf *, const gchar *);
static gboolean load_boolean_gconf_key (MainMenuConf *, const gchar *);
static gboolean load_int_gconf_key (MainMenuConf *, const gchar *);
static gchar *load_string_gconf_key (MainMenuConf *, const gchar *);
static GList *load_string_list_gconf_key (MainMenuConf *, const gchar *);
static GList *load_int_list_gconf_key (MainMenuConf *, const gchar *);

static GList *convert_gconf_list_to_glist (GConfValue *);
static GSList *convert_glist_to_gconf_list (GList *);
static void string_glist_free (GList *);

enum {
	FILE_LIST_CHANGED,
	LAST_SIGNAL
};

static guint main_menu_conf_signals [LAST_SIGNAL] = { 0 };

GType
main_menu_conf_get_type (void)
{
	static GType object_type = 0;

	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (MainMenuConfClass),
			NULL,
			NULL,
			(GClassInitFunc) main_menu_conf_class_init,
			NULL,
			NULL,
			sizeof (MainMenuConf),
			0,
			(GInstanceInitFunc) main_menu_conf_init
		};

		object_type = g_type_register_static (G_TYPE_OBJECT,
						      "MainMenuConf",
						      &object_info, 0);
	}

	return object_type;
}

static void
main_menu_conf_class_init (MainMenuConfClass * main_menu_conf_class)
{
	GObjectClass *g_obj_class = (GObjectClass *) main_menu_conf_class;


	main_menu_conf_class->file_list_changed = NULL;

	g_obj_class->dispose = main_menu_conf_dispose;

	main_menu_conf_signals [FILE_LIST_CHANGED] =
		g_signal_new ("file-list-changed",
			      G_TYPE_FROM_CLASS (main_menu_conf_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (MainMenuConfClass,
					       file_list_changed), NULL, NULL,
			      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	g_type_class_add_private (main_menu_conf_class,
				  sizeof (MainMenuConfPrivate));
}

static void
main_menu_conf_init (MainMenuConf * conf)
{
	MainMenuConfPrivate *priv = MAIN_MENU_CONF_GET_PRIVATE (conf);

	GList *showable_file_types;
	GList *node;
	gchar *temp_key;

	int type;


	priv->gconf_client = gconf_client_get_default ();

	conf->urgent_close = load_boolean_gconf_key (conf, URGENT_CLOSE_KEY);
	conf->search_command =
		load_string_gconf_key (conf, SEARCH_COMMAND_KEY);
	temp_key = load_string_gconf_key (conf, APP_BROWSER_KEY);
	conf->app_browser = load_desktop_item_from_unknown (temp_key);
	g_free (temp_key);
	temp_key = load_string_gconf_key (conf, FILE_BROWSER_KEY);
	conf->file_browser = load_desktop_item_from_unknown (temp_key);
	g_free (temp_key);

	conf->file_area_conf = g_new0 (FileAreaConf, 1);
	conf->file_area_conf->file_class =
		(FileClass) load_int_gconf_key (conf, FILE_CLASS_KEY);
	conf->file_area_conf->reordering_priority =
		(FileReorderingPriority) load_int_gconf_key (conf,
							     FILE_REORDERING_KEY);
	conf->file_area_conf->user_specified_apps =
		load_string_list_gconf_key (conf, APP_LIST_KEY);
	conf->file_area_conf->item_limit =
		load_int_gconf_key (conf, ITEM_LIMIT_KEY);

	conf->system_area_conf = g_new0 (SystemAreaConf, 1);
	conf->system_area_conf->system_list =
		load_int_list_gconf_key (conf, SYSTEM_LIST_KEY);
	conf->system_area_conf->help_item =
		load_string_gconf_key (conf, HELP_ITEM_KEY);
	conf->system_area_conf->control_center_item =
		load_string_gconf_key (conf, CTRL_CTR_ITEM_KEY);
	conf->system_area_conf->package_manager_item =
		load_string_gconf_key (conf, PKG_MGR_ITEM_KEY);

	conf->lock_down_conf = g_new0 (LockDownConf, 1);
	conf->lock_down_conf->search_area_visible =
		load_boolean_gconf_key (conf, SEARCH_VISIBLE_KEY);
	conf->lock_down_conf->system_area_visible =
		load_boolean_gconf_key (conf, SYSTEM_VISIBLE_KEY);
	conf->lock_down_conf->status_area_visible =
		load_boolean_gconf_key (conf, STATUS_VISIBLE_KEY);
	conf->lock_down_conf->ab_link_visible =
		load_boolean_gconf_key (conf, AB_LINK_VISIBLE_KEY);

	showable_file_types =
		load_int_list_gconf_key (conf, SHOWABLE_FILES_KEY);

	conf->lock_down_conf->file_area_showable_types [USER_SPECIFIED_APPS] =
		FALSE;
	conf->lock_down_conf->file_area_showable_types [RECENTLY_USED_APPS] =
		FALSE;
	conf->lock_down_conf->file_area_showable_types [RECENT_FILES] = FALSE;

	for (node = showable_file_types; node; node = node->next) {
		type = GPOINTER_TO_INT (node->data);

		if (0 <= type && type < FILE_CLASS_SENTINEL)
			conf->lock_down_conf->file_area_showable_types [type] =
				TRUE;
	}
}

MainMenuConf *
main_menu_conf_new ()
{
	return g_object_new (MAIN_MENU_CONF_TYPE, NULL);
}

static void
main_menu_conf_dispose (GObject * obj)
{
	/* FIXME */
}

GList *
main_menu_conf_find_user_app_by_uri (MainMenuConf * conf, const gchar * uri)
{
	g_assert (conf != NULL);

	return g_list_find_custom (conf->file_area_conf->user_specified_apps,
				   uri, desktop_item_location_compare);
}

#if VERBOSE
static void
dump_glist (GList * list)
{
	GList *node;
	gint i;


	g_printf ("main-menu-conf.c\n");

	for (node = list, i = 0; node; node = node->next, ++i)
		g_printf ("dnode  [%d] =  [%s]\n", i, (gchar *) node->data);

	g_printf ("\n");
}

static void
dump_gslist (GSList * list)
{
	GSList *node;
	gint i;


	g_printf ("main-menu-conf.c\n");

	for (node = list, i = 0; node; node = node->next, ++i)
		g_printf ("snode  [%d] =  [%s]\n", i, (gchar *) node->data);

	g_printf ("\n");
}
#endif

void
main_menu_conf_sync (MainMenuConf * conf, gpointer field)
{
	MainMenuConfPrivate *priv = MAIN_MENU_CONF_GET_PRIVATE (conf);

	GSList *gconf_list;
	GError *error = NULL;


	if (&conf->urgent_close == field) {
		gconf_client_set_bool (priv->gconf_client, URGENT_CLOSE_KEY,
				       conf->urgent_close, &error);

		if (error) {
			g_warning ("error setting %s\n", URGENT_CLOSE_KEY);

			g_error_free (error);
		}

		return;
	}

	if (&conf->file_area_conf->file_class == field) {
		gconf_client_set_int (priv->gconf_client,
				      FILE_CLASS_KEY,
				      conf->file_area_conf->file_class,
				      &error);

		if (error) {
			g_warning ("error setting %s\n", FILE_CLASS_KEY);

			g_error_free (error);
		}

		return;
	}

	if (conf->file_area_conf->user_specified_apps == field) {
		gconf_list =
			convert_glist_to_gconf_list (conf->file_area_conf->
						     user_specified_apps);

		gconf_client_set_list (priv->gconf_client, APP_LIST_KEY,
				       GCONF_VALUE_STRING, gconf_list,
				       &error);

		if (error) {
			g_warning ("error setting %s\n", APP_LIST_KEY);

			g_error_free (error);
		}

		return;
	}

#if VERBOSE
	main_menu_conf_dump (conf);
#endif
}

static gboolean
load_boolean_gconf_key (MainMenuConf * conf, const gchar * key)
{
	GConfValue *value;
	gboolean retval;


	value = load_gconf_key (conf, key);
	retval = gconf_value_get_bool (value);

	gconf_value_free (value);

	return retval;
}

static gint
load_int_gconf_key (MainMenuConf * conf, const gchar * key)
{
	GConfValue *value;
	gint retval;


	value = load_gconf_key (conf, key);
	retval = gconf_value_get_int (value);

	gconf_value_free (value);

	return retval;
}

static gchar *
load_string_gconf_key (MainMenuConf * conf, const gchar * key)
{
	GConfValue *value;
	gchar *retval;


	value = load_gconf_key (conf, key);
	retval = g_strdup (gconf_value_get_string (value));

	gconf_value_free (value);

	return retval;
}

static GList *
load_string_list_gconf_key (MainMenuConf * conf, const gchar * key)
{
	GList *list = NULL;

	GConfValue *value, *loop_value;
	gchar *str;

	GSList *node;


	value = load_gconf_key (conf, key);

	for (node = gconf_value_get_list (value); node; node = node->next) {
		loop_value = (GConfValue *) node->data;
		str = g_strdup (gconf_value_get_string (loop_value));

		list = g_list_append (list, str);
	}

	gconf_value_free (value);

	return list;
}

static GList *
load_int_list_gconf_key (MainMenuConf * conf, const gchar * key)
{
	GList *list = NULL;

	GConfValue *value, *loop_value;
	int x;

	GSList *node;


	value = load_gconf_key (conf, key);

	for (node = gconf_value_get_list (value); node; node = node->next) {
		loop_value = (GConfValue *) node->data;
		x = gconf_value_get_int (loop_value);

		list = g_list_append (list, GINT_TO_POINTER (x));
	}

	gconf_value_free (value);

	return list;
}

static GConfValue *
load_gconf_key (MainMenuConf * conf, const gchar * key)
{
	MainMenuConfPrivate *priv = MAIN_MENU_CONF_GET_PRIVATE (conf);

	GConfValue *value;
	GError *error = NULL;


	value = gconf_client_get (priv->gconf_client, key, &error);

	if (error) {
		g_warning ("error accessing %s  [%s]\n", key, error->message);

		g_error_free (error);
		error = NULL;
	}

	gconf_client_notify_add (priv->gconf_client, key,
				 main_menu_conf_change_cb, conf, NULL,
				 &error);

	if (error) {
		g_warning ("error monitoring %s  [%s]\n", key, error->message);

		g_error_free (error);
	}

	return value;
}

static void
main_menu_conf_change_cb (GConfClient * gconf_client, guint c_id,
			  GConfEntry * entry, gpointer user_data)
{
	MainMenuConf *conf = MAIN_MENU_CONF (user_data);

	const gchar *key;
	GConfValue *value;


	key = gconf_entry_get_key (entry);
	value = gconf_entry_get_value (entry);

	if (!strcmp (key, URGENT_CLOSE_KEY))
		conf->urgent_close = gconf_value_get_bool (value);

	else if (!strcmp (key, APP_BROWSER_KEY))
		conf->app_browser =
			load_desktop_item_from_unknown (gconf_value_get_string
							(value));

	else if (!strcmp (key, FILE_BROWSER_KEY))
		conf->app_browser =
			load_desktop_item_from_unknown (gconf_value_get_string
							(value));

	else if (!strcmp (key, SEARCH_COMMAND_KEY))
		conf->search_command =
			g_strdup (gconf_value_get_string (value));

	else if (!strcmp (key, FILE_CLASS_KEY))
		conf->file_area_conf->file_class =
			(FileClass) gconf_value_get_int (value);

	else if (!strcmp (key, FILE_REORDERING_KEY))
		conf->file_area_conf->reordering_priority =
			(FileReorderingPriority) gconf_value_get_int (value);

	else if (!strcmp (key, APP_LIST_KEY)) {
		string_glist_free (conf->file_area_conf->user_specified_apps);

		conf->file_area_conf->user_specified_apps =
			convert_gconf_list_to_glist (value);

		g_signal_emit (conf,
			       main_menu_conf_signals [FILE_LIST_CHANGED], 0);
	}

	else;
}

static GList *
convert_gconf_list_to_glist (GConfValue * gconf_list)
{
	GList *list = NULL;

	GConfValue *value;
	gchar *str;

	GSList *node;


	for (node = gconf_value_get_list (gconf_list); node;
	     node = node->next) {
		value = (GConfValue *) node->data;
		str = g_strdup (gconf_value_get_string (value));

		list = g_list_append (list, str);
	}

	return list;
}

static GSList *
convert_glist_to_gconf_list (GList * list)
{
	GSList *gconf_list = NULL;
	GList *node;


	for (node = list; node; node = node->next)
		gconf_list = g_slist_append (gconf_list, node->data);

	return gconf_list;
}

static void
string_glist_free (GList * list)
{
	GList *node;


	if (!list)
		return;

	for (node = list; node; node = node->next)
		g_free (node->data);

	g_list_free (list);
}

void
main_menu_conf_dump (MainMenuConf * conf)
{
	GList *node;


	g_printf ("MainMenuConf:                 %p\n", conf);
	g_printf ("  urgent_close:               %d\n", conf->urgent_close);
	g_printf ("  search_command:             %s\n", conf->search_command);
	g_printf ("  app_browser:                %s\n",
		  gnome_desktop_item_get_location (conf->app_browser));
	g_printf ("  file_browser:               %s\n",
		  gnome_desktop_item_get_location (conf->file_browser));
	g_printf ("  file_area_conf:             %p\n", conf->file_area_conf);
	g_printf ("    file_class:               %d\n",
		  conf->file_area_conf->file_class);
	g_printf ("    reordering_priority:      %d\n",
		  conf->file_area_conf->reordering_priority);
	g_printf ("    user_specified_apps:      %p\n",
		  conf->file_area_conf->user_specified_apps);

	for (node = conf->file_area_conf->user_specified_apps; node;
	     node = node->next)
		g_printf ("       [%s]\n", (gchar *) node->data);

	g_printf ("  lock_down_conf:             %p\n", conf->lock_down_conf);
	g_printf ("    search_area_visible:      %d\n",
		  conf->lock_down_conf->search_area_visible);
	g_printf ("    ab_link_visible:          %d\n",
		  conf->lock_down_conf->ab_link_visible);
	g_printf ("    system_area_visible:      %d\n",
		  conf->lock_down_conf->system_area_visible);
	g_printf ("    status_area_visible:      %d\n",
		  conf->lock_down_conf->status_area_visible);
	g_printf ("    file_area_showable_types:  [%d,%d,%d]\n",
		  conf->lock_down_conf->
		  file_area_showable_types [USER_SPECIFIED_APPS],
		  conf->lock_down_conf->
		  file_area_showable_types [RECENTLY_USED_APPS],
		  conf->lock_down_conf->
		  file_area_showable_types [RECENT_FILES]);

	g_printf ("\n");
}
