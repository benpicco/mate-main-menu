/* gbookmarkfile.c: parsing and building desktop bookmarks
 *
 * Copyright (C) 2005-2006 Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 */

#include "config.h"

#include "libslab-bookmarkfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <glib.h>
#include <glib/gi18n.h>

/* XBEL 1.0 standard entities */
#define XBEL_VERSION		"1.0"
#define XBEL_DTD_NICK		"xbel"
#define XBEL_DTD_SYSTEM		"+//IDN python.org//DTD XML Bookmark " \
				"Exchange Language 1.0//EN//XML"

#define XBEL_DTD_URI		"http://www.python.org/topics/xml/dtds/xbel-1.0.dtd"

#define XBEL_ROOT_ELEMENT	"xbel"
#define XBEL_FOLDER_ELEMENT	"folder" 	/* unused */
#define XBEL_BOOKMARK_ELEMENT	"bookmark"
#define XBEL_ALIAS_ELEMENT	"alias"		/* unused */
#define XBEL_SEPARATOR_ELEMENT	"separator" 	/* unused */
#define XBEL_TITLE_ELEMENT	"title"
#define XBEL_DESC_ELEMENT	"desc"
#define XBEL_INFO_ELEMENT	"info"
#define XBEL_METADATA_ELEMENT	"metadata"

#define XBEL_VERSION_ATTRIBUTE	"version"
#define XBEL_FOLDED_ATTRIBUTE	"folded" 	/* unused */
#define XBEL_OWNER_ATTRIBUTE	"owner"
#define XBEL_ADDED_ATTRIBUTE	"added"
#define XBEL_VISITED_ATTRIBUTE	"visited"
#define XBEL_MODIFIED_ATTRIBUTE "modified"
#define XBEL_ID_ATTRIBUTE	"id"
#define XBEL_HREF_ATTRIBUTE	"href"
#define XBEL_REF_ATTRIBUTE	"ref" 		/* unused */

#define XBEL_YES_VALUE		"yes"
#define XBEL_NO_VALUE		"no"

/* Desktop bookmark spec entities */
#define BOOKMARK_METADATA_OWNER 	"http://freedesktop.org"

#define BOOKMARK_NAMESPACE_NAME 	"bookmark"
#define BOOKMARK_NAMESPACE_URI		"http://www.freedesktop.org/standards/desktop-bookmarks"

#define BOOKMARK_GROUPS_ELEMENT		"groups"
#define BOOKMARK_GROUP_ELEMENT		"group"
#define BOOKMARK_APPLICATIONS_ELEMENT	"applications"
#define BOOKMARK_APPLICATION_ELEMENT	"application"
#define BOOKMARK_ICON_ELEMENT 		"icon"
#define BOOKMARK_PRIVATE_ELEMENT	"private"

#define BOOKMARK_NAME_ATTRIBUTE		"name"
#define BOOKMARK_EXEC_ATTRIBUTE		"exec"
#define BOOKMARK_COUNT_ATTRIBUTE 	"count"
#define BOOKMARK_TIMESTAMP_ATTRIBUTE	"timestamp"
#define BOOKMARK_HREF_ATTRIBUTE 	"href"
#define BOOKMARK_TYPE_ATTRIBUTE 	"type"

/* Shared MIME Info entities */
#define MIME_NAMESPACE_NAME 		"mime"
#define MIME_NAMESPACE_URI 		"http://www.freedesktop.org/standards/shared-mime-info"
#define MIME_TYPE_ELEMENT 		"mime-type"
#define MIME_TYPE_ATTRIBUTE 		"type"


typedef struct _BookmarkAppInfo  BookmarkAppInfo;
typedef struct _BookmarkMetadata BookmarkMetadata;
typedef struct _BookmarkItem     BookmarkItem;
typedef struct _ParseData        ParseData;

struct _BookmarkAppInfo
{
  gchar *name;
  gchar *exec;
  
  guint count;
  
  time_t stamp;
};

struct _BookmarkMetadata
{
  gchar *mime_type;
  
  GList *groups;
  
  GList *applications;
  GHashTable *apps_by_name;
  
  gchar *icon_href;
  gchar *icon_mime;
  
  guint is_private : 1;
};

struct _BookmarkItem
{
  gchar *uri;

  gchar *title;
  gchar *description;

  time_t added;
  time_t modified;
  time_t visited;

  BookmarkMetadata *metadata;
};

struct _LibSlabBookmarkFile
{
  gchar *title;
  gchar *description;

  /* we store our items in a list and keep a copy inside
   * an hash table for faster lookup performances
   */
  GList *items;
  GHashTable *items_by_uri;
};

/* parser state machine */
enum
{
  STATE_STARTED        = 0,
  
  STATE_ROOT,
  STATE_BOOKMARK,
  STATE_TITLE,
  STATE_DESC,
  STATE_INFO,
  STATE_METADATA,
  STATE_APPLICATIONS,
  STATE_APPLICATION,
  STATE_GROUPS,
  STATE_GROUP,
  STATE_MIME,
  STATE_ICON,
  
  STATE_FINISHED
};

static void          libslab_bookmark_file_init        (LibSlabBookmarkFile  *bookmark);
static void          libslab_bookmark_file_clear       (LibSlabBookmarkFile  *bookmark);
static gboolean      libslab_bookmark_file_parse       (LibSlabBookmarkFile  *bookmark,
						  const gchar    *buffer,
						  gsize           length,
						  GError        **error);
static gchar *       libslab_bookmark_file_dump        (LibSlabBookmarkFile  *bookmark,
						  gsize          *length,
						  GError        **error);
static BookmarkItem *libslab_bookmark_file_lookup_item (LibSlabBookmarkFile  *bookmark,
						  const gchar    *uri);
static void          libslab_bookmark_file_add_item    (LibSlabBookmarkFile  *bookmark,
						  BookmarkItem   *item,
						  GError        **error);
						       
static time_t  timestamp_from_iso8601 (const gchar *iso_date);
static gchar * timestamp_to_iso8601   (time_t       timestamp);
static time_t mktime_utc (struct tm *tm);
static gboolean libslab_time_val_from_iso8601 (const gchar *iso_date, GTimeVal    *time_);
static gchar * libslab_time_val_to_iso8601  (GTimeVal *time_);

/********************************
 * BookmarkAppInfo              *
 *                              *
 * Application metadata storage *
 ********************************/
static BookmarkAppInfo *
bookmark_app_info_new (const gchar *name)
{
  BookmarkAppInfo *retval;
 
  g_assert (name != NULL);
  
  retval = g_new0 (BookmarkAppInfo, 1);
  
  retval->name = g_strdup (name);
  retval->exec = NULL;
  retval->count = 0;
  retval->stamp = time (NULL);
  
  return retval;
}

static void
bookmark_app_info_free (BookmarkAppInfo *app_info)
{
  if (!app_info)
    return;
  
  if (app_info->name)
    g_free (app_info->name);
  
  if (app_info->exec)
    g_free (app_info->exec);
  
  g_free (app_info);
}

static gchar *
bookmark_app_info_dump (BookmarkAppInfo *app_info)
{
  gchar *retval;
  gchar *name, *exec;

  g_assert (app_info != NULL);

  if (app_info->count == 0)
    return NULL;

  name = g_markup_escape_text (app_info->name, -1);
  exec = g_markup_escape_text (app_info->exec, -1);
 
  retval = g_strdup_printf ("          <%s:%s %s=\"%s\" %s=\"%s\" %s=\"%ld\" %s=\"%u\"/>\n",
                            BOOKMARK_NAMESPACE_NAME,
                            BOOKMARK_APPLICATION_ELEMENT,
                            BOOKMARK_NAME_ATTRIBUTE, name,
                            BOOKMARK_EXEC_ATTRIBUTE, exec,
                            BOOKMARK_TIMESTAMP_ATTRIBUTE, (time_t) app_info->stamp,
                            BOOKMARK_COUNT_ATTRIBUTE, app_info->count);

  g_free (name);
  g_free (exec);

  return retval;
}


/***********************
 * BookmarkMetadata    *
 *                     *
 * Metadata storage    *
 ***********************/
static BookmarkMetadata *
bookmark_metadata_new (void)
{
  BookmarkMetadata *retval;
  
  retval = g_new0 (BookmarkMetadata, 1);

  retval->mime_type = NULL;
  
  retval->groups = NULL;
  
  retval->applications = NULL;
  retval->apps_by_name = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                NULL,
                                                NULL);
  
  retval->is_private = FALSE;
  
  retval->icon_href = NULL;
  retval->icon_mime = NULL;
  
  return retval;
}

static void
bookmark_metadata_free (BookmarkMetadata *metadata)
{
  if (!metadata)
    return;
  
  if (metadata->mime_type)
    g_free (metadata->mime_type);
    
  if (metadata->groups)
    {
      g_list_foreach (metadata->groups,
		      (GFunc) g_free,
		      NULL);
      g_list_free (metadata->groups);
    }
  
  if (metadata->applications)
    {
      g_list_foreach (metadata->applications,
		      (GFunc) bookmark_app_info_free,
		      NULL);
      g_list_free (metadata->applications);
      
      g_hash_table_destroy (metadata->apps_by_name);
    }
  
  if (metadata->icon_href)
    g_free (metadata->icon_href);
  
  if (metadata->icon_mime)
    g_free (metadata->icon_mime);
  
  g_free (metadata);
}

static gchar *
bookmark_metadata_dump (BookmarkMetadata *metadata)
{
  GString *retval;
 
  if (!metadata->applications)
    return NULL;
  
  retval = g_string_new (NULL);
  
  /* metadata container */
  g_string_append_printf (retval,
  			  "      <%s %s=\"%s\">\n",
  			  XBEL_METADATA_ELEMENT,
  			  XBEL_OWNER_ATTRIBUTE, BOOKMARK_METADATA_OWNER);
  
  /* mime type */
  if (metadata->mime_type)
    g_string_append_printf (retval,
  			    "        <%s:%s %s=\"%s\"/>\n",
  			    MIME_NAMESPACE_NAME,
  			    MIME_TYPE_ELEMENT,
  			    MIME_TYPE_ATTRIBUTE, metadata->mime_type);
  
  if (metadata->groups)
    {
      GList *l;
      
      /* open groups container */
      g_string_append_printf (retval,
      			      "        <%s:%s>\n",
      			      BOOKMARK_NAMESPACE_NAME,
      			      BOOKMARK_GROUPS_ELEMENT);
      
      for (l = g_list_last (metadata->groups); l != NULL; l = l->prev)
        {
          gchar *group_name;

	  group_name = g_markup_escape_text ((gchar *) l->data, -1);
          g_string_append_printf (retval,
          			  "          <%s:%s>%s</%s:%s>\n",
          			  BOOKMARK_NAMESPACE_NAME,
          			  BOOKMARK_GROUP_ELEMENT,
          			  group_name,
          			  BOOKMARK_NAMESPACE_NAME,
          			  BOOKMARK_GROUP_ELEMENT);
	  g_free (group_name);
        }
      
      /* close groups container */
      g_string_append_printf (retval,
      			      "        </%s:%s>\n",
      			      BOOKMARK_NAMESPACE_NAME,
      			      BOOKMARK_GROUPS_ELEMENT);
    }
  
  if (metadata->applications)
    {
      GList *l;
      
      /* open applications container */
      g_string_append_printf (retval,
      			      "        <%s:%s>\n",
      			      BOOKMARK_NAMESPACE_NAME,
      			      BOOKMARK_APPLICATIONS_ELEMENT);
      
      for (l = g_list_last (metadata->applications); l != NULL; l = l->prev)
        {
          BookmarkAppInfo *app_info = (BookmarkAppInfo *) l->data;
          gchar *app_data;

	  g_assert (app_info != NULL);
          
          app_data = bookmark_app_info_dump (app_info);

	  if (app_data)
            {
              retval = g_string_append (retval, app_data);

	      g_free (app_data);
	    }
        }
      
      /* close applications container */
      g_string_append_printf (retval,
      			      "        </%s:%s>\n",
      			      BOOKMARK_NAMESPACE_NAME,
      			      BOOKMARK_APPLICATIONS_ELEMENT);
    }
  
  /* icon */
  if (metadata->icon_href)
    {
      if (!metadata->icon_mime)
        metadata->icon_mime = g_strdup ("application/octet-stream");
      
      g_string_append_printf (retval,
      			      "       <%s:%s %s=\"%s\" %s=\"%s\"/>\n",
      			      BOOKMARK_NAMESPACE_NAME,
      			      BOOKMARK_ICON_ELEMENT,
      			      BOOKMARK_HREF_ATTRIBUTE, metadata->icon_href,
      			      BOOKMARK_TYPE_ATTRIBUTE, metadata->icon_mime);
    }
  
  /* private hint */
  if (metadata->is_private)
    g_string_append_printf (retval,
    			    "        <%s:%s/>\n",
    			    BOOKMARK_NAMESPACE_NAME,
    			    BOOKMARK_PRIVATE_ELEMENT);
  
  /* close metadata container */
  g_string_append_printf (retval, "      </%s>\n", XBEL_METADATA_ELEMENT);
  			   
  return g_string_free (retval, FALSE);
}

