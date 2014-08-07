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
enlightenment.spec.in \
enlightenment.spec \
enlightenment.pc \
pkgbuild/PKGBUILD \
pkgbuild/enlightenment.install

EDJE_CC = $(EDJE_CC_$(V))
EDJE_CC_ = $(EDJE_CC_$(AM_DEFAULT_VERBOSITY))
EDJE_CC_0 = @echo "  EDJE_CC " $@; @edje_cc@
EDJE_CC_1 = @edje_cc@

EDJE_FLAGS_VERBOSE_ = $(EDJE_FLAGS_VERBOSE_$(AM_DEFAULT_VERBOSITY))
EDJE_FLAGS_VERBOSE_0 = 
EDJE_FLAGS_VERBOSE_1 = -v
EDJE_FLAGS = $(EDJE_FLAGS_VERBOSE_$(V)) \
@EDJE_DEF@

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

.PHONY: $(PHONIES)

clean-local: doc-clean
	rm -rf config/*.cfg config/*~
	rm -rf config/standard/*.cfg config/standard/*~
	rm -rf config/tiling/*.cfg config/tiling/*~
	rm -rf config/mobile/*.cfg config/mobile/*~
	rm -rf config/default/*.cfg config/default/*~
	rm -f data/themes/*.edj

install-data-hook: $(INSTALL_DATA_HOOKS)
