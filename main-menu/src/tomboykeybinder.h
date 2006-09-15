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

#ifndef __TOMBOY_KEY_BINDER_H__
#define __TOMBOY_KEY_BINDER_H__

#include <glib/gtypes.h>

G_BEGIN_DECLS

typedef void (*TomboyBindkeyHandler) (char *keystring, gpointer user_data);

void tomboy_keybinder_init (void);

void tomboy_keybinder_bind (const char *keystring, TomboyBindkeyHandler handler,
	gpointer user_data);

void tomboy_keybinder_unbind (const char *keystring, TomboyBindkeyHandler handler);

gboolean tomboy_keybinder_is_modifier (guint keycode);

guint32 tomboy_keybinder_get_current_event_time (void);

G_END_DECLS
#endif /* __TOMBOY_KEY_BINDER_H__ */
