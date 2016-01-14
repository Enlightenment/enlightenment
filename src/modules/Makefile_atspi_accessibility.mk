if USE_MODULE_ATSPI_ACCESSIBILITY
atspi_accessibilitydir = $(MDIR)/atspi_accessibility

atspi_accessibilitypkgdir = $(MDIR)/atspi_accessibility/$(MODULE_ARCH)
atspi_accessibilitypkg_LTLIBRARIES = src/modules/atspi_accessibility/module.la

src_modules_atspi_accessibility_module_la_LIBADD = $(MOD_LIBS)
src_modules_atspi_accessibility_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_atspi_accessibility_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_atspi_accessibility_module_la_SOURCES = src/modules/atspi_accessibility/e_mod_main.c \
   src/modules/atspi_accessibility/e_atspi_object.c \
   src/modules/atspi_accessibility/e_a11y_zone.c

PHONIES += atspi_accessibility install-atspi_accessibility
atspi_accessibility: $(atspi_accessibilitypkg_LTLIBRARIES) $(atspi_accessibility_DATA)
install-atspi_accessibility: install-atspi_accessibilityDATA install-atspi_accessibilitypkgLTLIBRARIES
endif
