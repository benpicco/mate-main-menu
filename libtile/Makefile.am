INCLUDES =				\
	$(LIBTILE_CFLAGS)		\
	-I$(top_srcdir)/utils		\
	$(WARN_CFLAGS)

lib_LTLIBRARIES = libtile.la

libtile_la_SOURCES =			\
	application-tile.c		\
	document-tile.c			\
	nameplate-tile.c		\
	tile-action.c			\
	tile.c

libtile_includedir = $(includedir)/tile
libtile_include_HEADERS =		\
	application-tile.h		\
	document-tile.h			\
	nameplate-tile.h		\
	tile.h

libtile_la_LIBADD =			\
	$(LIBTILE_LIBS)			\
	$(top_builddir)/utils/libutils.la