/******************************************************
 * BookmarkItem                                       *
 *                                                    *
 * Storage for a single bookmark item inside the list *
 ******************************************************/
static BookmarkItem *
bookmark_item_new (const gchar *uri)
{
  BookmarkItem *item;
 
  g_assert (uri != NULL);
  
  item = g_new0 (BookmarkItem, 1);
  item->uri = g_strdup (uri);
  
  item->title = NULL;
  item->description = NULL;
  
  item->added = (time_t) -1;
  item->modified = (time_t) -1;
  item->visited = (time_t) -1;
  
  item->metadata = NULL;
  
  return item;
}

static void
bookmark_item_free (BookmarkItem *item)
{
  if (!item)
    return;

  if (item->uri)
    g_free (item->uri);
  
  if (item->title)
    g_free (item->title);
  
  if (item->description)
    g_free (item->description);
  
  if (item->metadata)
    bookmark_metadata_free (item->metadata);
  
  g_free (item);
}

static gchar *
bookmark_item_dump (BookmarkItem *item)
{
  GString *retval;
  gchar *added, *visited, *modified;
  gchar *escaped_uri;
 
  /* at this point, we must have at least a registered application; if we don't
   * we don't screw up the bookmark file, and just skip this item
   */
  if (!item->metadata || !item->metadata->applications)
    {
      g_warning ("Item for URI '%s' has no registered applications: skipping.\n", item->uri);
      return NULL;
    }
  
  retval = g_string_new (NULL);
  
  added = timestamp_to_iso8601 (item->added);
  modified = timestamp_to_iso8601 (item->modified);
  visited = timestamp_to_iso8601 (item->visited);

  escaped_uri = g_markup_escape_text (item->uri, -1);
  
  g_string_append_printf (retval,
                          "  <%s %s=\"%s\" %s=\"%s\" %s=\"%s\" %s=\"%s\">\n",
                          XBEL_BOOKMARK_ELEMENT,
                          XBEL_HREF_ATTRIBUTE, escaped_uri,
                          XBEL_ADDED_ATTRIBUTE, added,
                          XBEL_MODIFIED_ATTRIBUTE, modified,
                          XBEL_VISITED_ATTRIBUTE, visited);
  g_free (escaped_uri);
  g_free (visited);
  g_free (modified);
  g_free (added);
  
  if (item->title)
    {
      gchar *escaped_title;
      
      escaped_title = g_markup_escape_text (item->title, -1);
      g_string_append_printf (retval,
    			      "    <%s>%s</%s>\n",
    			      XBEL_TITLE_ELEMENT,
    			      escaped_title,
    			      XBEL_TITLE_ELEMENT);
      g_free (escaped_title);
    }
  
  if (item->description)
    {
      gchar *escaped_desc;
      
      escaped_desc = g_markup_escape_text (item->description, -1);
      g_string_append_printf (retval,
    			      "    <%s>%s</%s>\n",
    			      XBEL_DESC_ELEMENT,
    			      escaped_desc,
    			      XBEL_DESC_ELEMENT);
      g_free (escaped_desc);
    }
  
  if (item->metadata)
    {
      gchar *metadata;
      
      /* open info container */
      g_string_append_printf (retval, "    <%s>\n", XBEL_INFO_ELEMENT);
      
      metadata = bookmark_metadata_dump (item->metadata);
      if (metadata)
        {
          retval = g_string_append (retval, metadata);

	  g_free (metadata);
	}
      
      /* close info container */
      g_string_append_printf (retval, "    </%s>\n", XBEL_INFO_ELEMENT);
    }
  
  g_string_append_printf (retval, "  </%s>\n", XBEL_BOOKMARK_ELEMENT);
  
  return g_string_free (retval, FALSE);
}

static BookmarkAppInfo *
bookmark_item_lookup_app_info (BookmarkItem *item,
			       const gchar  *app_name)
{
  g_assert (item != NULL && app_name != NULL);

  if (!item->metadata)
    return NULL;
  
  return g_hash_table_lookup (item->metadata->apps_by_name, app_name);
}

/*************************
 *    LibSlabBookmarkFile    *
 *************************/
 
static void
libslab_bookmark_file_init (LibSlabBookmarkFile *bookmark)
{
  bookmark->title = NULL;
  bookmark->description = NULL;
  
  bookmark->items = NULL;
  bookmark->items_by_uri = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  NULL,
                                                  NULL);
}

static void
libslab_bookmark_file_clear (LibSlabBookmarkFile *bookmark)
{
  g_free (bookmark->title);
  g_free (bookmark->description);

  if (bookmark->items)
    {
      g_list_foreach (bookmark->items,
		      (GFunc) bookmark_item_free,
		      NULL);
      g_list_free (bookmark->items);
      
      bookmark->items = NULL;
    }
  
  if (bookmark->items_by_uri)
    {
      g_hash_table_destroy (bookmark->items_by_uri);
      
      bookmark->items_by_uri = NULL;
    }
}

struct _ParseData
{
  gint state;
  
  GHashTable *namespaces;
  
  LibSlabBookmarkFile *bookmark_file;
  BookmarkItem *current_item;
};

static ParseData *
parse_data_new (void)
{
  ParseData *retval;
  
  retval = g_new (ParseData, 1);
  
  retval->state = STATE_STARTED;
  retval->namespaces = g_hash_table_new_full (g_str_hash, g_str_equal,
  					      (GDestroyNotify) g_free,
  					      (GDestroyNotify) g_free);
  retval->bookmark_file = NULL;
  retval->current_item = NULL;
  
  return retval;
}

static void
parse_data_free (ParseData *parse_data)
{
  g_hash_table_destroy (parse_data->namespaces);
  
  g_free (parse_data);
}

#define IS_ATTRIBUTE(s,a)	((0 == strcmp ((s), (a))))

static void
parse_bookmark_element (GMarkupParseContext  *context,
			ParseData            *parse_data,
			const gchar         **attribute_names,
			const gchar         **attribute_values,
			GError              **error)
{
  const gchar *uri, *added, *modified, *visited;
  const gchar *attr;
  gint i;
  BookmarkItem *item;
  GError *add_error;
 
  g_assert ((parse_data != NULL) && (parse_data->state == STATE_BOOKMARK));
  
  i = 0;
  uri = added = modified = visited = NULL;
  for (attr = attribute_names[i]; attr != NULL; attr = attribute_names[++i])
    {
      if (IS_ATTRIBUTE (attr, XBEL_HREF_ATTRIBUTE))
        uri = attribute_values[i];
      else if (IS_ATTRIBUTE (attr, XBEL_ADDED_ATTRIBUTE))
        added = attribute_values[i];
      else if (IS_ATTRIBUTE (attr, XBEL_MODIFIED_ATTRIBUTE))
        modified = attribute_values[i];
      else if (IS_ATTRIBUTE (attr, XBEL_VISITED_ATTRIBUTE))
        visited = attribute_values[i];
      else
        {
          g_set_error (error, G_MARKUP_ERROR,
		       G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
          	       _("Unexpected attribute '%s' for element '%s'"),
          	       attr,
          	       XBEL_BOOKMARK_ELEMENT);
          return;
        }
    }
  
  if (!uri)
    {
      g_set_error (error, G_MARKUP_ERROR,
      		   G_MARKUP_ERROR_INVALID_CONTENT,
      		   _("Attribute '%s' of element '%s' not found"),
      		   XBEL_HREF_ATTRIBUTE,
      		   XBEL_BOOKMARK_ELEMENT);
      return;
    }
  
  g_assert (parse_data->current_item == NULL);
  
  item = bookmark_item_new (uri);
  
  if (added)
    item->added = timestamp_from_iso8601 (added);
  
  if (modified)
    item->modified = timestamp_from_iso8601 (modified);
  
  if (visited)
    item->visited = timestamp_from_iso8601 (visited);

  add_error = NULL;
  libslab_bookmark_file_add_item (parse_data->bookmark_file,
  			      item,
  			      &add_error);
  if (add_error)
    {
      bookmark_item_free (item);
      
      g_propagate_error (error, add_error);
      
      return;
    }  			      
  
  parse_data->current_item = item;
}

static void
parse_application_element (GMarkupParseContext  *context,
			   ParseData            *parse_data,
			   const gchar         **attribute_names,
			   const gchar         **attribute_values,
			   GError              **error)
{
  const gchar *name, *exec, *count, *stamp;
  const gchar *attr;
  gint i;
  BookmarkItem *item;
  BookmarkAppInfo *ai;
  
  g_assert ((parse_data != NULL) && (parse_data->state == STATE_APPLICATION));

  i = 0;
  name = exec = count = stamp = NULL;
  for (attr = attribute_names[i]; attr != NULL; attr = attribute_names[++i])
    {
      if (IS_ATTRIBUTE (attr, BOOKMARK_NAME_ATTRIBUTE))
        name = attribute_values[i];
      else if (IS_ATTRIBUTE (attr, BOOKMARK_EXEC_ATTRIBUTE))
        exec = attribute_values[i];
      else if (IS_ATTRIBUTE (attr, BOOKMARK_COUNT_ATTRIBUTE))
        count = attribute_values[i];
      else if (IS_ATTRIBUTE (attr, BOOKMARK_TIMESTAMP_ATTRIBUTE))
        stamp = attribute_values[i];
      else
        {
          g_set_error (error, G_MARKUP_ERROR,
		       G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
          	       _("Unexpected attribute '%s' for element '%s'"),
          	       attr,
          	       BOOKMARK_APPLICATION_ELEMENT);
          return;
        }        
    }

  if (!name)
    {
      g_set_error (error, G_MARKUP_ERROR,
      		   G_MARKUP_ERROR_INVALID_CONTENT,
      		   _("Attribute '%s' of element '%s' not found"),
      		   BOOKMARK_NAME_ATTRIBUTE,
      		   BOOKMARK_APPLICATION_ELEMENT);
      return;
    }
  
  if (!exec)
    {
      g_set_error (error, G_MARKUP_ERROR,
      		   G_MARKUP_ERROR_INVALID_CONTENT,
      		   _("Attribute '%s' of element '%s' not found"),
      		   BOOKMARK_EXEC_ATTRIBUTE,
      		   BOOKMARK_APPLICATION_ELEMENT);
      return;
    }

  g_assert (parse_data->current_item != NULL);  
  item = parse_data->current_item;
    
  ai = bookmark_item_lookup_app_info (item, name);
  if (!ai)
    {
      ai = bookmark_app_info_new (name);
      
      if (!item->metadata)
	item->metadata = bookmark_metadata_new ();
      
      item->metadata->applications = g_list_prepend (item->metadata->applications, ai);
      g_hash_table_replace (item->metadata->apps_by_name, ai->name, ai);
    }
      
  ai->exec = g_strdup (exec);
  
  if (count)
    ai->count = atoi (count);
  else
    ai->count = 1;
  
  if (stamp)
    ai->stamp = (time_t) atol (stamp);
  else
    ai->stamp = time (NULL);
}

