serverdir   = $(libdir)/bonobo/servers
server_DATA = GNOME_MainMenu.server
@INTLTOOL_SERVER_RULE@

schemasdir   = @GCONF_SCHEMA_FILE_DIR@
schemas_DATA = slab.schemas
@INTLTOOL_SCHEMAS_RULE@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_MainMenu_ContextMenu.xml

desktopdir   = $(datadir)/applications
desktop_DATA = gnome-screensaver-lock.desktop gnome-session-kill.desktop
@INTLTOOL_DESKTOP_RULE@

bookmarkdir = $(datadir)/gnome-main-menu
bookmark_DATA = applications.xbel documents.xbel system-items.xbel places.xbel

$(server_DATA).in: $(server_DATA).in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

$(schemas_DATA).in: $(schemas_DATA).in.in
	sed -e "s|\@DATADIR\@|$(datadir)|g" $< > $@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi

EXTRA_DIST = $(server_DATA).in.in $(schemas_DATA).in.in $(ui_DATA) $(desktop_DATA) $(bookmark_DATA)

CLEANFILES = $(server_DATA) $(schemas_DATA) $(server_DATA).in $(schemas_DATA).in
