INCLUDES =							\
	$(CONTROL_CENTER_CFLAGS)				\
	-I$(top_srcdir)/libslab					\
	$(WARN_CFLAGS)

bin_PROGRAMS = control-center 

control_center_SOURCES =					\
	control-center.c

control_center_LDADD =						\
	$(CONTROL_CENTER_LIBS)					\
	$(top_builddir)/libslab/libslab.la

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"
