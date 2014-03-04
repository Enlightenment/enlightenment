EET_EET = @eet_eet@

EXTRA_DIST += config/profile.src

configfilesdir = $(datadir)/enlightenment/data/config

configfiles_DATA = config/profile.cfg

SUFFIXES = .cfg

.src.cfg:
	$(MKDIR_P) $(@D)
	$(EET_EET) -e \
	$(top_builddir)/$@ config \
	$< 1

include config/default/Makefile.mk
include config/tiling/Makefile.mk
include config/standard/Makefile.mk
include config/mobile/Makefile.mk

config/profile.cfg: config/profile.src
	$(MKDIR_P) $(@D)
	$(EET_EET) -i \
	$(top_builddir)/$@ config \
	$< 1
