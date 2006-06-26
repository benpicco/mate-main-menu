/*
 * This file is part of libslab.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * Libslab is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libslab is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __EGG_RECENT_MODEL_H__
#define __EGG_RECENT_MODEL_H__

#include "egg-recent-item.h"

G_BEGIN_DECLS

#define EGG_TYPE_RECENT_MODEL		(egg_recent_model_get_type ())
#define EGG_RECENT_MODEL(obj)		G_TYPE_CHECK_INSTANCE_CAST (obj, EGG_TYPE_RECENT_MODEL, EggRecentModel)
#define EGG_RECENT_MODEL_CLASS(klass) 	G_TYPE_CHECK_CLASS_CAST (klass, EGG_TYPE_RECENT_MODEL, EggRecentModelClass)
#define EGG_IS_RECENT_MODEL(obj)	G_TYPE_CHECK_INSTANCE_TYPE (obj, egg_recent_model_get_type ())

typedef struct _EggRecentModel        EggRecentModel;
typedef struct _EggRecentModelPrivate EggRecentModelPrivate;
typedef struct _EggRecentModelClass   EggRecentModelClass;

struct _EggRecentModel {
	GObject                parent_instance;

	EggRecentModelPrivate *priv;
};

struct _EggRecentModelClass {
	GObjectClass parent_class;
			
	void (*changed) (EggRecentModel *model, GList *list);
};

typedef enum {
	EGG_RECENT_MODEL_SORT_MRU,
	EGG_RECENT_MODEL_SORT_LRU,
	EGG_RECENT_MODEL_SORT_NONE
} EggRecentModelSort;


/* Standard group names */
#define EGG_RECENT_GROUP_LAUNCHERS "Launchers"


GType    egg_recent_model_get_type     (void);

/* constructors */
EggRecentModel * egg_recent_model_new (const gchar *file_path, EggRecentModelSort sort);

/* public methods */
void     egg_recent_model_set_filter_mime_types (EggRecentModel *model,
						   ...);

void     egg_recent_model_set_filter_groups (EggRecentModel *model, ...);

void     egg_recent_model_set_filter_uri_schemes (EggRecentModel *model,
						   ...);

void     egg_recent_model_set_sort (EggRecentModel *model,
				      EggRecentModelSort sort);

gboolean egg_recent_model_add_full (EggRecentModel *model,
				      EggRecentItem *item);

gboolean egg_recent_model_add	     (EggRecentModel *model,
				      const gchar *uri);

gboolean egg_recent_model_delete   (EggRecentModel *model,
				      const gchar *uri);

void egg_recent_model_clear        (EggRecentModel *model);

GList * egg_recent_model_get_list  (EggRecentModel *model);

void egg_recent_model_changed      (EggRecentModel *model);

void egg_recent_model_set_limit    (EggRecentModel *model, int limit);
int  egg_recent_model_get_limit    (EggRecentModel *model);

void egg_recent_model_remove_expired (EggRecentModel *model);

G_END_DECLS

#endif /* __EGG_RECENT_MODEL_H__ */
