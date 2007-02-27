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
#include <NetworkManager.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <iwlib.h>
#include <glibtop/netlist.h>
#include <glibtop/netload.h>

#include "gnome-utils.h"

typedef struct
{
	DBusGConnection *nm_conn;
	DBusGProxy *nm_proxy;
} NetworkStatusAgentPrivate;

#define NETWORK_STATUS_AGENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NETWORK_STATUS_AGENT_TYPE, NetworkStatusAgentPrivate))

static void network_status_agent_dispose (GObject *);

static void init_nm_connection (NetworkStatusAgent *);

static NetworkStatusInfo *nm_get_first_active_device_info (NetworkStatusAgent *);
static GList *nm_get_devices (NetworkStatusAgent *);
static NetworkStatusInfo *nm_get_device_info (NetworkStatusAgent *, DBusGProxy *);

static void nm_state_change_cb (DBusGProxy *, guint, gpointer);
static DBusHandlerResult nm_message_filter (DBusConnection *, DBusMessage *, gpointer);

static gboolean string_is_valid_dbus_path (const gchar *);

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

	priv->nm_conn = NULL;
	priv->nm_proxy = NULL;
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

	if (!priv->nm_conn)
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
	/* FIXME */
}

static void
init_nm_connection (NetworkStatusAgent * agent)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (agent);

	GError *error = NULL;

	priv->nm_conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

	if (!priv->nm_conn)
	{
		handle_g_error (&error, "%s: dbus_g_bus_get () failed", G_STRFUNC);

		agent->nm_present = FALSE;

		return;
	}

	if (!dbus_bus_name_has_owner (dbus_g_connection_get_connection (priv->nm_conn),
			NM_DBUS_SERVICE, NULL))
	{
		agent->nm_present = FALSE;

		return;
	}

	agent->nm_present = TRUE;

	dbus_connection_set_exit_on_disconnect (dbus_g_connection_get_connection (priv->nm_conn),
		FALSE);

	priv->nm_proxy =
		dbus_g_proxy_new_for_name (priv->nm_conn, NM_DBUS_SERVICE, NM_DBUS_PATH,
		NM_DBUS_INTERFACE);

	dbus_g_proxy_add_signal (priv->nm_proxy, NM_DBUS_SIGNAL_STATE_CHANGE, G_TYPE_UINT,
		G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (priv->nm_proxy, NM_DBUS_SIGNAL_STATE_CHANGE,
		G_CALLBACK (nm_state_change_cb), agent, NULL);

	dbus_connection_add_filter (dbus_g_connection_get_connection (priv->nm_conn),
		nm_message_filter, agent, NULL);
}

static NetworkStatusInfo *
nm_get_first_active_device_info (NetworkStatusAgent * agent)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (agent);

	NetworkStatusInfo *info = NULL;

	GList *devices;
	GList *node;

	if (!priv->nm_conn)
		return NULL;

	devices = nm_get_devices (agent);

	for (node = devices; node; node = node->next)
	{
		info = nm_get_device_info (agent, (DBusGProxy *) node->data);

		if (info)
		{
			if (info->active)
				break;

			g_object_unref (info);

			info = NULL;
		}
	}

	/* FIXME: free contents of devices ? */
	g_list_free (devices);

	return info;
}

static GList *
nm_get_devices (NetworkStatusAgent * agent)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (agent);

	GPtrArray *ptr_array;
	gint i;

	GList *devices = NULL;

	DBusGProxy *proxy;

	GError *error = NULL;

	dbus_g_proxy_call (priv->nm_proxy, "getDevices", &error, G_TYPE_INVALID,
		dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_PROXY), &ptr_array,
		G_TYPE_INVALID);

	if (error)
	{
		handle_g_error (&error, "%s: calling \"getDevices\" failed", G_STRFUNC);

		return NULL;
	}

	for (i = 0; i < ptr_array->len; i++)
	{
		proxy = (DBusGProxy *) g_ptr_array_index (ptr_array, i);

		devices = g_list_append (devices, proxy);
	}
	g_ptr_array_free (ptr_array, TRUE);

	return devices;
}

