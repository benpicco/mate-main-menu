serverdir   = $(libdir)/bonobo/servers
server_DATA = GNOME_MainMenu.server
@INTLTOOL_SERVER_RULE@

schemasdir   = @GCONF_SCHEMA_FILE_DIR@
schemas_DATA = slab.schemas
@INTLTOOL_SCHEMAS_RULE@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_MainMenu_ContextMenu.xml

desktop_helperdir = $(datadir)/applications
desktop_helper_DATA = gnome-screensaver-lock.desktop gnome-session-logout.desktop gnome-session-shutdown.desktop

desktopdir   = $(datadir)/applications
desktop_DATA = trigger-panel-run-dialog.desktop
@INTLTOOL_DESKTOP_RULE@

bookmarkdir = $(datadir)/gnome-main-menu
bookmark_DATA = applications.xbel documents.xbel places.xbel empty.ots empty.ott

systemitemsdir = $(datadir)/gnome-main-menu
systemitems_DATA = system-items.xbel

$(desktop_DATA).in: $(desktop_DATA).in.in
	sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@

$(server_DATA).in: $(server_DATA).in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

$(schemas_DATA).in: $(schemas_DATA).in.in
	sed -e "s|\@DATADIR\@|$(datadir)|g" $< > $@

$(systemitems_DATA): $(systemitems_DATA).in
	sed -e "s|_title>|title>|g" $< > $@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi

EXTRA_DIST = $(server_DATA).in.in $(schemas_DATA).in.in $(ui_DATA) $(desktop_helper_DATA) $(desktop_DATA).in.in $(bookmark_DATA) $(systemitems_DATA).in

CLEANFILES = $(server_DATA) $(schemas_DATA) $(server_DATA).in $(schemas_DATA).in $(systemitems_DATA) $(desktop_DATA) $(desktop_DATA).in
