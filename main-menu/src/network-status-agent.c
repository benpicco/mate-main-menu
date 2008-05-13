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

#include "network-status-agent.h"

#include <string.h>
#include <libnm-glib/nm-client.h>
#include <NetworkManager.h>
#include <libnm-glib/nm-device-802-11-wireless.h>
#include <libnm-glib/nm-device-802-3-ethernet.h>
#include <nm-setting-ip4-config.h>
#include <nm-utils.h>
#include <arpa/inet.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <iwlib.h>
#include <glibtop/netlist.h>
#include <glibtop/netload.h>

#include "gnome-utils.h"

typedef struct
{
	NMClient * nm_client;
	guint state_curr;
} NetworkStatusAgentPrivate;

#define NETWORK_STATUS_AGENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NETWORK_STATUS_AGENT_TYPE, NetworkStatusAgentPrivate))

static void network_status_agent_dispose (GObject *);

static void init_nm_connection (NetworkStatusAgent *);

static NetworkStatusInfo *nm_get_first_active_device_info (NetworkStatusAgent *);
static NetworkStatusInfo *nm_get_device_info (NetworkStatusAgent *, NMDevice *);

static void nm_state_change_cb (NMDevice *device, GParamSpec *pspec, gpointer user_data);

static NetworkStatusInfo *gtop_get_first_active_device_info (void);

enum
{
	STATUS_CHANGED,
	LAST_SIGNAL
};

static guint network_status_agent_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NetworkStatusAgent, network_status_agent, G_TYPE_OBJECT)

static void network_status_agent_class_init (NetworkStatusAgentClass * this_class)
{
	GObjectClass *g_obj_class = (GObjectClass *) this_class;

	g_obj_class->dispose = network_status_agent_dispose;

	network_status_agent_signals[STATUS_CHANGED] = g_signal_new ("status-changed",
		G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (NetworkStatusAgentClass, status_changed),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	g_type_class_add_private (this_class, sizeof (NetworkStatusAgentPrivate));
}

static void
network_status_agent_init (NetworkStatusAgent * agent)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (agent);

	agent->nm_present = FALSE;

	priv->nm_client = NULL;
	priv->state_curr = 0;
}

NetworkStatusAgent *
network_status_agent_new ()
{
	return g_object_new (NETWORK_STATUS_AGENT_TYPE, NULL);
}

NetworkStatusInfo *
network_status_agent_get_first_active_device_info (NetworkStatusAgent * agent)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (agent);
	NetworkStatusInfo *info = NULL;

	if (!priv->nm_client)
		init_nm_connection (agent);

	if (agent->nm_present)
		info = nm_get_first_active_device_info (agent);
	else
		info = gtop_get_first_active_device_info ();

	return info;
}

static void
network_status_agent_dispose (GObject * obj)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (obj);
	if (priv->nm_client) {
		g_object_unref (priv->nm_client);
		priv->nm_client = NULL;
	}
}

static void
init_nm_connection (NetworkStatusAgent * agent)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (agent);

	priv->nm_client = nm_client_new();

	if (!(priv->nm_client && nm_client_get_manager_running (priv->nm_client)))
	{
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "nm_client_new failed");

		agent->nm_present = FALSE;

		return;
	}

	agent->nm_present = TRUE;
}

static NetworkStatusInfo *
nm_get_first_active_device_info (NetworkStatusAgent * agent)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (agent);

	NetworkStatusInfo *info = NULL;

	const GPtrArray *devices;
	gint i;

	if (!priv->nm_client)
		return NULL;

	devices = nm_client_get_devices (priv->nm_client);

	for (i = 0; devices && i < devices->len; i++)
	{
		NMDevice *nm_device;

		nm_device = NM_DEVICE (g_ptr_array_index (devices, i));
		info = nm_get_device_info (agent, nm_device);

		if (info)
		{
			if (info->active)
			{
				g_signal_connect (nm_device, "notify::state", G_CALLBACK (nm_state_change_cb), agent);
				break;
			}

			g_object_unref (info);

			info = NULL;
		}
	}

	return info;
}

static gchar *
ip4_address_as_string (guint32 ip)
{
	struct in_addr tmp_addr;
	gchar *ip_string;

	tmp_addr.s_addr = ip;
	ip_string = inet_ntoa (tmp_addr);

	return g_strdup (ip_string);
}

