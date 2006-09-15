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

#include "text-button.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtklabel.h>

#include "double-click-detector.h"

typedef struct
{
	GtkLabel *text_label;

	DoubleClickDetector *double_click_detector;
} TextButtonPrivate;

#define TEXT_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEXT_BUTTON_TYPE, TextButtonPrivate))

static void text_button_class_init (TextButtonClass *);
static void text_button_init (TextButton *);
static void text_button_finalize (GObject *);

static gboolean text_button_button_press (GtkWidget *, GdkEventButton *);
static gboolean text_button_key_release (GtkWidget *, GdkEventKey *);
static gboolean text_button_expose (GtkWidget *, GdkEventExpose *);
static gboolean text_button_enter_notify (GtkWidget *, GdkEventCrossing *);
static gboolean text_button_leave_notify (GtkWidget *, GdkEventCrossing *);

enum
{
	ACTIVATE_SIGNAL,
	LAST_SIGNAL
};

static guint text_button_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (TextButton, text_button, GTK_TYPE_EVENT_BOX)
     static void text_button_class_init (TextButtonClass * text_button_class)
{
	GObjectClass *g_obj_class;
	GtkWidgetClass *widget_class;

	g_obj_class = G_OBJECT_CLASS (text_button_class);
	widget_class = GTK_WIDGET_CLASS (text_button_class);

	g_type_class_add_private (text_button_class, sizeof (TextButtonPrivate));

	widget_class->button_press_event = text_button_button_press;
	widget_class->key_release_event = text_button_key_release;
	widget_class->expose_event = text_button_expose;
	widget_class->enter_notify_event = text_button_enter_notify;
	widget_class->leave_notify_event = text_button_leave_notify;

	g_obj_class->finalize = text_button_finalize;

	text_button_signals[ACTIVATE_SIGNAL] = g_signal_new ("activate",
		G_TYPE_FROM_CLASS (text_button_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (TextButtonClass, activate),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
text_button_init (TextButton * button)
{
	TextButtonPrivate *priv;

	gtk_event_box_set_visible_window (GTK_EVENT_BOX (button), FALSE);
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (button), GTK_CAN_FOCUS);

	priv = TEXT_BUTTON_GET_PRIVATE (button);

	priv->text_label = GTK_LABEL (gtk_label_new (""));
	gtk_label_set_use_markup (priv->text_label, TRUE);
	gtk_widget_modify_fg (GTK_WIDGET (priv->text_label), GTK_STATE_NORMAL,
		&GTK_WIDGET (priv->text_label)->style->base[GTK_STATE_SELECTED]);
	gtk_container_add (GTK_CONTAINER (button), GTK_WIDGET (priv->text_label));

	priv->double_click_detector = double_click_detector_new ();
}

GtkWidget *
text_button_new (const gchar * text)
{
	TextButton *button;
	TextButtonPrivate *priv;

	gchar *markup;

	button = g_object_new (TEXT_BUTTON_TYPE, NULL);
	priv = TEXT_BUTTON_GET_PRIVATE (button);

	button->text = g_strdup (text);

	markup = g_strdup_printf ("<span underline=\"single\">%s</span>", button->text);

	gtk_label_set_markup (priv->text_label, markup);

	g_free (markup);

	return GTK_WIDGET (button);
}

static void
text_button_finalize (GObject * object)
{
	TextButton *button;
	TextButtonPrivate *priv;

	button = TEXT_BUTTON (object);
	priv = TEXT_BUTTON_GET_PRIVATE (button);

	g_free (button->text);
	g_object_unref (priv->double_click_detector);
	(*G_OBJECT_CLASS (text_button_parent_class)->finalize) (object);
}

static gboolean
text_button_button_press (GtkWidget * widget, GdkEventButton * event)
{
	TextButtonPrivate *priv = TEXT_BUTTON_GET_PRIVATE (widget);

	if (event->button == 1)
	{
		if (!double_click_detector_is_double_click (priv->double_click_detector,
				event->time, TRUE))
			g_signal_emit (widget, text_button_signals[ACTIVATE_SIGNAL], 0);

		return TRUE;
	}

	return FALSE;
}

static gboolean
text_button_key_release (GtkWidget * widget, GdkEventKey * event)
{
	if (event->keyval == GDK_Return)
	{
		g_signal_emit (widget, text_button_signals[ACTIVATE_SIGNAL], 0);

		return TRUE;
	}

	return FALSE;
}

static gboolean
text_button_expose (GtkWidget * widget, GdkEventExpose * event)
{
	GList *child;

	if (GTK_WIDGET_HAS_FOCUS (widget))
		gtk_paint_focus (widget->style, widget->window, widget->state, &event->area, widget,
			NULL, widget->allocation.x, widget->allocation.y, widget->allocation.width,
			widget->allocation.height);

	child = gtk_container_get_children (GTK_CONTAINER (widget));

	for (; child; child = child->next)
		gtk_container_propagate_expose (GTK_CONTAINER (widget), GTK_WIDGET (child->data),
			event);

	return FALSE;
}

static gboolean
text_button_enter_notify (GtkWidget * widget, GdkEventCrossing * event)
{
	gdk_window_set_cursor (widget->window, gdk_cursor_new (GDK_HAND1));

	return TRUE;
}

static gboolean
text_button_leave_notify (GtkWidget * widget, GdkEventCrossing * event)
{
	gdk_window_set_cursor (widget->window, NULL);

	return TRUE;
}
