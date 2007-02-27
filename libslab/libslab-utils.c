#include "libslab-utils.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <string.h>
#include <gconf/gconf-value.h>
#include <libgnome/gnome-url.h>

#define GLOBAL_XDG_PATH_ENV_VAR  "XDG_DATA_DIRS"
#define DEFAULT_GLOBAL_XDG_PATH  "/usr/local/share:/usr/share"
#define DEFAULT_USER_XDG_DIR     ".local/share"
#define TOP_CONFIG_DIR           PACKAGE

#define SYSTEM_BOOKMARK_FILENAME "system-items.xbel"
#define APPS_BOOKMARK_FILENAME   "applications.xbel"
#define DOCS_BOOKMARK_FILENAME   "documents.xbel"
#define DIRS_BOOKMARK_FILENAME   "places.xbel"

#define DISABLE_TERMINAL_GCONF_KEY  "/desktop/gnome/lockdown/disable_command_line"
#define MODIFIABLE_SYSTEM_GCONF_KEY "/desktop/gnome/applications/main-menu/lock-down/user_modifiable_system_area"
#define MODIFIABLE_APPS_GCONF_KEY   "/desktop/gnome/applications/main-menu/lock-down/user_modifiable_apps"
#define MODIFIABLE_DOCS_GCONF_KEY   "/desktop/gnome/applications/main-menu/lock-down/user_modifiable_docs"
#define MODIFIABLE_DIRS_GCONF_KEY   "/desktop/gnome/applications/main-menu/lock-down/user_modifiable_dirs"

#define DESKTOP_ITEM_TERMINAL_EMULATOR_FLAG "TerminalEmulator"

static gchar                 *get_data_file_path     (const gchar *, gboolean);
static gboolean               store_has_uri          (const gchar *, const gchar *);
static GList                 *get_uri_list           (const gchar *);
static void                   save_uri_list          (const gchar *, const GList *);
static void                   remove_bookmark_item   (const gchar *, const gchar *, const gchar *);
static GnomeVFSMonitorHandle *add_store_file_monitor (const gchar *,
                                                      GnomeVFSMonitorCallback,
                                                      gpointer);

#define ALTERNATE_DOCPATH_KEY "DocPath"

gboolean
libslab_gtk_image_set_by_id (GtkImage *image, const gchar *id)
{
	GdkPixbuf *pixbuf;

	gint size;
	gint width;
	gint height;

	GtkIconTheme *icon_theme;

	gboolean found;

	gchar *tmp;


	if (! id)
		return FALSE;

	g_object_get (G_OBJECT (image), "icon-size", & size, NULL);

	if (size == GTK_ICON_SIZE_INVALID)
		size = GTK_ICON_SIZE_DND;

	gtk_icon_size_lookup (size, & width, & height);

	if (g_path_is_absolute (id)) {
		pixbuf = gdk_pixbuf_new_from_file_at_size (id, width, height, NULL);

		found = (pixbuf != NULL);

		if (found) {
			gtk_image_set_from_pixbuf (image, pixbuf);

			g_object_unref (pixbuf);
		}
		else
			gtk_image_set_from_stock (image, "gtk-missing-image", size);
	}
	else {
		tmp = g_strdup (id);

		if ( /* file extensions are not copesetic with loading by "name" */
			g_str_has_suffix (tmp, ".png") ||
			g_str_has_suffix (tmp, ".svg") ||
			g_str_has_suffix (tmp, ".xpm")
		)

			tmp [strlen (tmp) - 4] = '\0';

		if (gtk_widget_has_screen (GTK_WIDGET (image)))
			icon_theme = gtk_icon_theme_get_for_screen (
				gtk_widget_get_screen (GTK_WIDGET (image)));
		else
			icon_theme = gtk_icon_theme_get_default ();

		found = gtk_icon_theme_has_icon (icon_theme, tmp);

		if (found)
			gtk_image_set_from_icon_name (image, tmp, size);
		else
			gtk_image_set_from_stock (image, "gtk-missing-image", size);

		g_free (tmp);
	}

	return found;
}

