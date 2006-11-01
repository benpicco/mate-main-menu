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

#include "recent-files.h"

#include <gtk/gtkversion.h>
#if GTK_CHECK_VERSION (2, 10, 0)
#	define USE_GTK_RECENT_MANAGER

#	include <gtk/gtkrecentmanager.h>
#	include <libgnomevfs/gnome-vfs-ops.h>
#	include <libgnomevfs/gnome-vfs-utils.h>
#	include <libgnomevfs/gnome-vfs-monitor.h>
#else
#	define EGG_ENABLE_RECENT_FILES
#	include "egg-recent-model.h"
#endif

G_DEFINE_TYPE (MainMenuRecentMonitor, main_menu_recent_monitor, G_TYPE_OBJECT)
G_DEFINE_TYPE (MainMenuRecentFile,    main_menu_recent_file,    G_TYPE_OBJECT)

#define MAIN_MENU_RECENT_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_RECENT_MONITOR_TYPE, MainMenuRecentMonitorPrivate))
#define MAIN_MENU_RECENT_FILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_RECENT_FILE_TYPE, MainMenuRecentFilePrivate))

typedef struct {
#ifdef USE_GTK_RECENT_MANAGER
	GnomeVFSMonitorHandle *monitor_handle;
#else
	EggRecentModel *model;
#endif

	gulong changed_handler_id;
} MainMenuRecentMonitorPrivate;

typedef struct {
#ifdef USE_GTK_RECENT_MANAGER
	GtkRecentInfo *item_obj;
#else
	EggRecentItem *item_obj;

	gchar  *uri;
	gchar  *mime_type;
#endif
} MainMenuRecentFilePrivate;

enum {
	STORE_CHANGED,
	LAST_SIGNAL
};

static guint monitor_signals [LAST_SIGNAL] = { 0 };

static void main_menu_recent_monitor_finalize (GObject *);
static void main_menu_recent_file_finalize    (GObject *);

static GList *get_files (MainMenuRecentMonitor *, gboolean);

#ifdef USE_GTK_RECENT_MANAGER
static void recent_file_store_monitor_cb (
	GnomeVFSMonitorHandle *, const gchar *,
	const gchar *, GnomeVFSMonitorEventType, gpointer);

static gint recent_item_mru_comp_func (gconstpointer, gconstpointer);
#else
static void recent_file_store_changed_cb (EggRecentModel *, GList *, gpointer);
#endif

static void
main_menu_recent_monitor_class_init (MainMenuRecentMonitorClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = main_menu_recent_monitor_finalize;

	this_class->changed = NULL;

	g_type_class_add_private (this_class, sizeof (MainMenuRecentMonitorPrivate));

	monitor_signals [STORE_CHANGED] = g_signal_new (
		"changed", G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (MainMenuRecentMonitorClass, changed),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
main_menu_recent_file_class_init (MainMenuRecentFileClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = main_menu_recent_file_finalize;

	g_type_class_add_private (this_class, sizeof (MainMenuRecentFilePrivate));
}

static void
main_menu_recent_monitor_init (MainMenuRecentMonitor *this)
{
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (this);

#ifndef USE_GTK_RECENT_MANAGER
	priv->model = NULL;
#endif

	priv->changed_handler_id = 0;
}

static void
main_menu_recent_file_init (MainMenuRecentFile *this)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (this);

	priv->item_obj = NULL;

#ifndef USE_GTK_RECENT_MANAGER
	priv->uri       = NULL;
	priv->mime_type = NULL;
#endif
}

static void
main_menu_recent_monitor_finalize (GObject *g_object)
{
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (g_object);

#ifdef USE_GTK_RECENT_MANAGER
	gnome_vfs_monitor_cancel (priv->monitor_handle);
#else
	g_signal_handler_disconnect (priv->model, priv->changed_handler_id);

	g_object_unref (priv->model);
#endif

	(* G_OBJECT_CLASS (main_menu_recent_monitor_parent_class)->finalize) (g_object);
}

static void
main_menu_recent_file_finalize (GObject *g_object)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (g_object);

#ifdef USE_GTK_RECENT_MANAGER
	if (priv->item_obj)
		gtk_recent_info_unref (priv->item_obj);
#else
	if (priv->item_obj)
		egg_recent_item_unref (priv->item_obj);

	g_free (priv->uri);
	g_free (priv->mime_type);
#endif

	(* G_OBJECT_CLASS (main_menu_recent_file_parent_class)->finalize) (g_object);
}

MainMenuRecentMonitor *
main_menu_recent_monitor_new ()
{
	MainMenuRecentMonitor *this = g_object_new (MAIN_MENU_RECENT_MONITOR_TYPE, NULL);
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (this);

#ifdef USE_GTK_RECENT_MANAGER
	GtkRecentManager *manager;
	gchar *store_path;
	gchar *store_uri;


	manager = gtk_recent_manager_get_default ();
	g_object_get (G_OBJECT (manager), "filename", & store_path, NULL);
	store_uri = gnome_vfs_get_uri_from_local_path (store_path);

	gnome_vfs_monitor_add (
		& priv->monitor_handle, store_uri, GNOME_VFS_MONITOR_FILE,
		recent_file_store_monitor_cb, this);

	g_free (store_path);
	g_free (store_uri);

#else
	priv->model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_NONE);

	priv->changed_handler_id = g_signal_connect (
		priv->model, "changed", G_CALLBACK (recent_file_store_changed_cb), this);
