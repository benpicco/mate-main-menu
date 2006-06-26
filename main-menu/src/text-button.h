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

#ifndef __TEXT_BUTTON_H__
#define __TEXT_BUTTON_H__

#include <glib.h>
#include <gtk/gtkeventbox.h>

G_BEGIN_DECLS
#define TEXT_BUTTON_TYPE            (text_button_get_type ())
#define TEXT_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEXT_BUTTON_TYPE, TextButton))
#define TEXT_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TEXT_BUTTON_TYPE, TextButtonClass))
#define IS_TEXT_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEXT_BUTTON_TYPE))
#define IS_TEXT_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TEXT_BUTTON_TYPE))
#define TEXT_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEXT_BUTTON_TYPE, TextButtonClass))
	typedef struct {
	GtkEventBox parent_placeholder;

	gchar *text;
} TextButton;

typedef struct {
	GtkEventBoxClass parent_class;

	void (*activate) (TextButton * button);
} TextButtonClass;

GType text_button_get_type (void);
GtkWidget *text_button_new (const gchar * text);

G_END_DECLS
#endif /* __TEXT_BUTTON_H__ */
