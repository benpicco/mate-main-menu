INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Main-Menu\"		\
	-DDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(NAUTILUS_CFLAGS)				\
	$(WARN_CFLAGS)

nautilus_extensiondir=@NAUTILUSDIR@

nautilus_extension_LTLIBRARIES=libnautilus-main-menu.la

libnautilus_main_menu_la_SOURCES = \
	nautilus-main-menu.c       \
	nautilus-main-menu.h       \
	main-menu.c

libnautilus_main_menu_la_LDFLAGS = -module -avoid-version
libnautilus_main_menu_la_LIBADD  = $(NAUTILUS_LIBS) $(GNOMEVFS_LIBS) $(GCONF_LIBS) $(GNOMEDESKTOP_LIBS)

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"