GnomeDesktopItem *
libslab_gnome_desktop_item_new_from_unknown_id (const gchar *id)
{
	GnomeDesktopItem *item;

	GError *error = NULL;


	if (! id)
		return NULL;

	item = gnome_desktop_item_new_from_uri (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	item = gnome_desktop_item_new_from_file (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	item = gnome_desktop_item_new_from_basename (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	return NULL;
}

gboolean
libslab_gnome_desktop_item_launch_default (GnomeDesktopItem *item)
{
	GError *error = NULL;

	if (! item)
		return FALSE;

	gnome_desktop_item_launch (item, NULL, GNOME_DESKTOP_ITEM_LAUNCH_ONLY_ONE, & error);

	if (error) {
		g_warning ("error launching %s [%s]\n",
			gnome_desktop_item_get_location (item), error->message);

		g_error_free (error);

		return FALSE;
	}

	return TRUE;
}

gchar *
libslab_gnome_desktop_item_get_docpath (GnomeDesktopItem *item)
{
	gchar *path;

	path = g_strdup (gnome_desktop_item_get_localestring (item, GNOME_DESKTOP_ITEM_DOC_PATH));

	if (! path)
		path = g_strdup (gnome_desktop_item_get_localestring (item, ALTERNATE_DOCPATH_KEY));

	return path;
}

gboolean
libslab_gnome_desktop_item_open_help (GnomeDesktopItem *item)
{
	gchar *doc_path;
	gchar *help_uri;

	GError *error = NULL;

	gboolean retval = FALSE;


	if (! item)
		return retval;

	doc_path = libslab_gnome_desktop_item_get_docpath (item);

	if (doc_path) {
		help_uri = g_strdup_printf ("ghelp:%s", doc_path);

		gnome_url_show (help_uri, & error);

		if (error) {
			g_warning ("error opening %s [%s]\n", help_uri, error->message);

			g_error_free (error);

			retval = FALSE;
		}
		else
			retval = TRUE;

		g_free (help_uri);
		g_free (doc_path);
	}

	return retval;
}

guint32
libslab_get_current_time_millis ()
{
	GTimeVal t_curr;

	g_get_current_time (& t_curr);

	return 1000L * t_curr.tv_sec + t_curr.tv_usec / 1000L;
}

void
libslab_cclosure_marshal_VOID__POINTER_POINTER (GClosure     *closure,
                                                GValue       *retval,
                                                guint         n_param,
                                                const GValue *param,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data)
{
	libslab_marshal_func_VOID__POINTER_POINTER callback;
	GCClosure *cc = (GCClosure *) closure;
	gpointer data_0, data_1;


	g_return_if_fail (n_param == 3);

	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data_0 = closure->data;
		data_1 = g_value_peek_pointer (param);
	}
	else {
		data_0 = g_value_peek_pointer (param);
		data_1 = closure->data;
	}

	if (marshal_data)
		callback = (libslab_marshal_func_VOID__POINTER_POINTER) marshal_data;
	else
		callback = (libslab_marshal_func_VOID__POINTER_POINTER) cc->callback;

	callback (
		data_0, g_value_peek_pointer (param + 1),
		g_value_peek_pointer (param + 2), data_1);
}

gint
libslab_strcmp (const gchar *a, const gchar *b)
{
	if (! a && ! b)
		return 0;

	if (! a)
		return strcmp ("", b);

	if (! b)
		return strcmp (a, "");

	return strcmp (a, b);
}

gint
libslab_strlen (const gchar *a)
{
	if (! a)
		return 0;

	return strlen (a);
}

gpointer
libslab_get_gconf_value (const gchar *key)
{
	GConfClient *client;
	GConfValue  *value;
	GError      *error = NULL;

	gpointer retval = NULL;

	GList  *list;
	GSList *slist;

	GConfValue *value_i;
	GSList     *node;


	client = gconf_client_get_default ();
	value  = gconf_client_get (client, key, & error);

	if (error || ! value)
		libslab_handle_g_error (& error, "%s: error getting %s", G_STRFUNC, key);
	else {
		switch (value->type) {
			case GCONF_VALUE_STRING:
				retval = (gpointer) g_strdup (gconf_value_get_string (value));
				break;

			case GCONF_VALUE_INT:
				retval = GINT_TO_POINTER (gconf_value_get_int (value));
				break;

			case GCONF_VALUE_BOOL:
				retval = GINT_TO_POINTER (gconf_value_get_bool (value));
				break;

			case GCONF_VALUE_LIST:
				list = NULL;
				slist = gconf_value_get_list (value);

				for (node = slist; node; node = node->next) {
					value_i = (GConfValue *) node->data;

					if (value_i->type == GCONF_VALUE_STRING)
						list = g_list_append (
							list, g_strdup (
								gconf_value_get_string (value_i)));
					else if (value_i->type == GCONF_VALUE_INT)
						list = g_list_append (
							list, GINT_TO_POINTER (
								gconf_value_get_int (value_i)));
					else
						;
				}

				retval = (gpointer) list;

				break;

			default:
				break;
		}
	}

	g_object_unref (client);
	gconf_value_free (value);

	return retval;
}

void
libslab_set_gconf_value (const gchar *key, gconstpointer data)
{
	GConfClient *client;
	GConfValue  *value;

	GConfValueType type;
	GConfValueType list_type;

	GSList *slist = NULL;

	GError *error = NULL;

	GConfValue *value_i;
	GList      *node;


	client = gconf_client_get_default ();
	value  = gconf_client_get (client, key, & error);

	if (error) {
		libslab_handle_g_error (&error, "%s: error getting %s", G_STRFUNC, key);

		goto exit;
	}

	type = value->type;
	list_type = ((type == GCONF_VALUE_LIST) ?
		gconf_value_get_list_type (value) : GCONF_VALUE_INVALID);

	gconf_value_free (value);
	value = gconf_value_new (type);

	if (type == GCONF_VALUE_LIST)
		gconf_value_set_list_type (value, list_type);

	switch (type) {
		case GCONF_VALUE_STRING:
			gconf_value_set_string (value, g_strdup ((gchar *) data));
			break;

		case GCONF_VALUE_INT:
			gconf_value_set_int (value, GPOINTER_TO_INT (data));
			break;

		case GCONF_VALUE_BOOL:
			gconf_value_set_bool (value, GPOINTER_TO_INT (data));
			break;

		case GCONF_VALUE_LIST:
			for (node = (GList *) data; node; node = node->next) {
				value_i = gconf_value_new (list_type);

				if (list_type == GCONF_VALUE_STRING)
					gconf_value_set_string (value_i, (const gchar *) node->data);
				else if (list_type == GCONF_VALUE_INT)
					gconf_value_set_int (value_i, GPOINTER_TO_INT (node->data));
				else
					g_assert_not_reached ();

				slist = g_slist_append (slist, value_i);
			}

			gconf_value_set_list_nocopy (value, slist);

			break;

		default:
			break;
	}

	gconf_client_set (client, key, value, & error);

	if (error)
		libslab_handle_g_error (&error, "%s: error setting %s", G_STRFUNC, key);

exit:

	gconf_value_free (value);
	g_object_unref (client);
}

guint
libslab_gconf_notify_add (const gchar *key, GConfClientNotifyFunc callback, gpointer user_data)
{
	GConfClient *client;
	guint        conn_id;

	GError *error = NULL;


	client  = gconf_client_get_default ();
	conn_id = gconf_client_notify_add (client, key, callback, user_data, NULL, & error);

	if (error)
		libslab_handle_g_error (
			& error, "%s: error adding gconf notify for (%s)", G_STRFUNC, key);

	g_object_unref (client);

	return conn_id;
}

void
libslab_gconf_notify_remove (guint conn_id)
{
	GConfClient *client;

	GError *error = NULL;


	if (conn_id == 0)
		return;

	client = gconf_client_get_default ();
	gconf_client_notify_remove (client, conn_id);

	if (error)
		libslab_handle_g_error (
			& error, "%s: error removing gconf notify", G_STRFUNC);

	g_object_unref (client);
}

void
libslab_handle_g_error (GError **error, const gchar *msg_format, ...)
{
	gchar   *msg;
	va_list  args;


	va_start (args, msg_format);
	msg = g_strdup_vprintf (msg_format, args);
	va_end (args);

	if (*error) {
		g_log (
			G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"\nGError raised: [%s]\nuser_message: [%s]\n", (*error)->message, msg);

		g_error_free (*error);

		*error = NULL;
	}
	else
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "\nerror raised: [%s]\n", msg);

	g_free (msg);
}

GList *
libslab_get_system_item_uris ()
{
	GList *uris;
	gchar *path;


	path = get_data_file_path (SYSTEM_BOOKMARK_FILENAME, FALSE);
	uris = get_uri_list (path);

	g_free (path);

	return uris;
}

GList *
libslab_get_user_app_uris ()
{
	GList *uris;
	gchar *path;


	path = get_data_file_path (APPS_BOOKMARK_FILENAME, FALSE);
	uris = get_uri_list (path);

	g_free (path);

	return uris;
}

GList *
libslab_get_user_doc_uris ()
{
	GList *uris;
	gchar *path;


	path = get_data_file_path (DOCS_BOOKMARK_FILENAME, FALSE);
	uris = get_uri_list (path);

	g_free (path);

	return uris;
}

void
libslab_save_system_item_uris (const GList *uris)
{
	save_uri_list (SYSTEM_BOOKMARK_FILENAME, uris);
}

void
libslab_save_app_uris (const GList *uris)
{
	save_uri_list (APPS_BOOKMARK_FILENAME, uris);
}

void
libslab_save_doc_uris (const GList *uris)
{
	save_uri_list (DOCS_BOOKMARK_FILENAME, uris);
}

GnomeVFSMonitorHandle *
libslab_add_system_item_monitor (GnomeVFSMonitorCallback callback, gpointer user_data)
{
	return add_store_file_monitor (SYSTEM_BOOKMARK_FILENAME, callback, user_data);
}

GnomeVFSMonitorHandle *
libslab_add_apps_monitor (GnomeVFSMonitorCallback callback, gpointer user_data)
{
	return add_store_file_monitor (APPS_BOOKMARK_FILENAME, callback, user_data);
}

GnomeVFSMonitorHandle *
libslab_add_docs_monitor (GnomeVFSMonitorCallback callback, gpointer user_data)
{
	return add_store_file_monitor (DOCS_BOOKMARK_FILENAME, callback, user_data);
}

static GList *
get_uri_list (const gchar *path)
{
#ifdef USE_G_BOOKMARK
	GBookmarkFile   *bm_file;
#else
	EggBookmarkFile *bm_file;
#endif

	gchar **uris_array;
	GList  *uris_list = NULL;

	GError *error = NULL;

	gint i;


	if (! path)
		return NULL;

#ifdef USE_G_BOOKMARK
	bm_file = g_bookmark_file_new ();
	g_bookmark_file_load_from_file (bm_file, path, & error);
#else
	bm_file = egg_bookmark_file_new ();
	egg_bookmark_file_load_from_file (bm_file, path, & error);
#endif

	if (! error) {
#ifdef USE_G_BOOKMARK
		uris_array = g_bookmark_file_get_uris (bm_file, NULL);
#else
		uris_array = egg_bookmark_file_get_uris (bm_file, NULL);
#endif

		for (i = 0; uris_array [i]; ++i)
			uris_list = g_list_append (uris_list, g_strdup (uris_array [i]));
	}
	else
		libslab_handle_g_error (
			& error, "%s: couldn't load bookmark file [%s]", G_STRFUNC, path);

	g_strfreev (uris_array);

#ifdef USE_G_BOOKMARK
	g_bookmark_file_free (bm_file);
#else
	egg_bookmark_file_free (bm_file);
#endif

	return uris_list;
}

gchar *
libslab_get_system_item_store_path (gboolean writeable)
{
	return get_data_file_path (SYSTEM_BOOKMARK_FILENAME, writeable);
}

gchar *
libslab_get_user_apps_store_path (gboolean writeable)
{
	return get_data_file_path (APPS_BOOKMARK_FILENAME, writeable);
}

gchar *
libslab_get_user_docs_store_path (gboolean writeable)
{
	return get_data_file_path (DOCS_BOOKMARK_FILENAME, writeable);
}

gchar *
libslab_get_user_dirs_store_path (gboolean writeable)
{
	return get_data_file_path (DIRS_BOOKMARK_FILENAME, writeable);
}

void
libslab_remove_system_item (const gchar *uri)
{
	gchar *path_old;
	gchar *path_new;


	path_old = libslab_get_system_item_store_path (FALSE);
	path_new = libslab_get_system_item_store_path (TRUE);

	remove_bookmark_item (path_old, path_new, uri);

	g_free (path_new);
	g_free (path_old);
}

void
libslab_remove_user_doc (const gchar *uri)
{
	gchar *path_old;
	gchar *path_new;


	path_old = libslab_get_user_docs_store_path (FALSE);
	path_new = libslab_get_user_docs_store_path (TRUE);

	remove_bookmark_item (path_old, path_new, uri);

	g_free (path_new);
	g_free (path_old);
}

static void
remove_bookmark_item (const gchar *path_old, const gchar *path_new, const gchar *uri)
{
	LibSlabBookmarkFile *bm_file;

	GError *error = NULL;


	bm_file  = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path_old, & error);

	if (! error) {
		libslab_bookmark_file_remove_item (bm_file, uri, NULL);

		libslab_bookmark_file_to_file (bm_file, path_new, & error);

		if (error)
			libslab_handle_g_error (
				& error,
				"%s: couldn't save bookmark file [%s]",
				G_STRFUNC, path_new);
	}
	else
		libslab_handle_g_error (
			& error,
			"%s: couldn't open bookmark file [%s]",
			G_STRFUNC, path_old);

	libslab_bookmark_file_free (bm_file);
}

