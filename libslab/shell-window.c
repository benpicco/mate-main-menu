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

#include "shell-window.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkframe.h>
#include <gtk/gtklayout.h>
#include <gtk/gtkscrolledwindow.h>

#include "app-resizer.h"

static GtkWindowClass *parent_class = NULL;

static void shell_window_class_init (ShellWindowClass *);
static void shell_window_init (ShellWindow *);
static void shell_window_destroy (GtkObject *);
static void shell_window_handle_size_request (GtkWidget * widget, GtkRequisition * requisition,
	AppShellData * data);

gboolean shell_window_paint_window (GtkWidget * widget, GdkEventExpose * event, gpointer data);

#define SHELL_WINDOW_BORDER_WIDTH 6

GType
shell_window_get_type (void)
{
	static GType object_type = 0;

	if (!object_type)
	{
		static const GTypeInfo object_info = {
			sizeof (ShellWindowClass),
			NULL,
			NULL,
			(GClassInitFunc) shell_window_class_init,
			NULL,
			NULL,
			sizeof (ShellWindow),
			0,
			(GInstanceInitFunc) shell_window_init
		};

		object_type = g_type_register_static (
			GTK_TYPE_FRAME, "ShellWindow", &object_info, 0);
	}

	return object_type;
}

static void
shell_window_class_init (ShellWindowClass * klass)
{
	parent_class = g_type_class_peek_parent (klass);

	((GtkObjectClass *) klass)->destroy = shell_window_destroy;
}

static void
shell_window_init (ShellWindow * window)
{
	window->_hbox = NULL;
	window->_left_pane = NULL;
	window->_right_pane = NULL;
}

GtkWidget *
shell_window_new (AppShellData * app_data)
{
	ShellWindow *window = g_object_new (SHELL_WINDOW_TYPE, NULL);

	gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
	/*
	printf("shadow type:%d\n", gtk_frame_get_shadow_type(GTK_FRAME(window)));
	gtk_frame_set_shadow_type(GTK_FRAME(window), GTK_SHADOW_NONE);
	*/

	window->_hbox = GTK_BOX (gtk_hbox_new (FALSE, 0));
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (window->_hbox));

	g_signal_connect (G_OBJECT (window), "expose-event", G_CALLBACK (shell_window_paint_window),
		NULL);
	window->resize_handler_id =
		g_signal_connect (G_OBJECT (window), "size-request",
		G_CALLBACK (shell_window_handle_size_request), app_data);

	return GTK_WIDGET (window);
}

static void
shell_window_destroy (GtkObject * obj)
{
}

void
shell_window_clear_resize_handler (ShellWindow * win)
{
	if (win->resize_handler_id)
	{
		g_signal_handler_disconnect (win, win->resize_handler_id);
		win->resize_handler_id = 0;
	}
}

/* We want the window to come up with proper runtime calculated width ( ie taking into account font size, locale, ...) so
   we can't hard code a size. But since ScrolledWindow returns basically zero for it's size request we need to
   grab the "real" desired width. Once it's shown though we want to allow the user to size down if they want too, so
   we unhook this function
*/
static void
shell_window_handle_size_request (GtkWidget * widget, GtkRequisition * requisition,
	AppShellData * app_data)
{
	/*
	Fixme - counting on this being called after the real size request is done.
	seems to be that way but I don't know why. I would think I would need to explictly call it here first
	printf("Enter - shell_window_handle_size_request\n");
	printf("passed in width:%d, height:%d\n", requisition->width, requisition->height);
	printf("left side width:%d\n", SHELL_WINDOW(widget)->_left_pane->requisition.width);
	printf("right side width:%d\n", GTK_WIDGET(APP_RESIZER(app_data->category_layout)->child)->requisition.width);
	*/

	requisition->width +=
		GTK_WIDGET (APP_RESIZER (app_data->category_layout)->child)->requisition.width;

	/* use the left side as a minimum height, if the right side is taller,
	   use it up to SIZING_HEIGHT_PERCENT of the screen height
	*/
	gint height =
		GTK_WIDGET (APP_RESIZER (app_data->category_layout)->child)->requisition.height +
		10;
	if (height > requisition->height)
	{
		requisition->height =
			MIN (((gfloat) gdk_screen_height () * SIZING_HEIGHT_PERCENT), height);
	}
}

void
shell_window_set_contents (ShellWindow * shell, GtkWidget * left_pane, GtkWidget * right_pane)
{
	shell->_left_pane = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	shell->_right_pane = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);

	gtk_alignment_set_padding (GTK_ALIGNMENT (shell->_left_pane), 15, 15, 15, 15);
	gtk_alignment_set_padding (GTK_ALIGNMENT (shell->_right_pane), 0, 0, 1, 0);	/* space for vertical line */

	gtk_box_pack_start (shell->_hbox, shell->_left_pane, FALSE, FALSE, 0);
	gtk_box_pack_start (shell->_hbox, shell->_right_pane, TRUE, TRUE, 0);	/* this one takes any extra space */

	gtk_container_add (GTK_CONTAINER (shell->_left_pane), left_pane);
	gtk_container_add (GTK_CONTAINER (shell->_right_pane), right_pane);
}

gboolean
shell_window_paint_window (GtkWidget * widget, GdkEventExpose * event, gpointer data)
{
	GList *child;
	GtkWidget *left_pane, *right_pane;

	left_pane = SHELL_WINDOW (widget)->_left_pane;
	right_pane = SHELL_WINDOW (widget)->_right_pane;

	/* draw left pane background */
	gdk_draw_rectangle (widget->window, widget->style->bg_gc[GTK_STATE_ACTIVE], TRUE,
		left_pane->allocation.x, left_pane->allocation.y, left_pane->allocation.width,
		left_pane->allocation.height);

	/* draw right pane background */
	/*
	   gdk_draw_rectangle (
	   widget->window,
	   widget->style->base_gc [GTK_STATE_NORMAL],
	   TRUE,
	   right_pane->allocation.x, right_pane->allocation.y,
	   right_pane->allocation.width, right_pane->allocation.height
	   );
	 */

	/* draw pane separator */
	GdkGC *line_gc = widget->style->fg_gc[GTK_STATE_INSENSITIVE];
	GdkGCValues values;
	gdk_gc_get_values (line_gc, &values);
	gint orig_line_width = values.line_width;
	values.line_width = 2;
	gdk_gc_set_values (line_gc, &values, GDK_GC_LINE_WIDTH);
	gdk_draw_line (widget->window, line_gc, right_pane->allocation.x, right_pane->allocation.y,
		right_pane->allocation.x,
		right_pane->allocation.y + right_pane->allocation.height - 1);
	values.line_width = orig_line_width;
	gdk_gc_set_values (line_gc, &values, GDK_GC_LINE_WIDTH);

	/*
	   gdk_gc_set_line_attributes(widget->style->black_gc, 2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_ROUND);
	   gdk_draw_line (
	   widget->window,
	   widget->style->black_gc,
	   right_pane->allocation.x, right_pane->allocation.y,
	   right_pane->allocation.x, right_pane->allocation.y + right_pane->allocation.height - 1
	   );
	 */

	child = gtk_container_get_children (GTK_CONTAINER (widget));
	for (; child; child = child->next)
		gtk_container_propagate_expose (GTK_CONTAINER (widget), GTK_WIDGET (child->data),
			event);

	return FALSE;
}
