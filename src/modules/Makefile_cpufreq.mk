EXTRA_DIST += src/modules/cpufreq/module.desktop.in \
src/modules/cpufreq/e-module-cpufreq.edj
if USE_MODULE_CPUFREQ
cpufreqdir = $(MDIR)/cpufreq
cpufreq_DATA = src/modules/cpufreq/e-module-cpufreq.edj \
	       src/modules/cpufreq/module.desktop


cpufreqpkgdir = $(MDIR)/cpufreq/$(MODULE_ARCH)
cpufreqpkg_LTLIBRARIES = src/modules/cpufreq/module.la

src_modules_cpufreq_module_la_LIBADD = $(MOD_LIBS)
src_modules_cpufreq_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_cpufreq_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_cpufreq_module_la_SOURCES = src/modules/cpufreq/e_mod_main.h \
			     src/modules/cpufreq/e_mod_main.c \
			     src/modules/cpufreq/e_mod_config.c

src_modules_cpufreq_freqsetdir = $(cpufreqpkgdir)
src_modules_cpufreq_freqset_PROGRAMS = src/modules/cpufreq/freqset

src_modules_cpufreq_freqset_SOURCES = src/modules/cpufreq/freqset.c
src_modules_cpufreq_freqset_CPPFLAGS  = $(MOD_CPPFLAGS) @e_cflags@ @SUID_CFLAGS@
src_modules_cpufreq_freqset_LDFLAGS = @SUID_LDFLAGS@

cpufreq_setuid_root_mode = a=rx,u+xs
cpufreq_setuid_root_user = root

cpufreq-install-data-hook:
	@chown $(cpufreq_setuid_root_user) $(DESTDIR)$(src_modules_cpufreq_freqsetdir)/freqset$(EXEEXT) || true
	@chmod $(cpufreq_setuid_root_mode) $(DESTDIR)$(src_modules_cpufreq_freqsetdir)/freqset$(EXEEXT) || true

INSTALL_DATA_HOOKS += cpufreq-install-data-hook

PHONIES += cpufreq install-cpufreq
cpufreq: $(cpufreqpkg_LTLIBRARIES) $(cpufreq_DATA) $(src_modules_cpufreq_freqset_PROGRAMS)
install-cpufreq: install-cpufreqDATA install-cpufreqpkgLTLIBRARIES install-src_modules_cpufreq_freqsetPROGRAMS
endif