#endif

	return this;
}

static GList *
get_files (MainMenuRecentMonitor *this, gboolean apps)
{
	GList *list;
	GList *items;

#ifdef USE_GTK_RECENT_MANAGER
	GtkRecentManager *manager = gtk_recent_manager_get_default ();

	GtkRecentInfo *info;

	gboolean include;
#else
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (this);
#endif

	MainMenuRecentFile *item;
	MainMenuRecentFilePrivate *item_priv;

	GList *node;


#ifdef USE_GTK_RECENT_MANAGER
	list = gtk_recent_manager_get_items (manager);
#else
	egg_recent_model_set_sort (priv->model, EGG_RECENT_MODEL_SORT_MRU);

	if (apps)
		egg_recent_model_set_filter_groups (priv->model, "recently-used-apps", NULL);
	else
		egg_recent_model_set_filter_groups (priv->model, NULL);

	list = egg_recent_model_get_list (priv->model);
#endif

	items = NULL;

	for (node = list; node; node = node->next) {
		item = g_object_new (MAIN_MENU_RECENT_FILE_TYPE, NULL);
		item_priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (item);

#ifdef USE_GTK_RECENT_MANAGER
		info = (GtkRecentInfo *) node->data;

		include = (apps && gtk_recent_info_has_group (info, "recently-used-apps")) ||
			(! apps && ! gtk_recent_info_get_private_hint (info));

		if (include) {
			item_priv->item_obj = info;

			items = g_list_insert_sorted (items, item, recent_item_mru_comp_func);
		}
		else
			g_object_unref (item);

#else
		item_priv->item_obj = (EggRecentItem *) node->data;

		items = g_list_append (items, item);
#endif
	}

	g_list_free (list);

	return items;
}

GList *
main_menu_get_recent_files (MainMenuRecentMonitor *this)
{
	return get_files (this, FALSE);
}


GList *
main_menu_get_recent_apps (MainMenuRecentMonitor *this)
{
	return get_files (this, TRUE);
}

const gchar *
main_menu_recent_file_get_uri (MainMenuRecentFile *this)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (this);

#ifdef USE_GTK_RECENT_MANAGER
	return gtk_recent_info_get_uri (priv->item_obj);
#else
	if (! priv->uri)
		priv->uri = egg_recent_item_get_uri (priv->item_obj);

	return priv->uri;
#endif
}

const gchar *
main_menu_recent_file_get_mime_type (MainMenuRecentFile *this)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (this);

#ifdef USE_GTK_RECENT_MANAGER
	return gtk_recent_info_get_mime_type (priv->item_obj);
#else
	if (! priv->mime_type)
		priv->mime_type = egg_recent_item_get_mime_type (priv->item_obj);

	return priv->mime_type;
#endif
}

time_t
main_menu_recent_file_get_modified (MainMenuRecentFile *this)
{
	MainMenuRecentFilePrivate *priv = MAIN_MENU_RECENT_FILE_GET_PRIVATE (this);

#ifdef USE_GTK_RECENT_MANAGER
	return gtk_recent_info_get_modified (priv->item_obj);
#else
	return egg_recent_item_get_timestamp (priv->item_obj);
#endif
}

void
main_menu_rename_recent_file (MainMenuRecentMonitor *this, const gchar *uri_0, const gchar *uri_1)
{
#ifdef USE_GTK_RECENT_MANAGER
	GtkRecentManager *manager;

	GError *error = NULL;


	manager = gtk_recent_manager_get_default ();

	gtk_recent_manager_move_item (manager, uri_0, uri_1, & error);

	if (error) {
		g_warning ("unable to update recent file store with renamed file, [%s] -> [%s]\n", uri_0, uri_1);

		g_error_free (error);
	}
#else
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (this);

	egg_recent_model_delete (priv->model, uri_0);
	egg_recent_model_add    (priv->model, uri_1);
#endif
}

void
main_menu_remove_recent_file (MainMenuRecentMonitor *this, const gchar *uri)
{
#ifdef USE_GTK_RECENT_MANAGER
	GtkRecentManager *manager;

	GError *error = NULL;


	manager = gtk_recent_manager_get_default ();

	gtk_recent_manager_remove_item (manager, uri, & error);

	if (error) {
		g_warning ("unable to update recent file store with removed file, [%s]\n", uri);

		g_error_free (error);
	}
#else
	MainMenuRecentMonitorPrivate *priv = MAIN_MENU_RECENT_MONITOR_GET_PRIVATE (this);

	egg_recent_model_delete (priv->model, uri);
#endif
}

#ifdef USE_GTK_RECENT_MANAGER
static void
recent_file_store_monitor_cb (
	GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	g_signal_emit ((MainMenuRecentMonitor *) user_data, monitor_signals [STORE_CHANGED], 0);
}
#else

static void
recent_file_store_changed_cb (EggRecentModel *model, GList *list, gpointer user_data)
{
	g_signal_emit ((MainMenuRecentMonitor *) user_data, monitor_signals [STORE_CHANGED], 0);
}

#endif

#ifdef USE_GTK_RECENT_MANAGER
static gint
recent_item_mru_comp_func (gconstpointer a, gconstpointer b)
{
	time_t modified_a, modified_b;


	modified_a = main_menu_recent_file_get_modified ((MainMenuRecentFile *) a);
	modified_b = main_menu_recent_file_get_modified ((MainMenuRecentFile *) b);

	return modified_b - modified_a;
}
#endif
