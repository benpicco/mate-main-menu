if ENABLE_NAUTILUS_EXTENSION
NAUTILUS_DIR=nautilus-main-menu
else
NAUTILUS_DIR=
endif

SUBDIRS =				\
	libslab				\
	application-browser/etc		\
	application-browser/src		\
	main-menu/etc			\
	main-menu/src			\
	$(NAUTILUS_DIR)			\
	patch				\
	po

EXTRA_DIST =				\
	intltool-extract.in		\
	intltool-merge.in		\
	intltool-update.in

DISTCLEANFILES =			\
	intltool-extract		\
	intltool-merge			\
	intltool-update
