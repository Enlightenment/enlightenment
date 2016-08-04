EXTRA_DIST += src/modules/imfos/module.desktop.in
if USE_MODULE_IMFOS
imfosdir = $(MDIR)/imfos
imfos_DATA = src/modules/imfos/module.desktop


imfospkgdir = $(MDIR)/imfos/$(MODULE_ARCH)
imfospkg_LTLIBRARIES = src/modules/imfos/module.la

src_modules_imfos_module_la_LIBADD = $(MOD_LIBS) @IMFOS_LIBS@
src_modules_imfos_module_la_CPPFLAGS = $(MOD_CPPFLAGS) @IMFOS_CFLAGS@
src_modules_imfos_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_imfos_module_la_SOURCES = src/modules/imfos/e_mod_main.h \
			  src/modules/imfos/e_mod_main.c \
			  src/modules/imfos/e_mod_config.c \
		   src/modules/imfos/imfos_v4l.c \
		   src/modules/imfos/imfos_v4l.h \
		   src/modules/imfos/imfos_face.cpp \
		   src/modules/imfos/imfos_face.h

PHONIES += imfos install-imfos
imfos: $(imfospkg_LTLIBRARIES) $(imfos_DATA)
install-imfos: install-imfosDATA install-imfospkgLTLIBRARIES
endif