static void
parse_mime_type_element (GMarkupParseContext  *context,
			 ParseData            *parse_data,
			 const gchar         **attribute_names,
			 const gchar         **attribute_values,
			 GError              **error)
{
  const gchar *type;
  const gchar *attr;
  gint i;
  BookmarkItem *item;
  
  g_assert ((parse_data != NULL) && (parse_data->state == STATE_MIME));
  
  i = 0;
  type = NULL;
  for (attr = attribute_names[i]; attr != NULL; attr = attribute_names[++i])
    {
      if (IS_ATTRIBUTE (attr, MIME_TYPE_ATTRIBUTE))
        type = attribute_values[i];
      else
        {
          g_set_error (error, G_MARKUP_ERROR,
		       G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
          	       _("Unexpected attribute '%s' for element '%s'"),
          	       attr,
          	       MIME_TYPE_ELEMENT);
          return;
        }        
    }

  if (!type)
    type = "application/octet-stream";

  g_assert (parse_data->current_item != NULL);  
  item = parse_data->current_item;
    
  if (!item->metadata)
    item->metadata = bookmark_metadata_new ();
  
  item->metadata->mime_type = g_strdup (type);
}

static void
parse_icon_element (GMarkupParseContext  *context,
		    ParseData            *parse_data,
		    const gchar         **attribute_names,
		    const gchar         **attribute_values,
		    GError              **error)
{
  const gchar *href;
  const gchar *type;
  const gchar *attr;
  gint i;
  BookmarkItem *item;
  
  g_assert ((parse_data != NULL) && (parse_data->state == STATE_ICON));
  
  i = 0;
  href = NULL;
  type = NULL;
  for (attr = attribute_names[i]; attr != NULL; attr = attribute_names[++i])
    {
      if (IS_ATTRIBUTE (attr, BOOKMARK_HREF_ATTRIBUTE))
        href = attribute_values[i];
      else if (IS_ATTRIBUTE (attr, BOOKMARK_TYPE_ATTRIBUTE))
        type = attribute_values[i];
      else
        {
          g_set_error (error, G_MARKUP_ERROR,
		       G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
          	       _("Unexpected attribute '%s' for element '%s'"),
          	       attr,
          	       BOOKMARK_ICON_ELEMENT);
          return;
        }        
    }

  if (!href)
    {
      g_set_error (error, G_MARKUP_ERROR,
      		   G_MARKUP_ERROR_INVALID_CONTENT,
      		   _("Attribute '%s' of element '%s' not found"),
      		   BOOKMARK_HREF_ATTRIBUTE,
      		   BOOKMARK_ICON_ELEMENT);
      return;
    }

  if (!type)
    type = "application/octet-stream";

  g_assert (parse_data->current_item != NULL);  
  item = parse_data->current_item;
    
  if (!item->metadata)
    item->metadata = bookmark_metadata_new ();
  
  item->metadata->icon_href = g_strdup (href);
  item->metadata->icon_mime = g_strdup (type);
}

/* scans through the attributes of an element for the "xmlns" pragma, and
 * adds any resulting namespace declaration to a per-parser hashtable, using
 * the namespace name as a key for the namespace URI; if no key was found,
 * the namespace is considered as default, and stored under the "default" key.
 *
 * FIXME: this works on the assumption that the generator of the XBEL file
 * is either this code or is smart enough to place the namespace declarations
 * inside the main root node or inside the metadata node and does not redefine 
 * a namespace inside an inner node; this does *not* conform to the
 * XML-NS standard, although is a close approximation.  In order to make this
 * conformant to the XML-NS specification we should use a per-element
 * namespace table inside GMarkup and ask it to resolve the namespaces for us.
 */
static void
map_namespace_to_name (ParseData    *parse_data,
                       const gchar **attribute_names,
		       const gchar **attribute_values)
{
  const gchar *attr;
  gint i;
 
  g_assert (parse_data != NULL);
  
  if (!attribute_names || !attribute_names[0])
    return;
  
  i = 0;
  for (attr = attribute_names[i]; attr; attr = attribute_names[++i])
    {
      if (g_str_has_prefix (attr, "xmlns"))
        {
          gchar *namespace_name, *namespace_uri;
          gchar *p;
          
          p = g_utf8_strchr (attr, -1, ':');
          if (p)
            p = g_utf8_next_char (p);
          else
            p = "default";
          
          namespace_name = g_strdup (p);
          namespace_uri = g_strdup (attribute_values[i]);
          
          g_hash_table_replace (parse_data->namespaces,
                                namespace_name,
                                namespace_uri);
        }
     }
}

/* checks whether @element_full is equal to @element.
 *
 * if @namespace is set, it tries to resolve the namespace to a known URI,
 * and if found is prepended to the element name, from which is separated
 * using the character specified in the @sep parameter.
 */
static gboolean
is_element_full (ParseData   *parse_data,
                 const gchar *element_full,
                 const gchar *namespace,
                 const gchar *element,
                 const gchar  sep)
{
  gchar *ns_uri, *ns_name, *s, *resolved;
  const gchar *p, *element_name;
  gboolean retval;
 
  g_assert (parse_data != NULL);
  g_assert (element_full != NULL);
  
  if (!element)
    return FALSE;
    
  /* no namespace requested: dumb element compare */
  if (!namespace)
    return (0 == strcmp (element_full, element));
  
  /* search for namespace separator; if none found, assume we are under the
   * default namespace, and set ns_name to our "default" marker; if no default
   * namespace has been set, just do a plain comparison between @full_element
   * and @element.
   */
  p = strchr (element_full, ':');
  if (p)
    {
      ns_name = g_strndup (element_full, p - element_full);
      element_name = g_utf8_next_char (p);
    }
  else
    {
      ns_name = g_strdup ("default");
      element_name = element_full;
    }
  
  ns_uri = g_hash_table_lookup (parse_data->namespaces, ns_name);  
  if (!ns_uri)
    {
      /* no default namespace found */
      g_free (ns_name);
      
      return (0 == strcmp (element_full, element));
    }
  
  resolved = g_strdup_printf ("%s%c%s", ns_uri, sep, element_name);
  s = g_strdup_printf ("%s%c%s", namespace, sep, element);
  retval = (0 == strcmp (resolved, s));
  
  g_free (ns_name);
  g_free (resolved);
  g_free (s);
  
  return retval;
}

#define IS_ELEMENT(p,s,e)	(is_element_full ((p), (s), NULL, (e), '\0'))
#define IS_ELEMENT_NS(p,s,n,e)	(is_element_full ((p), (s), (n), (e), '|'))

static void
start_element_raw_cb (GMarkupParseContext *context,
                      const gchar         *element_name,
                      const gchar        **attribute_names,
                      const gchar        **attribute_values,
                      gpointer             user_data,
                      GError             **error)
{
  ParseData *parse_data = (ParseData *) user_data;

  /* we must check for namespace declarations first
   * 
   * XXX - we could speed up things by checking for namespace declarations
   * only on the root node, where they usually are; this would probably break
   * on streams not produced by us or by "smart" generators
   */
  map_namespace_to_name (parse_data, attribute_names, attribute_values);
  
  switch (parse_data->state)
    {
    case STATE_STARTED:
      if (IS_ELEMENT (parse_data, element_name, XBEL_ROOT_ELEMENT))
        {
          const gchar *attr;
          gint i;
          
          i = 0;
          for (attr = attribute_names[i]; attr; attr = attribute_names[++i])
            {
              if ((IS_ATTRIBUTE (attr, XBEL_VERSION_ATTRIBUTE)) &&
                  (0 == strcmp (attribute_values[i], XBEL_VERSION)))
                parse_data->state = STATE_ROOT;
            }
	}
      else
        g_set_error (error, G_MARKUP_ERROR,
		     G_MARKUP_ERROR_INVALID_CONTENT,
          	     _("Unexpected tag '%s', tag '%s' expected"),
          	     element_name, XBEL_ROOT_ELEMENT);
      break;
    case STATE_ROOT:
      if (IS_ELEMENT (parse_data, element_name, XBEL_TITLE_ELEMENT))
        parse_data->state = STATE_TITLE;
      else if (IS_ELEMENT (parse_data, element_name, XBEL_DESC_ELEMENT))
        parse_data->state = STATE_DESC;
      else if (IS_ELEMENT (parse_data, element_name, XBEL_BOOKMARK_ELEMENT))
        {
          GError *inner_error = NULL;
          
          parse_data->state = STATE_BOOKMARK;
          
          parse_bookmark_element (context,
          			  parse_data,
          			  attribute_names,
          			  attribute_values,
          			  &inner_error);
          if (inner_error)
            g_propagate_error (error, inner_error);
        }
      else
        g_set_error (error, G_MARKUP_ERROR,
        	     G_MARKUP_ERROR_INVALID_CONTENT,
        	     _("Unexpected tag '%s' inside '%s'"),
        	     element_name,
        	     XBEL_ROOT_ELEMENT);
      break;
    case STATE_BOOKMARK:
      if (IS_ELEMENT (parse_data, element_name, XBEL_TITLE_ELEMENT))
        parse_data->state = STATE_TITLE;
      else if (IS_ELEMENT (parse_data, element_name, XBEL_DESC_ELEMENT))
        parse_data->state = STATE_DESC;
      else if (IS_ELEMENT (parse_data, element_name, XBEL_INFO_ELEMENT))
        parse_data->state = STATE_INFO;
      else
        g_set_error (error, G_MARKUP_ERROR,
        	     G_MARKUP_ERROR_INVALID_CONTENT,
          	     _("Unexpected tag '%s' inside '%s'"),
          	     element_name,
          	     XBEL_BOOKMARK_ELEMENT);
      break;
    case STATE_INFO:
      if (IS_ELEMENT (parse_data, element_name, XBEL_METADATA_ELEMENT))
        {
          const gchar *attr;
          gint i;
          
          i = 0;
          for (attr = attribute_names[i]; attr; attr = attribute_names[++i])
            {
              if ((IS_ATTRIBUTE (attr, XBEL_OWNER_ATTRIBUTE)) &&
                  (0 == strcmp (attribute_values[i], BOOKMARK_METADATA_OWNER)))
                {
                  parse_data->state = STATE_METADATA;
                  
                  if (!parse_data->current_item->metadata)
                    parse_data->current_item->metadata = bookmark_metadata_new ();
                }
            }
        }
      else
        g_set_error (error, G_MARKUP_ERROR,
        	     G_MARKUP_ERROR_INVALID_CONTENT,
        	     _("Unexpected tag '%s', tag '%s' expected"),
        	     element_name,
        	     XBEL_METADATA_ELEMENT);
      break;
    case STATE_METADATA:
      if (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_APPLICATIONS_ELEMENT))
        parse_data->state = STATE_APPLICATIONS;
      else if (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_GROUPS_ELEMENT))
        parse_data->state = STATE_GROUPS;
      else if (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_PRIVATE_ELEMENT))
        parse_data->current_item->metadata->is_private = TRUE;
      else if (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_ICON_ELEMENT))
        {
          GError *inner_error = NULL;
          
	  parse_data->state = STATE_ICON;
          
          parse_icon_element (context,
          		      parse_data,
          		      attribute_names,
          		      attribute_values,
          		      &inner_error);
          if (inner_error)
            g_propagate_error (error, inner_error);
        }
      else if (IS_ELEMENT_NS (parse_data, element_name, MIME_NAMESPACE_URI, MIME_TYPE_ELEMENT))
        {
          GError *inner_error = NULL;
          
          parse_data->state = STATE_MIME;
          
          parse_mime_type_element (context,
          			   parse_data,
          			   attribute_names,
          			   attribute_values,
          			   &inner_error);
          if (inner_error)
            g_propagate_error (error, inner_error);
        }
      else
        g_set_error (error, G_MARKUP_ERROR,
        	     G_MARKUP_ERROR_UNKNOWN_ELEMENT,
        	     _("Unexpected tag '%s' inside '%s'"),
        	     element_name,
        	     XBEL_METADATA_ELEMENT);
      break;
    case STATE_APPLICATIONS:
      if (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_APPLICATION_ELEMENT))
        {
          GError *inner_error = NULL;
          
          parse_data->state = STATE_APPLICATION;
          
          parse_application_element (context,
          			     parse_data,
          			     attribute_names,
          			     attribute_values,
          			     &inner_error);
          if (inner_error)
            g_propagate_error (error, inner_error);
        }
      else
        g_set_error (error, G_MARKUP_ERROR,
        	     G_MARKUP_ERROR_INVALID_CONTENT,
        	     _("Unexpected tag '%s', tag '%s' expected"),
        	     element_name,
        	     BOOKMARK_APPLICATION_ELEMENT);
      break;
    case STATE_GROUPS:
      if (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_GROUP_ELEMENT))
        parse_data->state = STATE_GROUP;
      else
        g_set_error (error, G_MARKUP_ERROR,
        	     G_MARKUP_ERROR_INVALID_CONTENT,
        	     _("Unexpected tag '%s', tag '%s' expected"),
        	     element_name,
        	     BOOKMARK_GROUP_ELEMENT);
      break;
    case STATE_ICON:
      if (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_ICON_ELEMENT))
        {
          GError *inner_error = NULL;
          
          parse_icon_element (context,
          		      parse_data,
          		      attribute_names,
          		      attribute_values,
          		      &inner_error);
          if (inner_error)
            g_propagate_error (error, inner_error);
        }
      else
        g_set_error (error, G_MARKUP_ERROR,
        	     G_MARKUP_ERROR_UNKNOWN_ELEMENT,
        	     _("Unexpected tag '%s' inside '%s'"),
        	     element_name,
        	     XBEL_METADATA_ELEMENT);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
