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

#ifndef __NETWORK_STATUS_INFO_H_
#define __NETWORK_STATUS_INFO_H_

#include <glib-object.h>
#include <NetworkManager.h>

G_BEGIN_DECLS
#define NETWORK_STATUS_INFO_TYPE         (network_status_info_get_type ())
#define NETWORK_STATUS_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NETWORK_STATUS_INFO_TYPE, NetworkStatusInfo))
#define NETWORK_STATUS_INFO_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), NETWORK_STATUS_INFO_TYPE, NetworkStatusInfoClass))
#define IS_NETWORK_STATUS_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NETWORK_STATUS_INFO_TYPE))
#define IS_NETWORK_STATUS_INFO_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), NETWORK_STATUS_INFO_TYPE))
#define NETWORK_STATUS_INFO_GET_CLASS(o) (G_TYPE_CHECK_GET_CLASS ((o), NETWORK_STATUS_INFO_TYPE, NetworkStatusInfoClass))
	typedef struct {
	GObject parent_placeholder;

	gboolean active;
	NMDeviceType type;
	gchar *iface;
	gchar *essid;

	gchar *driver;
	gchar *ip4_addr;
	gchar *broadcast;
	gchar *subnet_mask;
	gchar *route;
	gchar *primary_dns;
	gchar *secondary_dns;
	gint speed_mbs;
	gchar *hw_addr;
} NetworkStatusInfo;

typedef struct {
	GObjectClass parent_class;
} NetworkStatusInfoClass;

GType network_status_info_get_type (void);

G_END_DECLS
#endif /* __NETWORK_STATUS_INFO_H_ */
