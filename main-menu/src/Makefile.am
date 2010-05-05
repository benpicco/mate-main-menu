INCLUDES =								\
	$(MAIN_MENU_CFLAGS)						\
	$(WARN_CFLAGS)

uidir = $(datadir)/$(PACKAGE)
ui_DATA = slab-button.ui slab-window.ui

libexec_PROGRAMS = main-menu

bin_PROGRAMS = trigger-panel-run-dialog

AM_CPPFLAGS =								\
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
	$(MAIN_MENU_LIBS)

trigger_panel_run_dialog_SOURCES =					\
	trigger-panel-run-dialog.c

trigger_panel_run_dialog_LDADD =					\
	$(MAIN_MENU_LIBS)

EXTRA_DIST = $(ui_DATA)