static NetworkStatusInfo *
nm_get_device_info (NetworkStatusAgent * agent, NMDevice * device)
{
	NetworkStatusInfo *info = g_object_new (NETWORK_STATUS_INFO_TYPE, NULL);
	const GArray *array;
	NMSettingIP4Address *def_addr;
	guint32 hostmask, network, bcast;

	info->iface = g_strdup (nm_device_get_iface (device));
	info->driver = g_strdup (nm_device_get_driver (device));
	info->active = (nm_device_get_state (device) == NM_DEVICE_STATE_ACTIVATED) ? TRUE : FALSE;
	if (! info->active)
		return info;
	NMIP4Config * cfg = nm_device_get_ip4_config (device);
	if(! cfg)
		return info;

	def_addr = nm_ip4_config_get_addresses (cfg);
	if (def_addr) {
		info->ip4_addr = ip4_address_as_string (def_addr->address);
		info->subnet_mask = ip4_address_as_string (def_addr->netmask);
		info->route = ip4_address_as_string (def_addr->gateway);

		network = ntohl (def_addr->address) & ntohl (def_addr->netmask);
		hostmask = ~ntohl (def_addr->netmask);
		bcast = htonl (network | hostmask);
		info->broadcast = ip4_address_as_string (bcast);
	}

	info->primary_dns = NULL;
	info->secondary_dns = NULL;
	array = nm_ip4_config_get_nameservers (cfg);
	if (array)
	{
		if (array->len > 0)
			info->primary_dns = ip4_address_as_string (g_array_index (array, guint32, 0));
		if (array->len > 1)
			info->secondary_dns = ip4_address_as_string (g_array_index (array, guint32, 1));
	}

	if (NM_IS_DEVICE_802_11_WIRELESS(device))
	{
		const GPtrArray *aps;
		gint i;
		info->type = DEVICE_TYPE_802_11_WIRELESS;

		info->speed_mbs = nm_device_802_11_wireless_get_bitrate (NM_DEVICE_802_11_WIRELESS(device));
		info->hw_addr = g_strdup (nm_device_802_11_wireless_get_hw_address (NM_DEVICE_802_11_WIRELESS(device)));
		aps = nm_device_802_11_wireless_get_access_points (NM_DEVICE_802_11_WIRELESS(device));
		for (i = 0; aps && i < aps->len; i++)
		{
			const GByteArray * ssid;
			ssid = nm_access_point_get_ssid (NM_ACCESS_POINT (g_ptr_array_index (aps, i)));
			if (ssid)
				info->essid = g_strdup (nm_utils_escape_ssid (ssid->data, ssid->len));
			else
				info->essid = g_strdup ("(none)");
			break; //fixme - we only show one for now
		}
	}
	else if (NM_IS_DEVICE_802_3_ETHERNET (device))
	{
		info->type = DEVICE_TYPE_802_3_ETHERNET;
		info->speed_mbs = nm_device_802_3_ethernet_get_speed (NM_DEVICE_802_3_ETHERNET(device));
		info->hw_addr = g_strdup (nm_device_802_3_ethernet_get_hw_address (NM_DEVICE_802_3_ETHERNET(device)));
	}

	return info;
}

static void
nm_state_change_cb (NMDevice *device, GParamSpec *pspec, gpointer user_data)
{
	NetworkStatusAgent        *this = NETWORK_STATUS_AGENT (user_data);
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (this);
	NMDeviceState              state;

	state = nm_device_get_state (device);

	if (priv->state_curr == state)
		return;

	priv->state_curr = state;

	g_signal_emit (this, network_status_agent_signals [STATUS_CHANGED], 0);
}

#define CHECK_FLAG(flags, offset) (((flags) & (1 << (offset))) ? TRUE : FALSE)

static NetworkStatusInfo *
gtop_get_first_active_device_info ()
{
	NetworkStatusInfo *info = NULL;

	glibtop_netlist net_list;
	gchar **networks;

	glibtop_netload net_load;

	gint sock_fd;
	wireless_config wl_cfg;

	gint ret;
	gint i;

	networks = glibtop_get_netlist (&net_list);

	for (i = 0; i < net_list.number; ++i)
	{
		glibtop_get_netload (&net_load, networks[i]);

		if (CHECK_FLAG (net_load.if_flags, GLIBTOP_IF_FLAGS_RUNNING)
			&& !CHECK_FLAG (net_load.if_flags, GLIBTOP_IF_FLAGS_LOOPBACK))
		{
			sock_fd = iw_sockets_open ();

			if (sock_fd <= 0)
			{
				g_warning ("error opening socket\n");

				info = NULL;
				break;
			}

			info = g_object_new (NETWORK_STATUS_INFO_TYPE, NULL);

			ret = iw_get_basic_config (sock_fd, networks[i], &wl_cfg);

			if (ret >= 0)
			{
				info->type = DEVICE_TYPE_802_11_WIRELESS;
				info->essid = g_strdup (wl_cfg.essid);
				info->iface = g_strdup (networks[i]);

				break;
			}
			else
			{
				info->type = DEVICE_TYPE_802_3_ETHERNET;
				info->essid = NULL;
				info->iface = g_strdup (networks[i]);

				break;
			}
		}
	}

	g_strfreev (networks);
	return info;
}

