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

#define EGG_ENABLE_RECENT_FILES
#include "egg-recent-model.h"

GList *
get_recent_files ()
{
	EggRecentModel *recent_model;

	GList *list;

	recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);

	list = egg_recent_model_get_list (recent_model);

	g_object_unref (recent_model);

	return list;
}

GList *
get_recent_apps ()
{
	EggRecentModel *recent_model;

	GList *list;

	recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);

	egg_recent_model_set_filter_groups (recent_model, "recently-used-apps", NULL);

	list = egg_recent_model_get_list (recent_model);

	g_object_unref (recent_model);

	return list;
}