end_element_raw_cb (GMarkupParseContext *context,
                    const gchar         *element_name,
                    gpointer             user_data,
                    GError             **error)
{
  ParseData *parse_data = (ParseData *) user_data;
  
  if (IS_ELEMENT (parse_data, element_name, XBEL_ROOT_ELEMENT))
    parse_data->state = STATE_FINISHED;
  else if (IS_ELEMENT (parse_data, element_name, XBEL_BOOKMARK_ELEMENT))
    {
      parse_data->current_item = NULL;
      
      parse_data->state = STATE_ROOT;
    }
  else if ((IS_ELEMENT (parse_data, element_name, XBEL_INFO_ELEMENT)) ||
           (IS_ELEMENT (parse_data, element_name, XBEL_TITLE_ELEMENT)) ||
           (IS_ELEMENT (parse_data, element_name, XBEL_DESC_ELEMENT)))
    {
      if (parse_data->current_item)
        parse_data->state = STATE_BOOKMARK;
      else
        parse_data->state = STATE_ROOT;
    }
  else if (IS_ELEMENT (parse_data, element_name, XBEL_METADATA_ELEMENT))
    parse_data->state = STATE_INFO;
  else if (IS_ELEMENT_NS (parse_data, element_name,
                          BOOKMARK_NAMESPACE_URI,
                          BOOKMARK_APPLICATION_ELEMENT))
    parse_data->state = STATE_APPLICATIONS;
  else if (IS_ELEMENT_NS (parse_data, element_name,
                          BOOKMARK_NAMESPACE_URI,
                          BOOKMARK_GROUP_ELEMENT))
    parse_data->state = STATE_GROUPS;
  else if ((IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_APPLICATIONS_ELEMENT)) ||
           (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_GROUPS_ELEMENT)) ||
           (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_PRIVATE_ELEMENT)) ||
           (IS_ELEMENT_NS (parse_data, element_name, BOOKMARK_NAMESPACE_URI, BOOKMARK_ICON_ELEMENT)) ||
           (IS_ELEMENT_NS (parse_data, element_name, MIME_NAMESPACE_URI, MIME_TYPE_ELEMENT)))
    parse_data->state = STATE_METADATA;
}

static void
text_raw_cb (GMarkupParseContext *context,
             const gchar         *text,
             gsize                length,
             gpointer             user_data,
             GError             **error)
{
  ParseData *parse_data = (ParseData *) user_data;
  gchar *payload;
  
  payload = g_strndup (text, length);
  
  switch (parse_data->state)
    {
    case STATE_TITLE:
      if (parse_data->current_item)
        {
          g_free (parse_data->current_item->title);
          parse_data->current_item->title = g_strdup (payload);
        }
      else
        {
          g_free (parse_data->bookmark_file->title);
          parse_data->bookmark_file->title = g_strdup (payload);
        }
      break;
    case STATE_DESC:
      if (parse_data->current_item)
        {
          g_free (parse_data->current_item->description);
          parse_data->current_item->description = g_strdup (payload);
        }
      else
        {
          g_free (parse_data->bookmark_file->description);
          parse_data->bookmark_file->description = g_strdup (payload);
        }
      break;
    case STATE_GROUP:
      {
      GList *groups;
      
      g_assert (parse_data->current_item != NULL);
      
      if (!parse_data->current_item->metadata)
        parse_data->current_item->metadata = bookmark_metadata_new ();
      
      groups = parse_data->current_item->metadata->groups;
      parse_data->current_item->metadata->groups = g_list_prepend (groups, g_strdup (payload));
      }
      break;
    case STATE_ROOT:
    case STATE_BOOKMARK:
    case STATE_INFO:
    case STATE_METADATA:
    case STATE_APPLICATIONS:
    case STATE_APPLICATION:
    case STATE_GROUPS:
    case STATE_MIME:
    case STATE_ICON:
      break;
    default:
      g_assert_not_reached ();
      break;
    }
  
  g_free (payload);
}

static const GMarkupParser markup_parser =
{
  start_element_raw_cb, /* start_element */
  end_element_raw_cb,   /* end_element */
  text_raw_cb,          /* text */
  NULL,                 /* passthrough */
  NULL
};

static gboolean
libslab_bookmark_file_parse (LibSlabBookmarkFile  *bookmark,
			 const gchar      *buffer,
			 gsize             length,
			 GError          **error)
{
  GMarkupParseContext *context;
  ParseData *parse_data;
  GError *parse_error, *end_error;
  gboolean retval;
  
  g_assert (bookmark != NULL);

  if (!buffer)
    return FALSE;
  
  if (length == -1)
    length = strlen (buffer);

  parse_data = parse_data_new ();
  parse_data->bookmark_file = bookmark;
  
  context = g_markup_parse_context_new (&markup_parser,
  					0,
  					parse_data,
  					(GDestroyNotify) parse_data_free);
  
  parse_error = NULL;
  retval = g_markup_parse_context_parse (context,
  					 buffer,
  					 length,
  					 &parse_error);
  if (!retval)
    {
      g_propagate_error (error, parse_error);
      
      return FALSE;
    }
  
  end_error = NULL;
  retval = g_markup_parse_context_end_parse (context, &end_error);
  if (!retval)
    {
      g_propagate_error (error, end_error);
      
      return FALSE;
    }
  
  g_markup_parse_context_free (context);
  
  return TRUE;
}

static gchar *
libslab_bookmark_file_dump (LibSlabBookmarkFile  *bookmark,
		      gsize          *length,
		      GError        **error)
{
  GString *retval;
  GList *l;
  
  retval = g_string_new (NULL);
  
  g_string_append_printf (retval,
  			  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
#if 0
			  /* XXX - do we really need the doctype? */
  			  "<!DOCTYPE %s\n"
  			  "  PUBLIC \"%s\"\n"
  			  "         \"%s\">\n"
#endif
  			  "<%s %s=\"%s\"\n"
  			  "      xmlns:%s=\"%s\"\n"
  			  "      xmlns:%s=\"%s\"\n>",
#if 0
			  /* XXX - do we really need the doctype? */
  			  XBEL_DTD_NICK,
  			  XBEL_DTD_SYSTEM, XBEL_DTD_URI,
#endif
  			  XBEL_ROOT_ELEMENT,
  			  XBEL_VERSION_ATTRIBUTE, XBEL_VERSION,
  			  BOOKMARK_NAMESPACE_NAME, BOOKMARK_NAMESPACE_URI,
  			  MIME_NAMESPACE_NAME, MIME_NAMESPACE_URI);
  
  if (bookmark->title)
    {
      gchar *escaped_title;
      
      escaped_title = g_markup_escape_text (bookmark->title, -1);
      
      g_string_append_printf (retval, "  <%s>%s</%s>\n",
                              XBEL_TITLE_ELEMENT,
                              escaped_title,
                              XBEL_TITLE_ELEMENT);
      
      g_free (escaped_title);
    }
  
  if (bookmark->description)
    {
      gchar *escaped_desc;
      
      escaped_desc = g_markup_escape_text (bookmark->description, -1);
      
      g_string_append_printf (retval, "  <%s>%s</%s>\n",
                              XBEL_DESC_ELEMENT,
                              escaped_desc,
                              XBEL_DESC_ELEMENT);
      
      g_free (escaped_desc);
    }
  
  if (!bookmark->items)
    goto out;
  else
    retval = g_string_append (retval, "\n");
  
  for (l = g_list_last (bookmark->items);
       l != NULL;
       l = l->prev)
    {
      BookmarkItem *item = (BookmarkItem *) l->data;
      gchar *item_dump;
      
      item_dump = bookmark_item_dump (item);
      if (!item_dump)
        continue;
      
      retval = g_string_append (retval, item_dump);
      
      g_free (item_dump);      
    }

out:
  g_string_append_printf (retval, "</%s>", XBEL_ROOT_ELEMENT);
  
  if (length)
    *length = retval->len;
  
  return g_string_free (retval, FALSE);
}

/**************
 *    Misc    *
 **************/
 
/* converts a Unix timestamp in a ISO 8601 compliant string; you
 * should free the returned string.
 */
static gchar *
timestamp_to_iso8601 (time_t timestamp)
{
  GTimeVal stamp;

  if (timestamp == (time_t) -1)
    g_get_current_time (&stamp);
  else
    {
      stamp.tv_sec = timestamp;
      stamp.tv_usec = 0;
    }

  return libslab_time_val_to_iso8601 (&stamp);
}

static time_t
timestamp_from_iso8601 (const gchar *iso_date)
{
  GTimeVal stamp;

  if (!libslab_time_val_from_iso8601 (iso_date, &stamp))
    return (time_t) -1;

  return (time_t) stamp.tv_sec;
}

/* converts a broken down date representation, relative to UTC, to
 * a timestamp; it uses timegm() if it's available.
 */
