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

#include "slab-window.h"

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

#include "slab-gnome-util.h"

G_DEFINE_TYPE (SlabWindow, slab_window, GTK_TYPE_WINDOW)

static void slab_window_size_request (GtkWidget *, GtkRequisition *);
static void slab_window_size_allocate (GtkWidget *, GtkAllocation *);

gboolean paint_window (GtkWidget *, GdkEventExpose *, gpointer);

#define SLAB_WINDOW_BORDER_WIDTH 6

static void slab_window_class_init (SlabWindowClass * this_class)
{
	GObjectClass *g_obj_class;
	GtkWidgetClass *widget_class;

	g_obj_class = G_OBJECT_CLASS (this_class);
	widget_class = GTK_WIDGET_CLASS (this_class);

	widget_class->size_request = slab_window_size_request;
	widget_class->size_allocate = slab_window_size_allocate;
}

static void
slab_window_init (SlabWindow * window)
{
	window->_hbox = NULL;
	window->_left_pane = NULL;
	window->_right_pane = NULL;
}

GtkWidget *
slab_window_new ()
{
	SlabWindow *this = g_object_new (SLAB_WINDOW_TYPE,
		"type-hint", GDK_WINDOW_TYPE_HINT_DOCK,
		"decorated", FALSE,
		"app-paintable", TRUE,
		NULL);

	this->_hbox = GTK_BOX (gtk_hbox_new (FALSE, 0));
	gtk_container_set_border_width (GTK_CONTAINER (this->_hbox), SLAB_WINDOW_BORDER_WIDTH);
	gtk_container_add (GTK_CONTAINER (this), GTK_WIDGET (this->_hbox));

	g_signal_connect (G_OBJECT (this), "expose-event", G_CALLBACK (paint_window), NULL);

	return GTK_WIDGET (this);
}

void
slab_window_set_contents (SlabWindow * slab, GtkWidget * left_pane, GtkWidget * right_pane)
{
	slab->_left_pane = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	slab->_right_pane = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);

	gtk_alignment_set_padding (GTK_ALIGNMENT (slab->_left_pane), 15, 15, 15, 15);
	gtk_alignment_set_padding (GTK_ALIGNMENT (slab->_right_pane), 15, 15, 15, 15);

	gtk_box_pack_start (slab->_hbox, slab->_left_pane, FALSE, FALSE, 0);
	gtk_box_pack_start (slab->_hbox, slab->_right_pane, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (slab->_left_pane), left_pane);
	gtk_container_add (GTK_CONTAINER (slab->_right_pane), right_pane);

	gtk_widget_show_all (GTK_WIDGET (slab->_hbox));
}

static void
slab_window_size_request (GtkWidget * widget, GtkRequisition * req)
{
	GtkWidgetClass *parent_class;

	parent_class = GTK_WIDGET_CLASS (g_type_class_peek_parent (SLAB_WINDOW_GET_CLASS (widget)));

	if (parent_class->size_request)
		(*parent_class->size_request) (widget, req);

	gtk_widget_queue_draw (widget);
}

static void
slab_window_size_allocate (GtkWidget * widget, GtkAllocation * alloc)
{
	GtkWidgetClass *parent_class;
	GtkWidget *child;

	child = gtk_bin_get_child (GTK_BIN (widget));

	alloc->width = child->requisition.width;
	alloc->height = child->requisition.height;

	parent_class = GTK_WIDGET_CLASS (g_type_class_peek_parent (SLAB_WINDOW_GET_CLASS (widget)));

	if (parent_class->size_allocate)
		(*parent_class->size_allocate) (widget, alloc);

	if (GDK_IS_WINDOW (widget->window))
		gdk_window_resize (widget->window, alloc->width, alloc->height);

	gtk_widget_queue_draw (widget);
}

gboolean
paint_window (GtkWidget * widget, GdkEventExpose * event, gpointer data)
{
	GList *child;
	GtkWidget *left_pane, *right_pane;

	/* draw colored border */

	gdk_draw_rectangle (widget->window, widget->style->bg_gc[GTK_STATE_SELECTED], TRUE,
		widget->allocation.x, widget->allocation.y, widget->allocation.width - 1,
		widget->allocation.height - 1);

	/* draw black outer outline */

	gdk_draw_rectangle (widget->window, widget->style->black_gc, FALSE, widget->allocation.x,
		widget->allocation.y, widget->allocation.width - 1, widget->allocation.height - 1);

	/* draw inner outline */

	gdk_draw_rectangle (widget->window, widget->style->fg_gc[GTK_STATE_INSENSITIVE], FALSE,
		widget->allocation.x + SLAB_WINDOW_BORDER_WIDTH - 1,
		widget->allocation.y + SLAB_WINDOW_BORDER_WIDTH - 1,
		widget->allocation.width - 2 * SLAB_WINDOW_BORDER_WIDTH + 1,
		widget->allocation.height - 2 * SLAB_WINDOW_BORDER_WIDTH + 1);

	left_pane = SLAB_WINDOW (widget)->_left_pane;
	right_pane = SLAB_WINDOW (widget)->_right_pane;

	/* draw left pane background */

	gdk_draw_rectangle (widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
		left_pane->allocation.x, left_pane->allocation.y, left_pane->allocation.width,
		left_pane->allocation.height);

	/* draw right pane background */

	gdk_draw_rectangle (widget->window, widget->style->bg_gc[GTK_STATE_ACTIVE], TRUE,
		right_pane->allocation.x, right_pane->allocation.y, right_pane->allocation.width,
		right_pane->allocation.height);

	/* draw pane separator */

	gdk_draw_line (widget->window, widget->style->dark_gc[GTK_STATE_NORMAL],
		right_pane->allocation.x, right_pane->allocation.y, right_pane->allocation.x,
		right_pane->allocation.y + right_pane->allocation.height - 1);

	child = gtk_container_get_children (GTK_CONTAINER (widget));

	for (; child; child = child->next)
		gtk_container_propagate_expose (GTK_CONTAINER (widget), GTK_WIDGET (child->data),
			event);

	return FALSE;
}
