/*
 * This file is part of the Control Center.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Control Center; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

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

#include "app-window.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkframe.h>
#include <gtk/gtklayout.h>
#include <gtk/gtkscrolledwindow.h>

#include "app-layout.h"

static GtkWindowClass *parent_class = NULL;

static void slab_window_class_init (SlabWindowClass *);
static void slab_window_init (SlabWindow *);
static void slab_window_destroy (GtkObject *);

gboolean paint_window (GtkWidget * widget, GdkEventExpose * event,
		       gpointer data);

#define SLAB_WINDOW_BORDER_WIDTH 6

GType
slab_window_get_type (void)
{
	printf ("ENTER - SLAB-slab_window_get_type\n");
	static GType object_type = 0;

	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (SlabWindowClass),
			NULL,
			NULL,
			(GClassInitFunc) slab_window_class_init,
			NULL,
			NULL,
			sizeof (SlabWindow),
			0,
			(GInstanceInitFunc) slab_window_init
		};

		object_type = g_type_register_static (GTK_TYPE_FRAME,
						      "SlabWindow",
						      &object_info, 0);
	}

	return object_type;
}

static void
slab_window_class_init (SlabWindowClass * klass)
{
	printf ("AAAAAAAAAAA\n\n");
	parent_class = g_type_class_peek_parent (klass);

	((GtkObjectClass *) klass)->destroy = slab_window_destroy;
}

static void
slab_window_init (SlabWindow * window)
{
	printf ("AAAAAAAAAAA\n\n");
	window->_hbox = NULL;
	window->_left_pane = NULL;
	window->_right_pane = NULL;
}

GtkWidget *
slab_window_new ()
{
	printf ("AAAAAAAAAAA\n\n");
	SlabWindow *window = g_object_new (SLAB_WINDOW_TYPE, NULL);

	gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);

	window->_hbox = GTK_BOX (gtk_hbox_new (FALSE, 0));
	gtk_container_add (GTK_CONTAINER (window),
			   GTK_WIDGET (window->_hbox));

	g_signal_connect (G_OBJECT (window),
			  "expose-event", G_CALLBACK (paint_window), NULL);

	return GTK_WIDGET (window);
}

static void
slab_window_destroy (GtkObject * obj)
{
	printf ("AAAAAAAAAAA\n\n");
}

void
slab_window_reset_contents (SlabWindow * slab, GtkWidget * left_pane,
			    GtkWidget * right_pane)
{
	printf ("AAAAAAAAAAA\n\n");
	GList *children =
		gtk_container_get_children (GTK_CONTAINER (slab->_left_pane));
	gtk_container_remove (GTK_CONTAINER (slab->_left_pane),
			      GTK_WIDGET (children->data));
	children = g_list_next (children);	/* should only have one child 
						   in list */
	g_assert (children == NULL);

	children =
		gtk_container_get_children (GTK_CONTAINER
					    (slab->_right_pane));
	gtk_container_remove (GTK_CONTAINER (slab->_right_pane),
			      GTK_WIDGET (children->data));
	children = g_list_next (children);	/* should only have one child 
						   in list */
	g_assert (children == NULL);

	gtk_container_add (GTK_CONTAINER (slab->_left_pane), left_pane);
	gtk_container_add (GTK_CONTAINER (slab->_right_pane), right_pane);
}

void
slab_window_set_contents (SlabWindow * slab, GtkWidget * left_pane,
			  GtkWidget * right_pane)
{
	printf ("AAAAAAAAAAA\n\n");
	slab->_left_pane = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	slab->_right_pane = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);

	gtk_alignment_set_padding (GTK_ALIGNMENT (slab->_left_pane), 15, 15,
				   15, 15);
	gtk_alignment_set_padding (GTK_ALIGNMENT (slab->_right_pane), 0, 0, 2,
				   0);

	gtk_box_pack_start (slab->_hbox, slab->_left_pane, FALSE, FALSE, 0);
	gtk_box_pack_start (slab->_hbox, slab->_right_pane, TRUE, TRUE, 0);	/* this 
										   one 
										   takes 
										   any 
										   extra 
										   space 
										 */

	gtk_container_add (GTK_CONTAINER (slab->_left_pane), left_pane);
	gtk_container_add (GTK_CONTAINER (slab->_right_pane), right_pane);
}

gboolean
paint_window (GtkWidget * widget, GdkEventExpose * event, gpointer data)
{
	printf ("ENTER - SLAB-paint_window\n");
	GList *child;
	GtkWidget *left_pane, *right_pane;

	left_pane = SLAB_WINDOW (widget)->_left_pane;
	right_pane = SLAB_WINDOW (widget)->_right_pane;

	/* draw left pane background */

	gdk_draw_rectangle (widget->window,
			    widget->style->bg_gc [GTK_STATE_ACTIVE],
			    TRUE,
			    left_pane->allocation.x, left_pane->allocation.y,
			    left_pane->allocation.width,
			    left_pane->allocation.height);

	/* draw right pane background */

	gdk_draw_rectangle (widget->window,
			    widget->style->bg_gc [GTK_STATE_ACTIVE],
			    TRUE,
			    right_pane->allocation.x,
			    right_pane->allocation.y,
			    right_pane->allocation.width,
			    right_pane->allocation.height);

	/* draw pane separator */

	gdk_draw_line (widget->window,
		       widget->style->dark_gc [GTK_STATE_NORMAL],
		       right_pane->allocation.x, right_pane->allocation.y,
		       right_pane->allocation.x,
		       right_pane->allocation.y +
		       right_pane->allocation.height - 1);

	child = gtk_container_get_children (GTK_CONTAINER (widget));

	for (; child; child = child->next)
		gtk_container_propagate_expose (GTK_CONTAINER (widget),
						GTK_WIDGET (child->data),
						event);

	return FALSE;
}
