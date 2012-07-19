#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

static void
run_dialog (GdkDisplay *display, GdkScreen  *screen, guint32 timestamp)
{
	Atom action_atom;
	Atom atom;
	Window root;
	XClientMessageEvent ev;
	
	if (!display)
		display = gdk_display_get_default ();
	if (!screen)
		screen = gdk_display_get_default_screen (display);
	root = GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen));
	
	action_atom = gdk_x11_get_xatom_by_name_for_display (display, "_MATE_PANEL_ACTION");
	atom = gdk_x11_get_xatom_by_name_for_display (display, "_MATE_PANEL_ACTION_RUN_DIALOG");
	
	ev.type = ClientMessage;
	ev.window = root;
	ev.message_type = action_atom;
	ev.format = 32;
	ev.data.l[0] = atom;
	ev.data.l[1] = timestamp;
	
	gdk_error_trap_push ();
	
	XSendEvent (gdk_x11_display_get_xdisplay (display),
			root, False, StructureNotifyMask, (XEvent*) &ev);
	
	gdk_flush ();
	gdk_error_trap_pop ();
}

int
main (int argc, char **argv)
{
	gint lastentry = 0;
	guint32 timestamp;
	
	const gchar* startup_id = g_getenv ("DESKTOP_STARTUP_ID");
	//printf ("startup id is %s\n", startup_id);
	if (startup_id && (startup_id[0] != '\0'))
	{
		gchar **results = g_strsplit (startup_id, "_TIME", 0);
		while (results[lastentry] != NULL)
			lastentry++;
		timestamp = (guint32) g_strtod (results[lastentry - 1], NULL);
		g_strfreev (results);
	}
	else
		timestamp = GDK_CURRENT_TIME;
	
	gdk_init (&argc, &argv);
	run_dialog (NULL, NULL, timestamp);
	gdk_notify_startup_complete ();
	
	return 0;
}