static time_t
mktime_utc (struct tm *tm)
{
  time_t retval;
  
#ifndef HAVE_TIMEGM
  static const gint days_before[] =
  {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };
#endif

#ifndef HAVE_TIMEGM
  if (tm->tm_mon < 0 || tm->tm_mon > 11)
    return (time_t) -1;

  retval = (tm->tm_year - 70) * 365;
  retval += (tm->tm_year - 68) / 4;
  retval += days_before[tm->tm_mon] + tm->tm_mday - 1;
  
  if (tm->tm_year % 4 == 0 && tm->tm_mon < 2)
    retval -= 1;
  
  retval = ((((retval * 24) + tm->tm_hour) * 60) + tm->tm_min) * 60 + tm->tm_sec;
#else
  retval = timegm (tm);
#endif /* !HAVE_TIMEGM */
  
  return retval;
}

/**
 * g_time_val_from_iso8601:
 * @iso_date: a ISO 8601 encoded date string
 * @time_: a #GTimeVal
 *
 * Converts a string containing an ISO 8601 encoded date and time
 * to a #GTimeVal and puts it into @time_.
 *
 * Return value: %TRUE if the conversion was successful.
 *
 * Since: 2.12
 */
static gboolean
libslab_time_val_from_iso8601 (const gchar *iso_date,
			 GTimeVal    *time_)
{
  struct tm tm;
  long val;

  g_return_val_if_fail (iso_date != NULL, FALSE);
  g_return_val_if_fail (time_ != NULL, FALSE);

  val = strtoul (iso_date, (char **)&iso_date, 10);
  if (*iso_date == '-')
    {
      /* YYYY-MM-DD */
      tm.tm_year = val - 1900;
      iso_date++;
      tm.tm_mon = strtoul (iso_date, (char **)&iso_date, 10) - 1;
      
      if (*iso_date++ != '-')
       	return FALSE;
      
      tm.tm_mday = strtoul (iso_date, (char **)&iso_date, 10);
    }
  else
    {
      /* YYYYMMDD */
      tm.tm_mday = val % 100;
      tm.tm_mon = (val % 10000) / 100 - 1;
      tm.tm_year = val / 10000 - 1900;
    }

  if (*iso_date++ != 'T')
    return FALSE;
  
  val = strtoul (iso_date, (char **)&iso_date, 10);
  if (*iso_date == ':')
    {
      /* hh:mm:ss */
      tm.tm_hour = val;
      iso_date++;
      tm.tm_min = strtoul (iso_date, (char **)&iso_date, 10);
      
      if (*iso_date++ != ':')
        return FALSE;
      
      tm.tm_sec = strtoul (iso_date, (char **)&iso_date, 10);
    }
  else
    {
      /* hhmmss */
      tm.tm_sec = val % 100;
      tm.tm_min = (val % 10000) / 100;
      tm.tm_hour = val / 10000;
    }

  time_->tv_sec = mktime_utc (&tm);
  time_->tv_usec = 1;
  
  if (*iso_date == '.')
    time_->tv_usec = strtoul (iso_date + 1, (char **)&iso_date, 10);
    
  if (*iso_date == '+' || *iso_date == '-')
    {
      gint sign = (*iso_date == '+') ? -1 : 1;
      
      val = 60 * strtoul (iso_date + 1, (char **)&iso_date, 10);
      
      if (*iso_date == ':')
	val = 60 * val + strtoul (iso_date + 1, NULL, 10);
      else
        val = 60 * (val / 100) + (val % 100);

      time_->tv_sec += (time_t) (val * sign);
    }

  return TRUE;
}

/**
 * g_time_val_to_iso8601:
 * @time_: a #GTimeVal
 * 
 * Converts @time_ into a ISO 8601 encoded string, relative to the
 * Coordinated Universal Time (UTC).
 *
 * Return value: a newly allocated string containing a ISO 8601 date
 *
 * Since: 2.12
 */
static gchar *
libslab_time_val_to_iso8601 (GTimeVal *time_)
{
  gchar *retval;

  g_return_val_if_fail (time_->tv_usec >= 0 && time_->tv_usec < G_USEC_PER_SEC, NULL);

#define ISO_8601_LEN 	21
#define ISO_8601_FORMAT "%Y-%m-%dT%H:%M:%SZ"
  retval = g_new0 (gchar, ISO_8601_LEN + 1);
  
  strftime (retval, ISO_8601_LEN,
	    ISO_8601_FORMAT,
	    gmtime (&(time_->tv_sec)));
  
  return retval;
}



GQuark
libslab_bookmark_file_error_quark (void)
{
  return g_quark_from_static_string ("egg-bookmark-file-error-quark");
}



/********************
 *    Public API    *
 ********************/

/**
 * libslab_bookmark_file_new:
 *
 * Creates a new empty #LibSlabBookmarkFile object.
 *
 * Use libslab_bookmark_file_load_from_file(), libslab_bookmark_file_load_from_data()
 * or libslab_bookmark_file_load_from_data_dirs() to read an existing bookmark
 * file.
 *
 * Return value: an empty #LibSlabBookmarkFile
 *
 * Since: 2.12
 */
LibSlabBookmarkFile *
libslab_bookmark_file_new (void)
{
  LibSlabBookmarkFile *bookmark;
  
  bookmark = g_new (LibSlabBookmarkFile, 1);
  
  libslab_bookmark_file_init (bookmark);
  
  return bookmark;
}

/**
 * libslab_bookmark_file_free:
 * @bookmark: a #LibSlabBookmarkFile
 *
 * Frees a #LibSlabBookmarkFile.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_free (LibSlabBookmarkFile *bookmark)
{
  if (!bookmark)
    return;
  
  libslab_bookmark_file_clear (bookmark);
  
  g_free (bookmark);  
}

/**
 * libslab_bookmark_file_load_from_data:
 * @bookmark: an empty #LibSlabBookmarkFile struct
 * @data: desktop bookmarks loaded in memory
 * @length: the length of @data in bytes
 * @error: return location for a #GError, or %NULL
 *
 * Loads a bookmark file from memory into an empty #LibSlabBookmarkFile
 * structure.  If the object cannot be created then @error is set to a
 * #LibSlabBookmarkFileError.
 *
 * Return value: %TRUE if a desktop bookmark could be loaded.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_load_from_data (LibSlabBookmarkFile  *bookmark,
				const gchar    *data,
				gsize           length,
				GError        **error)
{
  GError *parse_error;
  gboolean retval;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (length != 0, FALSE);
  
  if (length == (gsize) -1)
    length = strlen (data);

  if (bookmark->items)
    {
      libslab_bookmark_file_clear (bookmark);
      libslab_bookmark_file_init (bookmark);
    }
  
  parse_error = NULL;
  retval = libslab_bookmark_file_parse (bookmark, data, length, &parse_error);
  if (!retval)
    {
      g_propagate_error (error, parse_error);
      
      return FALSE;
    }
  
  return TRUE;
}

/**
 * libslab_bookmark_file_load_from_file:
 * @bookmark: an empty #LibSlabBookmarkFile struct
 * @filename: the path of a filename to load, in the GLib file name encoding
 * @error: return location for a #GError, or %NULL
 *
 * Loads a desktop bookmark file into an empty #LibSlabBookmarkFile structure.
 * If the file could not be loaded then @error is set to either a #GFileError
 * or #LibSlabBookmarkFileError.
 *
 * Return value: %TRUE if a desktop bookmark file could be loaded
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_load_from_file (LibSlabBookmarkFile  *bookmark,
				const gchar    *filename,
				GError        **error)
{
  gchar *buffer;
  gsize len;
  GError *read_error;
  gboolean retval;
	
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  read_error = NULL;
  g_file_get_contents (filename, &buffer, &len, &read_error);
  if (read_error)
    {
      g_propagate_error (error, read_error);

      return FALSE;
    }
  
  read_error = NULL;
  retval = libslab_bookmark_file_load_from_data (bookmark,
					   buffer,
					   len,
					   &read_error);
  if (read_error)
    {
      g_propagate_error (error, read_error);

      g_free (buffer);

      return FALSE;
    }

  g_free (buffer);

  return retval;
}


/* Iterates through all the directories in *dirs trying to
 * find file.  When it successfully locates file, returns a
 * string its absolute path.  It also leaves the unchecked
 * directories in *dirs.  You should free the returned string
 *
 * Adapted from gkeyfile.c
 */
static gchar *
find_file_in_data_dirs (const gchar   *file,
                        gchar       ***dirs,
                        GError       **error)
{
  gchar **data_dirs, *data_dir, *path;

  path = NULL;

  if (dirs == NULL)
    return NULL;

  data_dirs = *dirs;
  path = NULL;
  while (data_dirs && (data_dir = *data_dirs) && !path)
    {
      gchar *candidate_file, *sub_dir;

      candidate_file = (gchar *) file;
      sub_dir = g_strdup ("");
      while (candidate_file != NULL && !path)
        {
          gchar *p;

          path = g_build_filename (data_dir, sub_dir,
                                   candidate_file, NULL);

          candidate_file = strchr (candidate_file, '-');

          if (candidate_file == NULL)
            break;

          candidate_file++;

          g_free (sub_dir);
          sub_dir = g_strndup (file, candidate_file - file - 1);

          for (p = sub_dir; *p != '\0'; p++)
            {
              if (*p == '-')
                *p = G_DIR_SEPARATOR;
            }
        }
      g_free (sub_dir);
      data_dirs++;
    }

  *dirs = data_dirs;

  if (!path)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
                   LIBSLAB_BOOKMARK_FILE_ERROR_FILE_NOT_FOUND,
                   _("No valid bookmark file found in data dirs"));
      
      return NULL;
    }
  
  return path;
}


/**
 * libslab_bookmark_file_load_from_data_dirs:
 * @bookmark: a #LibSlabBookmarkFile
 * @file: a relative path to a filename to open and parse
 * @full_path: return location for a string containing the full path
 *   of the file, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * This function looks for a desktop bookmark file named @file in the
 * paths returned from g_get_user_data_dir() and g_get_system_data_dirs(), 
 * loads the file into @bookmark and returns the file's full path in 
 * @full_path.  If the file could not be loaded then an %error is
 * set to either a #GFileError or #LibSlabBookmarkFileError.
 *
 * Return value: %TRUE if a key file could be loaded, %FALSE othewise
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_load_from_data_dirs (LibSlabBookmarkFile  *bookmark,
				     const gchar    *file,
				     gchar         **full_path,
				     GError        **error)
{
  GError *file_error = NULL;
  gchar **all_data_dirs, **data_dirs;
  const gchar *user_data_dir;
  const gchar * const * system_data_dirs;
  gsize i, j;
  gchar *output_path;
  gboolean found_file;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (!g_path_is_absolute (file), FALSE);
  
  user_data_dir = g_get_user_data_dir ();
  system_data_dirs = g_get_system_data_dirs ();
  all_data_dirs = g_new0 (gchar *, g_strv_length ((gchar **)system_data_dirs) + 2);

  i = 0;
  all_data_dirs[i++] = g_strdup (user_data_dir);

  j = 0;
  while (system_data_dirs[j] != NULL)
    all_data_dirs[i++] = g_strdup (system_data_dirs[j++]);

  found_file = FALSE;
  data_dirs = all_data_dirs;
  output_path = NULL;
  while (*data_dirs != NULL && !found_file)
    {
      g_free (output_path);

      output_path = find_file_in_data_dirs (file, &data_dirs, &file_error);
      
      if (file_error)
        {
          g_propagate_error (error, file_error);
 	  break;
        }

      found_file = libslab_bookmark_file_load_from_file (bookmark,
      						   output_path,
      						   &file_error);
      if (file_error)
        {
	  g_propagate_error (error, file_error);
	  break;
        }
    }

  if (found_file && full_path)
    *full_path = output_path;
  else 
    g_free (output_path);

  g_strfreev (all_data_dirs);

  return found_file;
}


/**
 * libslab_bookmark_file_to_data:
 * @bookmark: a #LibSlabBookmarkFile
 * @length: return location for the length of the returned string, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * This function outputs @bookmark as a string.
 *
 * Return value: a newly allocated string holding
 *   the contents of the #LibSlabBookmarkFile
 *
 * Since: 2.12
 */
