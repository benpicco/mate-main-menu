INCLUDES =				\
	@LIBUTILS_CFLAGS@		\
	$(WARN_CFLAGS)

noinst_LTLIBRARIES = libutils.la

libutils_includedir = $(includedir)/slab
libutils_include_HEADERS =			\
	gnome-utils.h				\
	slab-gnome-util.h			

libutils_la_SOURCES =			\
	double-click-detector.c		\
	double-click-detector.h		\
	eggaccelerators.c		\
	eggaccelerators.h		\
	egg-recent-item.c		\
	egg-recent-item.h		\
	egg-recent-model.c		\
	egg-recent-model.h		\
	gnome-utils.c			\
	gnome-utils.h			\
	recent-files.c			\
	recent-files.h			\
	slab-gnome-util.c		\
	slab-gnome-util.h		\
	themed-icon.c			\
	themed-icon.h

libutils_la_LIBADD =			\
	@LIBUTILS_LIBS@
