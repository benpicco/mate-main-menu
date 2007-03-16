INCLUDES =								\
	$(MAIN_MENU_CFLAGS)						\
	-I$(top_srcdir)/libslab						\
	$(WARN_CFLAGS)

gladedir = $(datadir)/$(PACKAGE)
glade_DATA = slab-window.glade

libexec_PROGRAMS = main-menu

AM_CPPFLAGS =								\
	$(GLADE_CFLAGS)							\
	-DDATADIR=\""$(datadir)"\"					\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

main_menu_SOURCES =							\
	main-menu.c							\
	main-menu-ui.c			main-menu-ui.h			\
	main-menu-migration.c		main-menu-migration.h		\
	tile-table.c			tile-table.h			\
	hard-drive-status-tile.c	hard-drive-status-tile.h	\
	network-status-tile.c		network-status-tile.h		\
	network-status-agent.c		network-status-agent.h		\
	network-status-info.c		network-status-info.h		\
	tomboykeybinder.c		tomboykeybinder.h		\
	eggaccelerators.c		eggaccelerators.h

main_menu_LDADD =							\
	$(MAIN_MENU_LIBS)						\
	$(GLADE_LIBS)							\
	$(top_builddir)/libslab/libslab.la

EXTRA_DIST = $(glade_DATA)
