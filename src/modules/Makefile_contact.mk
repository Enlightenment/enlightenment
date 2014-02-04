EXTRA_DIST += src/modules/contact/module.desktop.in \
src/modules/contact/e-module-contact.edj
if USE_MODULE_CONTACT
contactdir = $(MDIR)/contact
contact_DATA = src/modules/contact/e-module-contact.edj \
	       src/modules/contact/module.desktop

contactpkgdir = $(MDIR)/contact/$(MODULE_ARCH)
contactpkg_LTLIBRARIES = src/modules/contact/module.la

src_modules_contact_module_la_LIBADD = $(MOD_LIBS)
src_modules_contact_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_contact_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_contact_module_la_SOURCES = src/modules/contact/e_mod_main.c \
			    src/modules/contact/e_mod_main.h \
                            src/modules/contact/e_policy.c \
                            src/modules/contact/e_policy.h \
                            src/modules/contact/e_edges.c \
                            src/modules/contact/e_edges.h

# TODO: incomplete
PHONIES += contact install-contact
contact: $(contactpkg_LTLIBRARIES) $(contact_DATA)
install-contact: install-contactDATA install-contactpkgLTLIBRARIES
endif
