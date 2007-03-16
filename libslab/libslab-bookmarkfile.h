/* gbookmarkfile.h: parsing and building desktop bookmarks
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

#ifndef __LIBSLAB_BOOKMARK_FILE_H__
#define __LIBSLAB_BOOKMARK_FILE_H__

#include <glib/gerror.h>
#include <time.h>

G_BEGIN_DECLS

/* GError enumeration
 */
#define LIBSLAB_BOOKMARK_FILE_ERROR	(libslab_bookmark_file_error_quark ())

typedef enum
{
  LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_URI,
  LIBSLAB_BOOKMARK_FILE_ERROR_INVALID_VALUE,
  LIBSLAB_BOOKMARK_FILE_ERROR_APP_NOT_REGISTERED,
  LIBSLAB_BOOKMARK_FILE_ERROR_URI_NOT_FOUND,
  LIBSLAB_BOOKMARK_FILE_ERROR_READ,
  LIBSLAB_BOOKMARK_FILE_ERROR_UNKNOWN_ENCODING,
  LIBSLAB_BOOKMARK_FILE_ERROR_WRITE,
  LIBSLAB_BOOKMARK_FILE_ERROR_FILE_NOT_FOUND
} LibSlabBookmarkFileError;

GQuark libslab_bookmark_file_error_quark (void);

/*
 * LibSlabBookmarkFile
 */
typedef struct _LibSlabBookmarkFile LibSlabBookmarkFile;

LibSlabBookmarkFile *libslab_bookmark_file_new                 (void);
void           libslab_bookmark_file_free                (LibSlabBookmarkFile  *bookmark);

gboolean       libslab_bookmark_file_load_from_file      (LibSlabBookmarkFile  *bookmark,
						    const gchar    *filename,
						    GError        **error);
gboolean       libslab_bookmark_file_load_from_data      (LibSlabBookmarkFile  *bookmark,
						    const gchar    *data,
						    gsize           length,
						    GError        **error);
gboolean       libslab_bookmark_file_load_from_data_dirs (LibSlabBookmarkFile  *bookmark,
						    const gchar    *file,
						    gchar         **full_path,
						    GError        **error);
gchar *        libslab_bookmark_file_to_data             (LibSlabBookmarkFile  *bookmark,
						    gsize          *length,
						    GError        **error) G_GNUC_MALLOC;
gboolean       libslab_bookmark_file_to_file             (LibSlabBookmarkFile  *bookmark,
						    const gchar    *filename,
						    GError        **error);

void           libslab_bookmark_file_set_title           (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *title);
gchar *        libslab_bookmark_file_get_title           (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    GError        **error) G_GNUC_MALLOC;
void           libslab_bookmark_file_set_description     (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *description);
gchar *        libslab_bookmark_file_get_description     (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    GError        **error) G_GNUC_MALLOC;
void           libslab_bookmark_file_set_mime_type       (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *mime_type);
gchar *        libslab_bookmark_file_get_mime_type       (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    GError        **error) G_GNUC_MALLOC;
void           libslab_bookmark_file_set_groups          (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar   **groups,
						    gsize           length);
void           libslab_bookmark_file_add_group           (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *group);
gboolean       libslab_bookmark_file_has_group           (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *group,
						    GError        **error);
gchar **       libslab_bookmark_file_get_groups          (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    gsize          *length,
						    GError        **error) G_GNUC_MALLOC;
void           libslab_bookmark_file_add_application     (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *name,
						    const gchar    *exec);
gboolean       libslab_bookmark_file_has_application     (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *name,
						    GError        **error);
gchar **       libslab_bookmark_file_get_applications    (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    gsize          *length,
						    GError        **error) G_GNUC_MALLOC;
gboolean       libslab_bookmark_file_set_app_info        (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *name,
						    const gchar    *exec,
						    gint            count,
						    time_t          stamp,
						    GError        **error);
gboolean       libslab_bookmark_file_get_app_info        (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *name,
						    gchar         **exec,
						    guint          *count,
						    time_t         *stamp,
						    GError        **error);
void           libslab_bookmark_file_set_is_private      (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    gboolean        is_private);
gboolean       libslab_bookmark_file_get_is_private      (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    GError        **error);
void           libslab_bookmark_file_set_icon            (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *href,
						    const gchar    *mime_type);
gboolean       libslab_bookmark_file_get_icon            (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    gchar         **href,
						    gchar         **mime_type,
						    GError        **error);
void           libslab_bookmark_file_set_added           (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    time_t          added);
time_t         libslab_bookmark_file_get_added           (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    GError        **error);
void           libslab_bookmark_file_set_modified        (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    time_t          modified);
time_t         libslab_bookmark_file_get_modified        (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    GError        **error);
void           libslab_bookmark_file_set_visited         (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    time_t          visited);
time_t         libslab_bookmark_file_get_visited         (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri, 
						    GError        **error);
gboolean       libslab_bookmark_file_has_item            (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri);
gint           libslab_bookmark_file_get_size            (LibSlabBookmarkFile  *bookmark);
gchar **       libslab_bookmark_file_get_uris            (LibSlabBookmarkFile  *bookmark,
						    gsize          *length) G_GNUC_MALLOC;
gboolean       libslab_bookmark_file_remove_group        (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *group,
						    GError        **error);
gboolean       libslab_bookmark_file_remove_application  (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    const gchar    *name,
						    GError        **error);
gboolean       libslab_bookmark_file_remove_item         (LibSlabBookmarkFile  *bookmark,
						    const gchar    *uri,
						    GError        **error);
gboolean       libslab_bookmark_file_move_item           (LibSlabBookmarkFile  *bookmark,
						    const gchar    *old_uri,
						    const gchar    *new_uri,
						    GError        **error);

G_END_DECLS

#endif /* __LIBSLAB_BOOKMARK_FILE_H__ */
