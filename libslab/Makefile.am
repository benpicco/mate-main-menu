INCLUDES =					\
	$(LIBSLAB_CFLAGS)			\
	$(WARN_CFLAGS)

lib_LTLIBRARIES = libslab.la

libslab_la_SOURCES =				\
	$(MARSHAL_GENERATED)			\
	app-resizer.c				\
	app-shell.c				\
	app-shell-startup.c			\
	application-tile.c			\
	bookmark-agent.h			\
	bookmark-agent-libslab.h		\
	bookmark-agent.c			\
	directory-tile.c			\
	document-tile.c				\
	double-click-detector.c			\
	double-click-detector.h			\
	libslab-bookmarkfile.c			\
	libslab-bookmarkfile.h			\
	egg-recent-item.c			\
	egg-recent-item.h			\
	egg-recent-model.c			\
	egg-recent-model.h			\
	gnome-utils.c				\
	libslab-utils.c				\
	libslab-utils.h				\
	nameplate-tile.c			\
	search-bar.c				\
	search-context-picker.c			\
	search-entry-watermark.h		\
	search-entry.c				\
	shell-window.c				\
	slab-gnome-util.c			\
	slab-section.c				\
	system-tile.c				\
	themed-icon.c				\
	themed-icon.h				\
	tile-action.c				\
	tile.c

libslab_includedir = $(includedir)/slab
libslab_include_HEADERS =			\
	$(BUILT_SOURCES)			\
	app-resizer.h				\
	app-shell.h				\
	app-shell-startup.h			\
	application-tile.h			\
	directory-tile.h			\
	document-tile.h				\
	gnome-utils.h				\
	nameplate-tile.h			\
	search-bar.h				\
	search-context-picker.h			\
	search-entry.h				\
	shell-window.h				\
	slab-gnome-util.h			\
	slab-section.h				\
	system-tile.h				\
	tile.h

libslab_la_LIBADD =				\
	$(LIBSLAB_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libslab.pc


search-entry-watermark.h: search-entry-watermark.svg
	echo '#define SEARCH_ENTRY_WATERMARK_SVG "\' > $@; \
	sed -e 's/"/\\"/g' -e 's/$$/\\/' -e 's/#000000/#%s/g' $< >> $@; \
	echo '"' >> $@


MARSHAL_GENERATED = nld-marshal.c nld-marshal.h

nld-marshal.h: nld-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=nld_marshal $(srcdir)/nld-marshal.list --header > nld-marshal.tmp \
	&& mv nld-marshal.tmp nld-marshal.h ) \
	|| ( rm -f nld-marshal.tmp && exit 1 )

nld-marshal.c: nld-marshal.h
	( (echo '#include "nld-marshal.h"'; @GLIB_GENMARSHAL@ --prefix=nld_marshal $(srcdir)/nld-marshal.list --body) > nld-marshal.tmp \
	&& mv nld-marshal.tmp nld-marshal.c ) \
	|| ( rm -f nld-marshal.tmp && exit 1 )


BUILT_SOURCES =				\
	search-entry-watermark.h	\
	$(MARSHAL_GENERATED)

CLEANFILES =				\
	search-entry-watermark.h	\
	$(MARSHAL_GENERATED)

EXTRA_DIST=				\
	search-entry-watermark.svg	\
	nld-marshal.list
