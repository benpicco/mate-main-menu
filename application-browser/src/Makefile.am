INCLUDES =							\
	$(APPLICATION_BROWSER_CFLAGS)				\
	$(WARN_CFLAGS)

bin_PROGRAMS = application-browser

application_browser_SOURCES =					\
	application-browser.c

application_browser_LDADD =					\
	$(APPLICATION_BROWSER_LIBS)

AM_CPPFLAGS =							\
	-DMATELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"
