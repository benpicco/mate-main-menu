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

#include <gtk/gtk.h>
#include "app-layout.h"

static gint aval_table_width = -1;

gint
calculate_launcher_width ()
{
	/* FIXME - right now we hard code the width, do we want to
	   dynamically determine the optimal size */
	return 230;
}

void
OLD_remove_container_entries (GtkContainer * widget)
{
	GList *children = gtk_container_get_children (widget);

	if (children == NULL)
		return;

	do {
		GtkWidget *child = GTK_WIDGET (children->data);
		GtkWidget *parent =
			gtk_widget_get_parent (GTK_WIDGET (child));
		if (parent) {
			gtk_container_remove (GTK_CONTAINER (parent),
					      GTK_WIDGET (child));
		}
	}
	while (NULL != (children = g_list_next (children)));
}

void
set_available_table_width (gint width)
{
	aval_table_width = width;
}

gint
get_available_table_width ()
{
	/* Make sure we set this before we call to get it */
	if (aval_table_width == -1)
		g_assert_not_reached ();

	return aval_table_width;
}

void
OLD_resize_table (GtkTable * table, GList * launcher_list)
{
	static gboolean initial_setup = TRUE;

	OLD_remove_container_entries (GTK_CONTAINER (table));

	gint launcher_width = calculate_launcher_width ();

	gint table_width = -1;

	if (initial_setup) {	/* should only be true when called before the 
				   window is shown - ie initial setup */
		initial_setup = FALSE;
		set_available_table_width (launcher_width * INITIAL_NUM_COLS);
		printf ("TABLE WIDTH NOT SET YET\n");
	}

	table_width = get_available_table_width ();

	gint columns = table_width / launcher_width;

	if (columns < 1) {
		printf ("resize_table() - columns less than 1\n");
		columns = 1;
	}
	float rows =
		((float) g_list_length (launcher_list)) / (float) columns;
	float remainder = rows - ((int) rows);

	if (remainder != 0.0)
		rows += 1;

	gtk_table_resize (table, (int) rows, columns);
}
