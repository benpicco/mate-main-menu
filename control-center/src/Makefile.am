INCLUDES =							\
	$(CONTROL_CENTER_CFLAGS)				\
	-I$(top_srcdir)/libtile					\
	-I$(top_srcdir)/libslab					\
	-I$(top_srcdir)/utils					\
	$(WARN_CFLAGS)

bin_PROGRAMS = control-center 

control_center_SOURCES =					\
	control-center.c

control_center_LDADD =						\
	$(CONTROL_CENTER_LIBS)					\
	$(top_builddir)/libtile/libtile.la			\
	$(top_builddir)/libslab/libslab.la			\
	$(top_builddir)/utils/libutils.la

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"