gchar *
libslab_bookmark_file_to_data (LibSlabBookmarkFile  *bookmark,
			 gsize          *length,
			 GError        **error)
{
  GError *write_error = NULL;
  gchar *retval;
  
  g_return_val_if_fail (bookmark != NULL, NULL);
  
  retval = libslab_bookmark_file_dump (bookmark, length, &write_error);
  if (write_error)
    {
      g_propagate_error (error, write_error);
      
      return NULL;
    }
      
  return retval;
}

/**
 * libslab_bookmark_file_to_file:
 * @bookmark: a #LibSlabBookmarkFile
 * @filename: path of the output file
 * @error: return location for a #GError, or %NULL
 *
 * This function outputs @bookmark into a file.  The write process is
 * guaranteed to be atomic by using g_file_set_contents() internally.
 *
 * Return value: %TRUE if the file was successfully written.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_to_file (LibSlabBookmarkFile  *bookmark,
			 const gchar    *filename,
			 GError        **error)
{
  gchar *data;
  GError *data_error, *write_error;
  gsize len;
  gboolean retval;

  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  
  data_error = NULL;
  data = libslab_bookmark_file_to_data (bookmark, &len, &data_error);
  if (data_error)
    {
      g_propagate_error (error, data_error);
      
      return FALSE;
    }

  write_error = NULL;
  g_file_set_contents (filename, data, len, &write_error);
  if (write_error)
    {
      g_propagate_error (error, write_error);
      
      retval = FALSE;
    }
  else
    retval = TRUE;

  g_free (data);
  
  return retval;
}

static BookmarkItem *
libslab_bookmark_file_lookup_item (LibSlabBookmarkFile *bookmark,
			     const gchar   *uri)
{
  g_assert (bookmark != NULL && uri != NULL);
  
  return g_hash_table_lookup (bookmark->items_by_uri, uri);
}

/* this function adds a new item to the list */
static void
libslab_bookmark_file_add_item (LibSlabBookmarkFile  *bookmark,
			  BookmarkItem   *item,
			  GError        **error)
{
  g_assert (bookmark != NULL);
  g_assert (item != NULL);

  /* this should never happen; and if it does, then we are
   * screwing up something big time.
   */
  if (G_UNLIKELY (libslab_bookmark_file_has_item (bookmark, item->uri)))
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_URI,
		   _("A bookmark for URI '%s' already exists"),
		   item->uri);
      return;
    }
  
  bookmark->items = g_list_prepend (bookmark->items, item);
  
  g_hash_table_replace (bookmark->items_by_uri,
			item->uri,
			item);

  if (item->added == (time_t) -1)
    item->added = time (NULL);
  
  if (item->modified == (time_t) -1)
    item->modified = time (NULL);
}

/**
 * libslab_bookmark_file_remove_item:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @error: return location for a #GError, or %NULL
 *
 * Removes the bookmark for @uri from the bookmark file @bookmark.
 *
 * Return value: %TRUE if the bookmark was removed successfully.
 * 
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_remove_item (LibSlabBookmarkFile  *bookmark,
			     const gchar    *uri,
			     GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return FALSE;
    }

  bookmark->items = g_list_remove (bookmark->items, item);
  g_hash_table_remove (bookmark->items_by_uri, item->uri);  
  
  bookmark_item_free (item);

  return TRUE;
}

/**
 * libslab_bookmark_file_has_item:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 *
 * Looks whether the desktop bookmark has an item with its URI set to @uri.
 *
 * Return value: %TRUE if @uri is inside @bookmark, %FALSE otherwise
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_has_item (LibSlabBookmarkFile *bookmark,
			  const gchar   *uri)
{
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  
  return (NULL != g_hash_table_lookup (bookmark->items_by_uri, uri));
}

/**
 * libslab_bookmark_file_get_uris:
 * @bookmark: a #LibSlabBookmarkFile
 * @length: return location for the number of returned URIs, or %NULL
 *
 * Returns all URIs of the bookmarks in the bookmark file @bookmark.
 * The array of returned URIs will be %NULL-terminated, so @length may
 * optionally be %NULL.
 *
 * Return value: a newly allocated %NULL-terminated array of strings.
 *   Use g_strfreev() to free it.
 *
 * Since: 2.12
 */
gchar **
libslab_bookmark_file_get_uris (LibSlabBookmarkFile *bookmark,
			  gsize         *length)
{
  GList *l;
  gchar **uris;
  gsize i, n_items;
  
  g_return_val_if_fail (bookmark != NULL, NULL);
  
  n_items = g_list_length (bookmark->items); 
  uris = g_new0 (gchar *, n_items + 1);
  
  for (l = g_list_last (bookmark->items), i = 0; l != NULL; l = l->prev)
    {
      BookmarkItem *item = (BookmarkItem *) l->data;
      
      g_assert (item != NULL);
      
      uris[i++] = g_strdup (item->uri);
    }
  uris[i] = NULL;
  
  if (length)
    *length = i;
  
  return uris;
}

/**
 * libslab_bookmark_file_set_title:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI or %NULL
 * @title: a UTF-8 encoded string
 *
 * Sets @title as the title of the bookmark for @uri inside the
 * bookmark file @bookmark.
 *
 * If @uri is %NULL, the title of @bookmark is set.
 *
 * If a bookmark for @uri cannot be found then it is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_title (LibSlabBookmarkFile *bookmark,
			   const gchar   *uri,
			   const gchar   *title)
{
  g_return_if_fail (bookmark != NULL);
  
  if (!uri)
    {
      g_free (bookmark->title);
      bookmark->title = g_strdup (title);
    }
  else
    {
      BookmarkItem *item;
      
      item = libslab_bookmark_file_lookup_item (bookmark, uri);
      if (!item)
        {
          item = bookmark_item_new (uri);
          libslab_bookmark_file_add_item (bookmark, item, NULL);
        }
      
      g_free (item->title);
      item->title = g_strdup (title);
      
      item->modified = time (NULL);
    }
}

/**
 * libslab_bookmark_file_get_title:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Returns the title of the bookmark for @uri.
 *
 * If @uri is %NULL, the title of @bookmark is returned.
 *
 * In the event the URI cannot be found, %NULL is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: a newly allocated string or %NULL if the specified
 *   URI cannot be found.
 *
 * Since: 2.12
 */
gchar *
libslab_bookmark_file_get_title (LibSlabBookmarkFile  *bookmark,
			   const gchar    *uri,
			   GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, NULL);
  
  if (!uri)
    return g_strdup (bookmark->title);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return NULL;
    }
  
  return g_strdup (item->title);
}

/**
 * libslab_bookmark_file_set_description:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI or %NULL
 * @description: a string
 *
 * Sets @description as the description of the bookmark for @uri.
 *
 * If @uri is %NULL, the description of @bookmark is set.
 *
 * If a bookmark for @uri cannot be found then it is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_description (LibSlabBookmarkFile *bookmark,
				 const gchar   *uri,
				 const gchar   *description)
{
  g_return_if_fail (bookmark != NULL);

  if (!uri)
    {
      g_free (bookmark->description); 
      bookmark->description = g_strdup (description);
    }
  else
    {
      BookmarkItem *item;
      
      item = libslab_bookmark_file_lookup_item (bookmark, uri);
      if (!item)
        {
          item = bookmark_item_new (uri);
          libslab_bookmark_file_add_item (bookmark, item, NULL);
        }
      
      g_free (item->description);
      item->description = g_strdup (description);
      
      item->modified = time (NULL);
    }
}

/**
 * libslab_bookmark_file_get_description:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the description of the bookmark for @uri.
 *
 * In the event the URI cannot be found, %NULL is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: a newly allocated string or %NULL if the specified
 *   URI cannot be found.
 *
 * Since: 2.12
 */
gchar *
libslab_bookmark_file_get_description (LibSlabBookmarkFile  *bookmark,
				 const gchar    *uri,
				 GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, NULL);

  if (!uri)
    return g_strdup (bookmark->description);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return NULL;
    }
  
  return g_strdup (item->description);
}

/**
 * libslab_bookmark_file_set_mime_type:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @mime_type: a MIME type
 *
 * Sets @mime_type as the MIME type of the bookmark for @uri.
 *
 * If a bookmark for @uri cannot be found then it is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_mime_type (LibSlabBookmarkFile *bookmark,
			       const gchar   *uri,
			       const gchar   *mime_type)
{
  BookmarkItem *item;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);
  g_return_if_fail (mime_type != NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }
  
  if (!item->metadata)
    item->metadata = bookmark_metadata_new ();
  
  if (item->metadata->mime_type != NULL)
    g_free (item->metadata->mime_type);
  
  item->metadata->mime_type = g_strdup (mime_type);
  item->modified = time (NULL);
}

/**
 * libslab_bookmark_file_get_mime_type:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the MIME type of the resource pointed by @uri.
 *
 * In the event the URI cannot be found, %NULL is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.  In the
 * event that the MIME type cannot be found, %NULL is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_VALUE.
 *
 * Return value: a newly allocated string or %NULL if the specified
 *   URI cannot be found.
 *
 * Since: 2.12
 */
gchar *
libslab_bookmark_file_get_mime_type (LibSlabBookmarkFile  *bookmark,
			       const gchar    *uri,
			       GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return NULL;
    }
  
  if (!item->metadata)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_VALUE,
		   _("No MIME type defined in the bookmark for URI '%s'"),
		   uri);
      return NULL;
    }
  
  return g_strdup (item->metadata->mime_type);
}

/**
 * libslab_bookmark_file_set_is_private:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @is_private: %TRUE if the bookmark should be marked as private
 *
 * Sets the private flag of the bookmark for @uri.
 *
 * If a bookmark for @uri cannot be found then it is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_is_private (LibSlabBookmarkFile *bookmark,
				const gchar   *uri,
				gboolean       is_private)
{
  BookmarkItem *item;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }
  
  if (!item->metadata)
    item->metadata = bookmark_metadata_new ();
  
  item->metadata->is_private = (is_private == TRUE);
  item->modified = time (NULL);
}

/**
 * libslab_bookmark_file_get_is_private:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @error: return location for a #GError, or %NULL
 *
 * Gets whether the private flag of the bookmark for @uri is set.
 *
 * In the event the URI cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.  In the
 * event that the private flag cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_VALUE.
 *
 * Return value: %TRUE if the private flag is set, %FALSE otherwise.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_get_is_private (LibSlabBookmarkFile  *bookmark,
				const gchar    *uri,
				GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return FALSE;
    }
  
  if (!item->metadata)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_VALUE,
		   _("No private flag has been defined in bookmark for URI '%s'"),
		    uri);
      return FALSE;
    }
  
  return item->metadata->is_private;
}

/**
 * libslab_bookmark_file_set_added:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @added: a timestamp or -1 to use the current time
 *
 * Sets the time the bookmark for @uri was added into @bookmark.
 *
 * If no bookmark for @uri is found then it is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_added (LibSlabBookmarkFile *bookmark,
			   const gchar   *uri,
			   time_t         added)
{
  BookmarkItem *item;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }

  if (added == (time_t) -1)
    time (&added);
  
  item->added = added;
  item->modified = added;
}

/**
 * libslab_bookmark_file_get_added:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @error: return location for a #GError, or %NULL
 *
 * Gets the time the bookmark for @uri was added to @bookmark
 *
 * In the event the URI cannot be found, -1 is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: a timestamp
 *
 * Since: 2.12
 */