void
libslab_add_user_doc (const gchar *uri, const gchar *mime_type, time_t modified,
                      const gchar *app_name, const gchar *app_exec)
{
	LibSlabBookmarkFile *bm_file;

	gchar *path_old;
	gchar *path_new;

	GError *error = NULL;


	path_old = libslab_get_user_docs_store_path (FALSE);
	path_new = libslab_get_user_docs_store_path (TRUE);

	bm_file = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path_old, & error);

	if (! error) {
		libslab_bookmark_file_set_mime_type   (bm_file, uri, mime_type);
		libslab_bookmark_file_set_modified    (bm_file, uri, modified);
		libslab_bookmark_file_add_application (bm_file, uri, app_name, app_exec);

		libslab_bookmark_file_to_file (bm_file, path_new, & error);

		if (error)
			libslab_handle_g_error (
				& error,
				"%s: couldn't save bookmark file [%s]",
				G_STRFUNC, path_new);
	}
	else
		libslab_handle_g_error (
			& error,
			"%s: couldn't open bookmark file [%s]",
			G_STRFUNC, path_old);

	libslab_bookmark_file_free (bm_file);
}

gboolean
libslab_system_item_store_has_uri (const gchar *uri)
{
	gchar *path;
	gboolean exists;


	path = libslab_get_system_item_store_path (FALSE);

	exists = store_has_uri (path, uri);

	g_free (path);

	return exists;
}

