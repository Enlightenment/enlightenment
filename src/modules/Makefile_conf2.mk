conf2dir = $(MDIR)/conf2
conf2_DATA = src/modules/conf2/e-module-conf2.edj \
	    src/modules/conf2/module.desktop
CLEANFILES += src/modules/conf2/e-module-conf2.edj
EXTRA_DIST += $(conf2_DATA) \
src/modules/conf2/e-module-conf2.edc \
src/modules/conf2/module.desktop.in \
src/modules/conf2/images/volume_knob_ledsoff.png \
src/modules/conf2/images/volume_knob_move.png \
src/modules/conf2/images/volume_knob.png \
src/modules/conf2/images/volume_led_01.png

conf2pkgdir = $(MDIR)/conf2/$(MODULE_ARCH)
conf2pkg_LTLIBRARIES = src/modules/conf2/module.la

src_modules_conf2_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_conf2_module_la_SOURCES = src/modules/conf2/e_mod_main.c \
			 src/modules/conf2/e_mod_main.h \
			 src/modules/conf2/e_conf2.c

src_modules_conf2_module_la_CPPFLAGS = $(MOD_CPPFLAGS) @ELM_CFLAGS@
src_modules_conf2_module_la_LIBADD = $(MOD_LIBS) @ELM_LIBS@

CONF2_EDJE_FLAGS = $(EDJE_FLAGS) -id $(top_srcdir)/src/modules/conf2/images -id $(top_srcdir)/data/themes/img

src/modules/conf2/%.edj: $(top_srcdir)/src/modules/conf2/%.edc Makefile
	$(EDJE_CC) $(CONF2_EDJE_FLAGS) $< $@

PHONIES += conf2 install-conf2
conf2: $(conf2pkg_LTLIBRARIES) $(conf2_DATA)
install-conf2: install-conf2DATA install-conf2pkgLTLIBRARIES

