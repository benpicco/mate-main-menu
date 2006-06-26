INCLUDES =					\
	$(LIBSLAB_CFLAGS)			\
	-I$(top_srcdir)/libtile			\
	-I$(top_srcdir)/utils			\
	$(WARN_CFLAGS)

lib_LTLIBRARIES = libslab.la

libslab_la_SOURCES =				\
	$(MARSHAL_GENERATED)			\
	app-resizer.c				\
	app-shell.c				\
	app-shell-startup.c			\
	search-bar.c				\
	search-context-picker.c			\
	search-entry-watermark.h		\
	search-entry.c				\
	shell-window.c				\
	slab-section.c

libslab_includedir = $(includedir)/slab
libslab_include_HEADERS =			\
	$(BUILT_SOURCES)			\
	app-resizer.h				\
	app-shell.h				\
	app-shell-startup.h			\
	search-bar.h				\
	search-context-picker.h			\
	search-entry.h				\
	shell-window.h				\
	slab-section.h

libslab_la_LIBADD =				\
	$(LIBSLAB_LIBS)				\
	$(LIBTILE_LIBS)				\
	$(top_builddir)/utils/libutils.la

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