gboolean
libslab_user_apps_store_has_uri (const gchar *uri)
{
	gchar *path;
	gboolean exists;


	path = libslab_get_user_apps_store_path (FALSE);

	exists = store_has_uri (path, uri);

	g_free (path);

	return exists;
}

gboolean
libslab_user_docs_store_has_uri (const gchar *uri)
{
	gchar *path;
	gboolean exists;


	path = libslab_get_user_docs_store_path (FALSE);

	exists = store_has_uri (path, uri);

	g_free (path);

	return exists;
}

gboolean
libslab_desktop_item_is_a_terminal (const gchar *uri)
{
	GnomeDesktopItem *d_item;
	const gchar      *categories;

	gboolean is_terminal = FALSE;


	d_item = libslab_gnome_desktop_item_new_from_unknown_id (uri);

	if (! d_item)
		return FALSE;

	categories = gnome_desktop_item_get_string (d_item, GNOME_DESKTOP_ITEM_CATEGORIES);

	is_terminal = (categories && strstr (categories, DESKTOP_ITEM_TERMINAL_EMULATOR_FLAG));

	gnome_desktop_item_unref (d_item);

	return is_terminal;
}

gchar *
libslab_string_replace_once (const gchar *string, const gchar *key, const gchar *value)
{
	GString *str_built;
	gint pivot;


	pivot = strstr (string, key) - string;

	str_built = g_string_new_len (string, pivot);
	g_string_append (str_built, value);
	g_string_append (str_built, & string [pivot + strlen (key)]);

	return g_string_free (str_built, FALSE);
}

