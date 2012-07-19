desktopdir       = $(datadir)/applications
desktop_DATA     = application-browser.desktop
@INTLTOOL_DESKTOP_RULE@

schemasdir   = @MATECONF_SCHEMA_FILE_DIR@
schemas_DATA = application-browser.schemas
@INTLTOOL_SCHEMAS_RULE@

$(desktop_DATA).in: $(desktop_DATA).in.in
	sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			MATECONF_CONFIG_SOURCE=$(MATECONF_SCHEMA_CONFIG_SOURCE) $(MATECONFTOOL) --makefile-install-rule $$p; \
		done \
	fi

EXTRA_DIST = $(desktop_DATA).in.in $(schemas_DATA).in

CLEANFILES = $(desktop_DATA) $(schemas_DATA) $(desktop_DATA).in
