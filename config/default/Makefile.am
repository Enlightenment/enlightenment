MAINTAINERCLEANFILES = Makefile.in

EET_EET = @eet_eet@

EXTRA_DIST = \
        profile.desktop \
        enlightenment-default.png \
	e.src \
	e_bindings.src

filesdir = $(datadir)/enlightenment/data/config/default
files_DATA = \
        profile.desktop \
        enlightenment-default.png \
	e.cfg \
	e_bindings.cfg

SUFFIXES = .cfg

.src.cfg:
	$(EET_EET) -e \
	$(top_builddir)/config/default/$@ config \
	$< 1

clean-local:
	rm -rf *.cfg *~
	
