/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2007 Novell, Inc.
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

#ifndef __BOOKMARK_AGENT_LIBSLAB_H__
#define __BOOKMARK_AGENT_LIBSLAB_H__

#include <glib.h>

G_BEGIN_DECLS

#include "libslab-bookmarkfile.h"

#define GBookmarkFile                    LibSlabBookmarkFile
                                                                            
#define g_bookmark_file_new              libslab_bookmark_file_new
#define g_bookmark_file_free             libslab_bookmark_file_free

#define g_bookmark_file_load_from_file   libslab_bookmark_file_load_from_file
#define g_bookmark_file_to_file          libslab_bookmark_file_to_file

#define g_bookmark_file_has_item         libslab_bookmark_file_has_item
#define g_bookmark_file_remove_item      libslab_bookmark_file_remove_item
#define g_bookmark_file_get_uris         libslab_bookmark_file_get_uris
#define g_bookmark_file_get_size         libslab_bookmark_file_get_size
#define g_bookmark_file_get_title        libslab_bookmark_file_get_title
#define g_bookmark_file_set_title        libslab_bookmark_file_set_title
#define g_bookmark_file_get_mime_type    libslab_bookmark_file_get_mime_type
#define g_bookmark_file_set_mime_type    libslab_bookmark_file_set_mime_type
#define g_bookmark_file_get_modified     libslab_bookmark_file_get_modified
#define g_bookmark_file_set_modified     libslab_bookmark_file_set_modified
#define g_bookmark_file_get_icon         libslab_bookmark_file_get_icon
#define g_bookmark_file_set_icon         libslab_bookmark_file_set_icon
#define g_bookmark_file_get_applications libslab_bookmark_file_get_applications
#define g_bookmark_file_add_application  libslab_bookmark_file_add_application
#define g_bookmark_file_get_app_info     libslab_bookmark_file_get_app_info
#define g_bookmark_file_get_groups       libslab_bookmark_file_get_groups
#define g_bookmark_file_add_group        libslab_bookmark_file_add_group
#define g_bookmark_file_remove_group     libslab_bookmark_file_remove_group
#define g_bookmark_file_move_item        libslab_bookmark_file_move_item

G_END_DECLS

#endif
