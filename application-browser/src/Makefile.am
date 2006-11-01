INCLUDES =							\
	$(APPLICATION_BROWSER_CFLAGS)				\
	-I$(top_srcdir)/libslab					\
	$(WARN_CFLAGS)

bin_PROGRAMS = application-browser

application_browser_SOURCES =					\
	application-browser.c

application_browser_LDADD =					\
	$(APPLICATION_BROWSER_LIBS)				\
	$(top_builddir)/libslab/libslab.la

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"