static NetworkStatusInfo *
nm_get_device_info (NetworkStatusAgent * agent, DBusGProxy * device)
{
	NetworkStatusAgentPrivate *priv = NETWORK_STATUS_AGENT_GET_PRIVATE (agent);
	NetworkStatusInfo *info = g_object_new (NETWORK_STATUS_INFO_TYPE, NULL);

	DBusGProxy *proxy;
	gchar *network_path = NULL;

	GError *error = NULL;

	proxy = dbus_g_proxy_new_for_name (priv->nm_conn, NM_DBUS_SERVICE,
		dbus_g_proxy_get_path (device), NM_DBUS_INTERFACE_DEVICES);

	dbus_g_proxy_call (proxy, "getProperties", &error, G_TYPE_INVALID,
		DBUS_TYPE_G_PROXY, NULL,
		G_TYPE_STRING, &info->iface,
		G_TYPE_UINT, &info->type,
		G_TYPE_STRING, NULL,
		G_TYPE_BOOLEAN, &info->active,
		G_TYPE_UINT, NULL,
		G_TYPE_STRING, &info->ip4_addr,
		G_TYPE_STRING, &info->subnet_mask,
		G_TYPE_STRING, &info->broadcast,
		G_TYPE_STRING, &info->hw_addr,
		G_TYPE_STRING, &info->route,
		G_TYPE_STRING, &info->primary_dns,
		G_TYPE_STRING, &info->secondary_dns,
		G_TYPE_INT, NULL,
		G_TYPE_INT, NULL,
		G_TYPE_BOOLEAN, NULL,
		G_TYPE_INT, &info->speed_mbs,
		G_TYPE_UINT, NULL,
		G_TYPE_UINT, NULL,
		G_TYPE_STRING, &network_path,
		G_TYPE_STRV, NULL,
		G_TYPE_INVALID);

	if (error)
	{
		handle_g_error (&error, "%s: calling \"getProperties\" (A) failed", G_STRFUNC);

		g_free (network_path);
		g_object_unref (info);

		return NULL;
	}

	if (info->active)
	{
		dbus_g_proxy_call (proxy, "getDriver", &error, G_TYPE_INVALID, G_TYPE_STRING,
			&info->driver, G_TYPE_INVALID);

		if (error)
			handle_g_error (&error, "%s: calling \"getDriver\" failed", G_STRFUNC);

		if (info->type == DEVICE_TYPE_802_11_WIRELESS
			&& string_is_valid_dbus_path (network_path))
		{
			proxy = dbus_g_proxy_new_for_name (priv->nm_conn, NM_DBUS_SERVICE,
				network_path, NM_DBUS_INTERFACE_DEVICES);

			dbus_g_proxy_call (proxy, "getProperties", &error, G_TYPE_INVALID,
				DBUS_TYPE_G_PROXY, NULL,
				G_TYPE_STRING, &info->essid,
				G_TYPE_STRING, NULL,
				G_TYPE_INT, NULL,
				G_TYPE_DOUBLE, NULL,
				G_TYPE_INT, NULL,
				G_TYPE_BOOLEAN, NULL,
				G_TYPE_INT, NULL,
				G_TYPE_BOOLEAN, NULL,
				G_TYPE_INVALID);
		}

		if (error)
			handle_g_error (&error, "%s: calling \"getProperties\" (B) failed",
				G_STRFUNC);
	}

	g_free (network_path);

	return info;
}

static void
nm_state_change_cb (DBusGProxy * proxy, guint state, gpointer user_data)
{
	g_signal_emit (NETWORK_STATUS_AGENT (user_data),
		network_status_agent_signals[STATUS_CHANGED], 0);
}

static DBusHandlerResult
nm_message_filter (DBusConnection * nm_conn, DBusMessage * msg, gpointer user_data)
{
	if ((dbus_message_is_signal (msg, DBUS_INTERFACE_LOCAL, "Disconnected")
			&& !strcmp (dbus_message_get_path (msg), DBUS_PATH_LOCAL))
		|| dbus_message_is_signal (msg, DBUS_INTERFACE_DBUS, "NameOwnerChanged"))
	{
		init_nm_connection (NETWORK_STATUS_AGENT (user_data));

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*
 * This macro and function are copied from dbus-marshal-validate.c
 */

#define VALID_NAME_CHARACTER(c) (	\
	((c) >= '0' && (c) <= '9') ||	\
	((c) >= 'A' && (c) <= 'Z') ||	\
	((c) >= 'a' && (c) <= 'z') ||	\
	((c) == '_')			\
)

static gboolean
string_is_valid_dbus_path (const gchar * str)
{
	const gchar *s;
	const gchar *end;
	const gchar *last_slash;

	gint len;

	len = strlen (str);

	s = str;
	end = s + len;

	if (*s != '/')
		return FALSE;
	last_slash = s;
	++s;

	while (s != end)
	{
		if (*s == '/')
		{
			if ((s - last_slash) < 2)
				return FALSE;	/* no empty path components allowed */

			last_slash = s;
		}
		else
		{
			if (!VALID_NAME_CHARACTER (*s))
				return FALSE;
		}

		++s;
	}

	if ((end - last_slash) < 2 && len > 1)
		return FALSE;	/* trailing slash not allowed unless the string is "/" */

	return TRUE;
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

