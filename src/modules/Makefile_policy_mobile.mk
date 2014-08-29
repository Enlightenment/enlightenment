EXTRA_DIST += src/modules/policy_mobile/e-module-policy-mobile.edj
if USE_MODULE_POLICY_MOBILE
policy_mobiledir = $(MDIR)/policy_mobile
policy_mobile_DATA = src/modules/policy_mobile/e-module-policy-mobile.edj

policy_mobilepkgdir = $(MDIR)/policy_mobile/$(MODULE_ARCH)
policy_mobilepkg_LTLIBRARIES = src/modules/policy_mobile/module.la

src_modules_policy_mobile_module_la_LIBADD = $(MOD_LIBS)
src_modules_policy_mobile_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_policy_mobile_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_policy_mobile_module_la_SOURCES = \
src/modules/policy_mobile/e_mod_main.h \
src/modules/policy_mobile/e_mod_config.c \
src/modules/policy_mobile/e_mod_softkey.c \
src/modules/policy_mobile/e_mod_main.c

PHONIES += policy_mobile install-policy_mobile
policy_mobile: $(policy_mobilepkg_LTLIBRARIES) $(policy_mobile_DATA)
install-policy_mobile: install-policy_mobileDATA install-policy_mobilepkgLTLIBRARIES
endif
