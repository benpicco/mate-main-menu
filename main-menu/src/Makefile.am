INCLUDES =								\
	$(MAIN_MENU_CFLAGS)						\
	-I$(top_srcdir)/utils						\
	-I$(top_srcdir)/libtile						\
	-I$(top_srcdir)/libslab						\
	$(WARN_CFLAGS)

libexec_PROGRAMS = main-menu

AM_CPPFLAGS =								\
	$(GLADE_CFLAGS)							\
	-DDATADIR=\""$(datadir)"\"					\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

main_menu_SOURCES =							\
	file-area-widget.c		file-area-widget.h		\
	hard-drive-status-tile.c	hard-drive-status-tile.h	\
	main-menu.c							\
	main-menu-common.h						\
	main-menu-engine.c						\
	main-menu-ui.c							\
	main-menu-conf.c		main-menu-conf.h		\
	network-status-tile.c		network-status-tile.h		\
	network-status-agent.c		network-status-agent.h		\
	network-status-info.c		network-status-info.h		\
	recent-files.c			recent-files.h			\
	slab-window.c			slab-window.h			\
	system-tile.c			system-tile.h			\
	text-button.c			text-button.h			\
	tile-table.c			tile-table.h			\
	tomboykeybinder.c		tomboykeybinder.h

main_menu_LDADD =							\
	$(MAIN_MENU_LIBS)						\
	$(GLADE_LIBS)							\
	$(top_builddir)/utils/libutils.la				\
	$(top_builddir)/libtile/libtile.la				\
	$(top_builddir)/libslab/libslab.la