void
libslab_spawn_command (const gchar *cmd)
{
	gchar **argv;

	GError *error = NULL;


	if (! cmd || strlen (cmd) < 1)
		return;

	argv = g_strsplit (cmd, " ", -1);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, & error);

	if (error)
		libslab_handle_g_error (& error, "%s: error spawning [%s]", G_STRFUNC, cmd);

	g_strfreev (argv);
}

static gboolean
store_has_uri (const gchar *path, const gchar *uri)
{
	LibSlabBookmarkFile *bm_file;

	gboolean exists = FALSE;

	GError *error = NULL;


	bm_file = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path, & error);

	if (! error)
		exists = libslab_bookmark_file_has_item (bm_file, uri);
	else
		libslab_handle_g_error (
			& error,
			"%s: couldn't open bookmark file [%s]",
			G_STRFUNC, path);

	libslab_bookmark_file_free (bm_file);

	return exists;
}

static void
save_uri_list (const gchar *filename, const GList *uris)
{
#ifdef USE_G_BOOKMARK
	GBookmarkFile *bm_file;
#else
	EggBookmarkFile *bm_file;
#endif

	gchar *path;
	gchar *uri;

	const GList *node;

	GError *error = NULL;


	path = get_data_file_path (filename, TRUE);

#ifdef USE_G_BOOKMARK
	bm_file = g_bookmark_file_new ();
#else
	bm_file = egg_bookmark_file_new ();
#endif

	for (node = uris; node; node = node->next) {
		uri = (gchar *) node->data;

#ifdef USE_G_BOOKMARK
		g_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
		g_bookmark_file_add_application (bm_file, uri, NULL, NULL);
#else
		egg_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
		egg_bookmark_file_add_application (bm_file, uri, g_get_prgname (), g_get_prgname ());
#endif
	}

#ifdef USE_G_BOOKMARK
	g_bookmark_file_to_file (bm_file, path, & error);
#else
	egg_bookmark_file_to_file (bm_file, path, & error);
#endif

	if (error)
		libslab_handle_g_error (
			& error, "%s: cannot save uri list [%s]",
			G_STRFUNC, path);

#ifdef USE_G_BOOKMARK
	g_bookmark_file_free (bm_file);
#else
	egg_bookmark_file_free (bm_file);
#endif
}

