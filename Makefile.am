ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AUTOMAKE_OPTIONS = subdir-objects

SUBDIRS = .

if HAVE_PO

SUBDIRS += po

endif

CLEANFILES =
DISTCLEANFILES =
INSTALL_DATA_HOOKS =
PHONIES =

MAINTAINERCLEANFILES = \
ABOUT-NLS* \
Makefile.in \
aclocal.m4 \
compile \
config.guess \
config.h.in \
config.h.in~ \
config.rpath \
config.sub \
configure \
depcomp \
install-sh \
ltmain.sh \
missing \
$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz \
$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.bz2 \
$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-doc.tar.bz2

EXTRA_DIST = \
config.rpath \
README \
AUTHORS \
COPYING \
enlightenment.pc

include m4/Makefile.mk

if HAVE_PO

include po/Makefile.mk

endif

DISTCHECK_CONFIGURE_FLAGS = --disable-systemd

include config/Makefile.mk

include data/Makefile.mk

include src/Makefile.mk

filesdir = $(datadir)/enlightenment/
files_DATA = AUTHORS COPYING 

if HAVE_FREEBSD
pkgconfigdir = $(libdir)data/pkgconfig
else
pkgconfigdir = $(libdir)/pkgconfig
endif

pkgconfig_DATA = enlightenment.pc

include doc/Makefile.mk

include meson_dist.mk
EXTRA_DIST += $(meson_dist)

.PHONY: $(PHONIES)

clean-local: doc-clean
	rm -rf config/*.cfg config/*~
	rm -rf config/standard/*.cfg config/standard/*~
	rm -rf config/tiling/*.cfg config/tiling/*~
	rm -rf config/mobile/*.cfg config/mobile/*~
	rm -rf config/default/*.cfg config/default/*~
	rm -f data/themes/*.edj

install-data-hook: $(INSTALL_DATA_HOOKS)
