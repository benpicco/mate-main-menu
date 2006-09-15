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

#include "network-status-info.h"

G_DEFINE_TYPE (NetworkStatusInfo, network_status_info, G_TYPE_OBJECT)

static void network_status_info_finalize (GObject *);

static void network_status_info_class_init (NetworkStatusInfoClass * this_class)
{
	GObjectClass *g_obj_class = (GObjectClass *) this_class;

	g_obj_class->finalize = network_status_info_finalize;
}

static void
network_status_info_init (NetworkStatusInfo * info)
{
	info->active = FALSE;
	info->type = DEVICE_TYPE_UNKNOWN;
	info->iface = NULL;
	info->essid = NULL;

	info->driver = NULL;
	info->ip4_addr = NULL;
	info->broadcast = NULL;
	info->subnet_mask = NULL;
	info->route = NULL;
	info->primary_dns = NULL;
	info->secondary_dns = NULL;
	info->speed_mbs = -1;
	info->hw_addr = NULL;
}

static void
network_status_info_finalize (GObject * obj)
{
	NetworkStatusInfo *info = NETWORK_STATUS_INFO (obj);
	if (info->iface)
		g_free (info->iface);
	if (info->essid)
		g_free (info->essid);
	if (info->driver)
		g_free (info->driver);
	if (info->ip4_addr)
		g_free (info->ip4_addr);
	if (info->broadcast)
		g_free (info->broadcast);
	if (info->subnet_mask)
		g_free (info->subnet_mask);
	if (info->route)
		g_free (info->route);
	if (info->primary_dns)
		g_free (info->primary_dns);
	if (info->secondary_dns)
		g_free (info->secondary_dns);
	if (info->hw_addr)
		g_free (info->hw_addr);

	(*G_OBJECT_CLASS (network_status_info_parent_class)->finalize) (obj);
}