time_t
libslab_bookmark_file_get_added (LibSlabBookmarkFile  *bookmark,
			   const gchar    *uri,
			   GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, (time_t) -1);
  g_return_val_if_fail (uri != NULL, (time_t) -1);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return (time_t) -1;
    }
  
  return item->added;
}

/**
 * libslab_bookmark_file_set_modified:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @modified: a timestamp or -1 to use the current time
 *
 * Sets the last time the bookmark for @uri was last modified.
 *
 * If no bookmark for @uri is found then it is created.
 *
 * The "modified" time should only be set when the bookmark's meta-data
 * was actually changed.  Every function of #LibSlabBookmarkFile that
 * modifies a bookmark also changes the modification time, except for
 * libslab_bookmark_file_set_visited().
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_modified (LibSlabBookmarkFile *bookmark,
			      const gchar   *uri,
			      time_t         modified)
{
  BookmarkItem *item;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }
  
  if (modified == (time_t) -1)
    time (&modified);
  
  item->modified = modified;
}

/**
 * libslab_bookmark_file_get_modified:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @error: return location for a #GError, or %NULL
 *
 * Gets the time when the bookmark for @uri was last modified.
 *
 * In the event the URI cannot be found, -1 is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: a timestamp
 *
 * Since: 2.12
 */
time_t
libslab_bookmark_file_get_modified (LibSlabBookmarkFile  *bookmark,
			      const gchar    *uri,
			      GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, (time_t) -1);
  g_return_val_if_fail (uri != NULL, (time_t) -1);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return (time_t) -1;
    }
  
  return item->modified;
}

/**
 * libslab_bookmark_file_set_visited:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @visited: a timestamp or -1 to use the current time
 *
 * Sets the time the bookmark for @uri was last visited.
 *
 * If no bookmark for @uri is found then it is created.
 *
 * The "visited" time should only be set if the bookmark was launched, 
 * either using the command line retrieved by libslab_bookmark_file_get_app_info()
 * or by the default application for the bookmark's MIME type, retrieved
 * using libslab_bookmark_file_get_mime_type().  Changing the "visited" time
 * does not affect the "modified" time.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_visited (LibSlabBookmarkFile *bookmark,
			     const gchar   *uri,
			     time_t         visited)
{
  BookmarkItem *item;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }

  if (visited == (time_t) -1)
    time (&visited);
  
  item->visited = visited;
}

/**
 * libslab_bookmark_file_get_visited:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @error: return location for a #GError, or %NULL
 *
 * Gets the time the bookmark for @uri was last visited.
 *
 * In the event the URI cannot be found, -1 is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: a timestamp.
 *
 * Since: 2.12
 */
time_t
libslab_bookmark_file_get_visited (LibSlabBookmarkFile  *bookmark,
			     const gchar    *uri,
			     GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, (time_t) -1);
  g_return_val_if_fail (uri != NULL, (time_t) -1);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return (time_t) -1;
    }
  
  return item->visited;
}

/**
 * libslab_bookmark_file_has_group:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @group: the group name to be searched
 * @error: return location for a #GError, or %NULL
 *
 * Checks whether @group appears in the list of groups to which
 * the bookmark for @uri belongs to.
 *
 * In the event the URI cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: %TRUE if @group was found.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_has_group (LibSlabBookmarkFile  *bookmark,
			   const gchar    *uri,
			   const gchar    *group,
			   GError        **error)
{
  BookmarkItem *item;
  GList *l;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return FALSE;
    }
  
  if (!item->metadata)
    return FALSE;
   
  for (l = item->metadata->groups; l != NULL; l = l->next)
    {
      if (strcmp (l->data, group) == 0)
        return TRUE;
    }
  
  return FALSE;

}

/**
 * libslab_bookmark_file_add_group:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @group: the group name to be added
 *
 * Adds @group to the list of groups to which the bookmark for @uri
 * belongs to.
 *
 * If no bookmark for @uri is found then it is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_add_group (LibSlabBookmarkFile *bookmark,
			   const gchar   *uri,
			   const gchar   *group)
{
  BookmarkItem *item;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);
  g_return_if_fail (group != NULL && group[0] != '\0');
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }
  
  if (!item->metadata)
    item->metadata = bookmark_metadata_new ();
  
  if (!libslab_bookmark_file_has_group (bookmark, uri, group, NULL))
    {
      item->metadata->groups = g_list_prepend (item->metadata->groups,
                                               g_strdup (group));
      
      item->modified = time (NULL);
    }
}

/**
 * libslab_bookmark_file_remove_group:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @group: the group name to be removed
 * @error: return location for a #GError, or %NULL
 *
 * Removes @group from the list of groups to which the bookmark
 * for @uri belongs to.
 *
 * In the event the URI cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 * In the event no group was defined, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_VALUE.
 *
 * Return value: %TRUE if @group was successfully removed.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_remove_group (LibSlabBookmarkFile  *bookmark,
			      const gchar    *uri,
			      const gchar    *group,
			      GError        **error)
{
  BookmarkItem *item;
  GList *l;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return FALSE;
    }
  
  if (!item->metadata)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
                   LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_VALUE,
                   _("No groups set in bookmark for URI '%s'"),
                   uri);
      return FALSE;
    }
  
  for (l = item->metadata->groups; l != NULL; l = l->next)
    {
      if (strcmp (l->data, group) == 0)
        {
          item->metadata->groups = g_list_remove_link (item->metadata->groups, l);
          g_free (l->data);
	  g_list_free_1 (l);
          
          item->modified = time (NULL);          
          
          return TRUE;
        }
    }
  
  return FALSE;
}

/**
 * libslab_bookmark_file_set_groups:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: an item's URI
 * @groups: an array of group names, or %NULL to remove all groups
 * @length: number of group name values in @groups
 *
 * Sets a list of group names for the item with URI @uri.  Each previously
 * set group name list is removed.
 *
 * If @uri cannot be found then an item for it is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_groups (LibSlabBookmarkFile  *bookmark,
			    const gchar    *uri,
			    const gchar   **groups,
			    gsize           length)
{
  BookmarkItem *item;
  gsize i;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);
  g_return_if_fail (groups != NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }
  
  if (!item->metadata)
    item->metadata = bookmark_metadata_new ();

  if (item->metadata->groups != NULL)
    {
      g_list_foreach (item->metadata->groups,
		      (GFunc) g_free,
		      NULL);
      g_list_free (item->metadata->groups);
      item->metadata->groups = NULL;
    }
  
  if (groups)
    {
      for (i = 0; groups[i] != NULL && i < length; i++)
        item->metadata->groups = g_list_append (item->metadata->groups,
					        g_strdup (groups[i]));
    }

  item->modified = time (NULL);
}

/**
 * libslab_bookmark_file_get_groups:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @length: return location for the length of the returned string, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the list of group names of the bookmark for @uri.
 *
 * In the event the URI cannot be found, %NULL is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * The returned array is %NULL terminated, so @length may optionally
 * be %NULL.
 *
 * Return value: a newly allocated %NULL-terminated array of group names.
 *   Use g_strfreev() to free it.
 *
 * Since: 2.12
 */
gchar **
libslab_bookmark_file_get_groups (LibSlabBookmarkFile  *bookmark,
			    const gchar    *uri,
			    gsize          *length,
			    GError        **error)
{
  BookmarkItem *item;
  GList *l;
  gsize len, i;
  gchar **retval;
  
  g_return_val_if_fail (bookmark != NULL, NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return NULL;
    }
  
  if (!item->metadata)
    {
      if (length)
	*length = 0;
      
      return NULL;
    }
  
  len = g_list_length (item->metadata->groups);
  retval = g_new0 (gchar *, len + 1);
  for (l = g_list_last (item->metadata->groups), i = 0;
       l != NULL;
       l = l->prev)
    {
      gchar *group_name = (gchar *) l->data;
      
      g_assert (group_name != NULL);
      
      retval[i++] = g_strdup (group_name);
    }
  retval[i] = NULL;
  
  if (length)
    *length = len;
  
  return retval;
}

/**
 * libslab_bookmark_file_add_application:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @name: the name of the application registering the bookmark
 *   or %NULL
 * @exec: command line to be used to launch the bookmark or %NULL
 *
 * Adds the application with @name and @exec to the list of
 * applications that have registered a bookmark for @uri into
 * @bookmark.
 *
 * Every bookmark inside a #LibSlabBookmarkFile must have at least an
 * application registered.  Each application must provide a name, a
 * command line useful for launching the bookmark, the number of times
 * the bookmark has been registered by the application and the last
 * time the application registered this bookmark.
 *
 * If @name is %NULL, the name of the application will be the
 * same returned by g_get_application(); if @exec is %NULL, the
 * command line will be a composition of the program name as
 * returned by g_get_prgname() and the "%u" modifier, which will be
 * expanded to the bookmark's URI.
 *
 * This function will automatically take care of updating the
 * registrations count and timestamping in case an application
 * with the same @name had already registered a bookmark for
 * @uri inside @bookmark.
 *
 * If no bookmark for @uri is found, one is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_add_application (LibSlabBookmarkFile *bookmark,
				 const gchar   *uri,
				 const gchar   *name,
				 const gchar   *exec)
{
  BookmarkItem *item;
  gchar *app_name, *app_exec;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }
  
  if (name && name[0] != '\0')
    app_name = g_strdup (name);
  else
    app_name = g_strdup (g_get_application_name ());
  
  if (exec && exec[0] != '\0')
    app_exec = g_strdup (exec);
  else
    app_exec = g_strjoin (" ", g_get_prgname(), "%u", NULL);

  libslab_bookmark_file_set_app_info (bookmark, uri,
                                app_name,
                                app_exec,
                                -1,
                                (time_t) -1,
                                NULL);
  
  g_free (app_exec);
  g_free (app_name);
}

/**
 * libslab_bookmark_file_remove_application:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @name: the name of the application
 * @error: return location for a #GError or %NULL
 *
 * Removes application registered with @name from the list of applications
 * that have registered a bookmark for @uri inside @bookmark.
 *
 * In the event the URI cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 * In the event that no application with name @app_name has registered
 * a bookmark for @uri,  %FALSE is returned and error is set to
 * #LIBSLAB_BOOKMARK_FILE_ERROR_APP_NOT_REGISTERED.
 *
 * Return value: %TRUE if the application was successfully removed.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_remove_application (LibSlabBookmarkFile  *bookmark,
				    const gchar    *uri,
				    const gchar    *name,
				    GError        **error)
{
  GError *set_error;
  gboolean retval;
    
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  
  set_error = NULL;
  retval = libslab_bookmark_file_set_app_info (bookmark, uri,
  					 name,
  					 "",
	  				 0,
  					 (time_t) -1,
  					 &set_error);
  if (set_error)
    {
      g_propagate_error (error, set_error);
      
      return FALSE;
    }
  
  return retval;
}

/**
 * libslab_bookmark_file_has_application:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @name: the name of the application
 * @error: return location for a #GError or %NULL
 *
 * Checks whether the bookmark for @uri inside @bookmark has been
 * registered by application @name.
 *
 * In the event the URI cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: %TRUE if the application @name was found
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_has_application (LibSlabBookmarkFile  *bookmark,
				 const gchar    *uri,
				 const gchar    *name,
				 GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return FALSE;
    }
  
  return (NULL != bookmark_item_lookup_app_info (item, name));
}

/**
 * libslab_bookmark_file_set_app_info:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @name: an application's name
 * @exec: an application's command line
 * @count: the number of registrations done for this application
 * @stamp: the time of the last registration for this application
 * @error: return location for a #GError or %NULL
 *
 * Sets the meta-data of application @name inside the list of
 * applications that have registered a bookmark for @uri inside
 * @bookmark.
 *
 * You should rarely use this function; use libslab_bookmark_file_add_application()
 * and libslab_bookmark_file_remove_application() instead.
 *
 * @name can be any UTF-8 encoded string used to identify an
 * application.
 * @exec can have one of these two modifiers: "%f", which will
 * be expanded as the local file name retrieved from the bookmark's
 * URI; "%u", which will be expanded as the bookmark's URI.
 * The expansion is done automatically when retrieving the stored
 * command line using the libslab_bookmark_file_get_app_info() function.
 * @count is the number of times the application has registered the
 * bookmark; if is < 0, the current registration count will be increased
 * by one, if is 0, the application with @name will be removed from
 * the list of registered applications.
 * @stamp is the Unix time of the last registration; if it is -1, the
 * current time will be used.
 *
 * If you try to remove an application by setting its registration count to
 * zero, and no bookmark for @uri is found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND; similarly,
 * in the event that no application @name has registered a bookmark
 * for @uri,  %FALSE is returned and error is set to
 * #LIBSLAB_BOOKMARK_FILE_ERROR_APP_NOT_REGISTERED.  Otherwise, if no bookmark
 * for @uri is found, one is created.
 *
 * Return value: %TRUE if the application's meta-data was successfully
 *   changed.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_set_app_info (LibSlabBookmarkFile  *bookmark,
			      const gchar    *uri,
			      const gchar    *name,
			      const gchar    *exec,
			      gint            count,
			      time_t          stamp,
			      GError        **error)
{
  BookmarkItem *item;
  BookmarkAppInfo *ai;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (exec != NULL, FALSE);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      if (count == 0)
        {
          g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		       LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		       _("No bookmark found for URI '%s'"),
		       uri);
	  return FALSE;
	}
      else
        {
          item = bookmark_item_new (uri);
	  libslab_bookmark_file_add_item (bookmark, item, NULL);
	}
    }
  
  ai = bookmark_item_lookup_app_info (item, name);
  if (!ai)
    {
      if (count == 0)
        {
          g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		       LIBSLAB_BOOKMARK_FILE_ERROR_APP_NOT_REGISTERED,
		       _("No application with name '%s' registered a bookmark for '%s'"),
		       name,
		       uri);
          return FALSE;
        }
      else
        {
          ai = bookmark_app_info_new (name);
          
          item->metadata->applications = g_list_prepend (item->metadata->applications, ai);
          g_hash_table_replace (item->metadata->apps_by_name, ai->name, ai);
        }
    }

  if (count == 0)
    {
      item->metadata->applications = g_list_remove (item->metadata->applications, ai);
      g_hash_table_remove (item->metadata->apps_by_name, ai->name);
      bookmark_app_info_free (ai);

      item->modified = time (NULL);
          
      return TRUE;
    }
  else if (count > 0)
    ai->count = count;
  else
    ai->count += 1;
      
  if (stamp != (time_t) -1)
    ai->stamp = stamp;
  else
    ai->stamp = time (NULL);
  
  if (exec && exec[0] != '\0')
    {
      g_free (ai->exec);
      ai->exec = g_strdup (exec);
    }
  
  item->modified = time (NULL);
  
  return TRUE;
}

/* expands the application's command line */
static gchar *
expand_exec_line (const gchar *exec_fmt,
		  const gchar *uri)
{
  GString *exec;
  gchar ch;
  
  exec = g_string_new (NULL);
  while ((ch = *exec_fmt++) != '\0')
   {
     if (ch != '%')
       {
         exec = g_string_append_c (exec, ch);
         continue;
       }
     
     ch = *exec_fmt++;
     switch (ch)
       {
       case '\0':
	 goto out;
       case 'u':
         g_string_append (exec, uri);
         break;
       case 'f':
         {
	   gchar *file = g_filename_from_uri (uri, NULL, NULL);
	   g_string_append (exec, file);
	   g_free (file);
         }
         break;
       case '%':
       default:
         exec = g_string_append_c (exec, ch);
         break;
       }
   }
   
 out:
  return g_string_free (exec, FALSE);
}

