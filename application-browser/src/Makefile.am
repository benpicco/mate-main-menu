INCLUDES =							\
	$(APPLICATION_BROWSER_CFLAGS)				\
	-I$(top_srcdir)/libtile					\
	-I$(top_srcdir)/libslab					\
	-I$(top_srcdir)/utils					\
	$(WARN_CFLAGS)

bin_PROGRAMS = application-browser

application_browser_SOURCES =					\
	app-layout.c						\
	app-layout.h						\
	application-browser.c					\
	app-window.c						\
	app-window.h

application_browser_LDADD =					\
	$(APPLICATION_BROWSER_LIBS)				\
	$(top_builddir)/libtile/libtile.la			\
	$(top_builddir)/libslab/libslab.la			\
	$(top_builddir)/utils/libutils.la

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"