static gchar *
get_data_file_path (const gchar *filename, gboolean writeable)
{
	gboolean user_modifiable;

	gchar *path;
	gchar *dir;

	const gchar * const *sys_dirs;

	gint i;


	if (! strcmp (filename, SYSTEM_BOOKMARK_FILENAME))
		user_modifiable = GPOINTER_TO_INT (
			libslab_get_gconf_value (MODIFIABLE_SYSTEM_GCONF_KEY));
	else if (! strcmp (filename, APPS_BOOKMARK_FILENAME))
		user_modifiable = GPOINTER_TO_INT (
			libslab_get_gconf_value (MODIFIABLE_APPS_GCONF_KEY));
	else if (! strcmp (filename, DOCS_BOOKMARK_FILENAME))
		user_modifiable = GPOINTER_TO_INT (
			libslab_get_gconf_value (MODIFIABLE_DOCS_GCONF_KEY));
	else if (! strcmp (filename, DIRS_BOOKMARK_FILENAME))
		user_modifiable = GPOINTER_TO_INT (
			libslab_get_gconf_value (MODIFIABLE_DIRS_GCONF_KEY));
	else
		user_modifiable = TRUE;

	if (! user_modifiable && writeable)
		return NULL;

	if (user_modifiable) {
		path = g_build_filename (g_get_user_data_dir (), PACKAGE, filename, NULL);

		if (writeable) {
			dir = g_path_get_dirname (path);
			g_mkdir_with_parents (dir, 0700);
			g_free (dir);

			return path;
		}

		if (g_file_test (path, G_FILE_TEST_EXISTS))
			return path;
	}

	sys_dirs = g_get_system_data_dirs ();

	for (i = 0; sys_dirs [i]; ++i) {
		path = g_build_filename (sys_dirs [i], PACKAGE, filename, NULL);

		if (g_file_test (path, G_FILE_TEST_EXISTS))
			return path;

		g_free (path);
	}

	return NULL;
}

static GnomeVFSMonitorHandle *
add_store_file_monitor (const gchar *filename, GnomeVFSMonitorCallback callback, gpointer user_data)
{
	GnomeVFSMonitorHandle *handle;
	gchar                 *path;
	gchar                 *uri;


	path = get_data_file_path (filename, FALSE);
	uri  = g_filename_to_uri (path, NULL, NULL);

	gnome_vfs_monitor_add (& handle, uri, GNOME_VFS_MONITOR_FILE, callback, user_data);

	g_free (path);
	g_free (uri);

	return handle;
}