/**
 * libslab_bookmark_file_get_app_info:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @name: an application's name
 * @exec: location for the command line of the application, or %NULL
 * @count: return location for the registration count, or %NULL
 * @stamp: return location for the last registration time, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Gets the registration informations of @app_name for the bookmark for
 * @uri.  See libslab_bookmark_file_set_app_info() for more informations about
 * the returned data.
 *
 * The string returned in @app_exec must be freed.
 *
 * In the event the URI cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.  In the
 * event that no application with name @app_name has registered a bookmark
 * for @uri,  %FALSE is returned and error is set to
 * #LIBSLAB_BOOKMARK_FILE_ERROR_APP_NOT_REGISTERED.
 *
 * Return value: %TRUE on success.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_get_app_info (LibSlabBookmarkFile  *bookmark,
			      const gchar    *uri,
			      const gchar    *name,
			      gchar         **exec,
			      guint          *count,
			      time_t         *stamp,
			      GError        **error)
{
  BookmarkItem *item;
  BookmarkAppInfo *ai;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return FALSE;
    }
  
  ai = bookmark_item_lookup_app_info (item, name);
  if (!ai)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_APP_NOT_REGISTERED,
		   _("No application with name '%s' registered a bookmark for '%s'"),
		   name,
		   uri);
      return FALSE;
    }
  
  if (exec)
    *exec = expand_exec_line (ai->exec, uri);
  
  if (count)
    *count = ai->count;
  
  if (stamp)
    *stamp = ai->stamp;
  
  return TRUE;
}

/**
 * libslab_bookmark_file_get_applications:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @length: return location of the length of the returned list, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the names of the applications that have registered the
 * bookmark for @uri.
 * 
 * In the event the URI cannot be found, %NULL is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: a newly allocated %NULL-terminated array of strings.
 *   Use g_strfreev() to free it.
 *
 * Since: 2.12
 */
gchar **
libslab_bookmark_file_get_applications (LibSlabBookmarkFile  *bookmark,
				  const gchar    *uri,
				  gsize          *length,
				  GError        **error)
{
  BookmarkItem *item;
  GList *l;
  gchar **apps;
  gsize i, n_apps;
  
  g_return_val_if_fail (bookmark != NULL, NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return NULL;
    }
  
  if (!item->metadata)
    {	   
      if (length)
	*length = 0;
      
      return NULL;
    }
  
  n_apps = g_list_length (item->metadata->applications);
  apps = g_new0 (gchar *, n_apps + 1);
  
  for (l = g_list_last (item->metadata->applications), i = 0;
       l != NULL;
       l = l->prev)
    {
      BookmarkAppInfo *ai;
      
      ai = (BookmarkAppInfo *) l->data;
      
      g_assert (ai != NULL);
      g_assert (ai->name != NULL);
      
      apps[i++] = g_strdup (ai->name);
    }
  apps[i] = NULL;
  
  if (length)
    *length = i;
  
  return apps;
}

/**
 * libslab_bookmark_file_get_size:
 * @bookmark: a #LibSlabBookmarkFile
 * 
 * Gets the number of bookmarks inside @bookmark.
 * 
 * Return value: the number of bookmarks
 *
 * Since: 2.12
 */
gint
libslab_bookmark_file_get_size (LibSlabBookmarkFile *bookmark)
{
  g_return_val_if_fail (bookmark != NULL, 0);

  return g_list_length (bookmark->items);
}

/**
 * libslab_bookmark_file_move_item:
 * @bookmark: a #LibSlabBookmarkFile
 * @old_uri: a valid URI
 * @new_uri: a valid URI, or %NULL
 * @error: return location for a #GError or %NULL
 *
 * Changes the URI of a bookmark item from @old_uri to @new_uri.  Any
 * existing bookmark for @new_uri will be overwritten.  If @new_uri is
 * %NULL, then the bookmark is removed.
 *
 * In the event the URI cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: %TRUE if the URI was successfully changed
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_move_item (LibSlabBookmarkFile  *bookmark,
			   const gchar    *old_uri,
			   const gchar    *new_uri,
			   GError        **error)
{
  BookmarkItem *item;
  GError *remove_error;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (old_uri != NULL, FALSE);

  item = libslab_bookmark_file_lookup_item (bookmark, old_uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   old_uri);
      return FALSE;
    }

  if (new_uri && new_uri[0] != '\0')
    {
      if (libslab_bookmark_file_has_item (bookmark, new_uri))
        {
          remove_error = NULL;
          libslab_bookmark_file_remove_item (bookmark, new_uri, &remove_error);
          if (remove_error)
            {
              g_propagate_error (error, remove_error);
              
              return FALSE;
            }
        }

      g_hash_table_steal (bookmark->items_by_uri, item->uri);
      
      g_free (item->uri);
      item->uri = g_strdup (new_uri);
      item->modified = time (NULL);

      g_hash_table_replace (bookmark->items_by_uri, item->uri, item);

      return TRUE;
    }
  else
    {
      remove_error = NULL;
      libslab_bookmark_file_remove_item (bookmark, old_uri, &remove_error);
      if (remove_error)
        {
          g_propagate_error (error, remove_error);
          
          return FALSE;
        }

      return TRUE;
    }
}

/**
 * libslab_bookmark_file_set_icon:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @href: the URI of the icon for the bookmark, or %NULL
 * @mime_type: the MIME type of the icon for the bookmark
 *
 * Sets the icon for the bookmark for @uri.  If @href is %NULL, unsets
 * the currently set icon.
 *
 * If no bookmark for @uri is found it is created.
 *
 * Since: 2.12
 */
void
libslab_bookmark_file_set_icon (LibSlabBookmarkFile *bookmark,
			  const gchar   *uri,
			  const gchar   *href,
			  const gchar   *mime_type)
{
  BookmarkItem *item;
  
  g_return_if_fail (bookmark != NULL);
  g_return_if_fail (uri != NULL);

  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      item = bookmark_item_new (uri);
      libslab_bookmark_file_add_item (bookmark, item, NULL);
    }
  
  if (!item->metadata)
    item->metadata = bookmark_metadata_new ();
  
  g_free (item->metadata->icon_href);
  g_free (item->metadata->icon_mime);
  
  item->metadata->icon_href = g_strdup (href);
  
  if (mime_type && mime_type[0] != '\0')
    item->metadata->icon_mime = g_strdup (mime_type);
  else
    item->metadata->icon_mime = g_strdup ("application/octet-stream");
  
  item->modified = time (NULL);
}

/**
 * libslab_bookmark_file_get_icon:
 * @bookmark: a #LibSlabBookmarkFile
 * @uri: a valid URI
 * @href: return location for the icon's location or %NULL
 * @mime_type: return location for the icon's MIME type or %NULL
 * @error: return location for a #GError or %NULL
 *
 * Gets the icon of the bookmark for @uri.
 *
 * In the event the URI cannot be found, %FALSE is returned and
 * @error is set to #LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND.
 *
 * Return value: %TRUE if the icon for the bookmark for the URI was found.
 *   You should free the returned strings.
 *
 * Since: 2.12
 */
gboolean
libslab_bookmark_file_get_icon (LibSlabBookmarkFile  *bookmark,
			  const gchar    *uri,
			  gchar         **href,
			  gchar         **mime_type,
			  GError        **error)
{
  BookmarkItem *item;
  
  g_return_val_if_fail (bookmark != NULL, FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  
  item = libslab_bookmark_file_lookup_item (bookmark, uri);
  if (!item)
    {
      g_set_error (error, LIBSLAB_BOOKMARK_FILE_ERROR,
		   LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
		   _("No bookmark found for URI '%s'"),
		   uri);
      return FALSE;
    }
  
  if ((!item->metadata) || (!item->metadata->icon_href))
    return FALSE;
  
  if (href)
    *href = g_strdup (item->metadata->icon_href);
  
  if (mime_type)
    *mime_type = g_strdup (item->metadata->icon_mime);
  
  return TRUE;
}
