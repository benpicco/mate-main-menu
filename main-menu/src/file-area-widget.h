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

#ifndef __FILE_AREA_WIDGET_H__
#define __FILE_AREA_WIDGET_H__

#include <gtk/gtk.h>

#include "main-menu-common.h"

G_BEGIN_DECLS

#define FILE_AREA_WIDGET_TYPE            (file_area_widget_get_type ())
#define FILE_AREA_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FILE_AREA_WIDGET_TYPE, FileAreaWidget))
#define FILE_AREA_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FILE_AREA_WIDGET_TYPE, FileAreaWidgetClass))
#define IS_FILE_AREA_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FILE_AREA_WIDGET_TYPE))
#define IS_FILE_AREA_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FILE_AREA_WIDGET_TYPE))
#define FILE_AREA_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FILE_AREA_WIDGET_TYPE, FileAreaWidgetClass))

typedef struct
{
	GtkVBox parent;

	GtkWidget *selector_label;
} FileAreaWidget;

typedef struct
{
	GtkWindowClass parent_class;
} FileAreaWidgetClass;

GType file_area_widget_get_type (void);

GtkWidget *file_area_widget_new (MainMenuUI * ui, MainMenuEngine * engine);
void file_area_widget_update (FileAreaWidget * file_area);

G_END_DECLS
#endif /* __FILE_AREA_WIDGET_H__ */
