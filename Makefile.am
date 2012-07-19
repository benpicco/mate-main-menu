if ENABLE_CAJA_EXTENSION
CAJA_DIR=caja-main-menu
else
CAJA_DIR=
endif

SUBDIRS =				\
	application-browser/etc		\
	application-browser/src		\
	main-menu/etc			\
	main-menu/src			\
	$(CAJA_DIR)			\
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
