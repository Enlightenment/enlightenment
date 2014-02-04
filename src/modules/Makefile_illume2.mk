EXTRA_DIST += src/modules/illume2/module.desktop.in \
src/modules/illume2/e-module-illume2.edj
if USE_MODULE_ILLUME2
illume2dir = $(MDIR)/illume2
illume2_DATA = src/modules/illume2/e-module-illume2.edj \
	       src/modules/illume2/module.desktop

	      src/modules/illume2/module.desktop.in

# keyboards
illume2keyboardsdir = $(MDIR)/illume2/keyboards
illume2keyboards_DATA = src/modules/illume2/keyboards/ignore_built_in_keyboards

# policies
## illume
illume2policyillumedir = $(MDIR)/illume2/policies
illume2policyillume_LTLIBRARIES = src/modules/illume2/policies/illume/illume.la
ILLUME2POLICYCPPFLAGS = $(MOD_CPPFLAGS) -I$(top_srcdir)/src/modules/illume2

src_modules_illume2_policies_illume_illume_la_CPPFLAGS = $(ILLUME2POLICYCPPFLAGS)
src_modules_illume2_policies_illume_illume_la_LIBADD = $(MOD_LIBS)
src_modules_illume2_policies_illume_illume_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume2_policies_illume_illume_la_SOURCES = src/modules/illume2/policies/illume/policy.c \
					    src/modules/illume2/policies/illume/policy.h \
					    src/modules/illume2/policies/illume/illume.c \
					    src/modules/illume2/policies/illume/illume.h
src_modules_illume2_policies_illume_illume_la_LIBTOOLFLAGS = --tag=disable-static 

## tablet
src_modules_illume2policytabletdir = $(MDIR)/illume2/policies
src_modules_illume2policytablet_LTLIBRARIES = src/modules/illume2/policies/tablet/tablet.la

src_modules_illume2_policies_tablet_tablet_la_CPPFLAGS = $(ILLUME2POLICYCPPFLAGS)
src_modules_illume2_policies_tablet_tablet_la_LIBADD = $(MOD_LIBS)
src_modules_illume2_policies_tablet_tablet_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume2_policies_tablet_tablet_la_SOURCES = src/modules/illume2/policies/tablet/policy.c \
					    src/modules/illume2/policies/tablet/policy.h \
					    src/modules/illume2/policies/tablet/tablet.c \
					    src/modules/illume2/policies/tablet/tablet.h
src_modules_illume2_policies_tablet_tablet_la_LIBTOOLFLAGS = --tag=disable-static 

src_modules_illume2pkgdir = $(MDIR)/illume2/$(MODULE_ARCH)
src_modules_illume2pkg_LTLIBRARIES = src/modules/illume2/module.la

src_modules_illume2_module_la_LIBADD = $(MOD_LIBS)
src_modules_illume2_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_illume2_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_illume2_module_la_SOURCES = src/modules/illume2/e_mod_main.c \
			    src/modules/illume2/e_mod_main.h \
			    src/modules/illume2/e_illume.h \
			    src/modules/illume2/e_illume.c \
			    src/modules/illume2/e_illume_private.h \
			    src/modules/illume2/e_mod_config_policy.c \
			    src/modules/illume2/e_mod_select_window.c \
			    src/modules/illume2/e_mod_config_windows.c \
			    src/modules/illume2/e_mod_config_animation.c \
			    src/modules/illume2/e_mod_quickpanel.c \
			    src/modules/illume2/e_mod_kbd_device.c \
			    src/modules/illume2/e_mod_kbd.c \
			    src/modules/illume2/e_mod_policy.c \
			    src/modules/illume2/e_mod_config.c

# TODO: incomplete
PHONIES += illume2 install-illume2
illume2: $(illume2pkg_LTLIBRARIES) $(illume2_DATA)
install-illume2: install-illume2DATA install-illume2pkgLTLIBRARIES
endif
