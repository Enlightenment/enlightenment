EXTRA_DIST += src/modules/sysinfo/module.desktop \
src/modules/sysinfo/e-module-sysinfo.edj
if USE_MODULE_SYSINFO
sysinfodir = $(MDIR)/sysinfo
sysinfo_DATA = src/modules/sysinfo/e-module-sysinfo.edj \
	    src/modules/sysinfo/module.desktop


sysinfopkgdir = $(MDIR)/sysinfo/$(MODULE_ARCH)
sysinfopkg_LTLIBRARIES = src/modules/sysinfo/module.la

src_modules_sysinfo_module_la_LIBADD = $(MOD_LIBS)
src_modules_sysinfo_module_la_CPPFLAGS = $(MOD_CPPFLAGS) -Wall -Wextra -Wshadow
src_modules_sysinfo_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_sysinfo_module_la_SOURCES = src/modules/sysinfo/mod.c \
			 src/modules/sysinfo/sysinfo.h \
			 src/modules/sysinfo/batman/batman.h \
                         src/modules/sysinfo/batman/batman.c \
                         src/modules/sysinfo/batman/batman_fallback.c \
                         src/modules/sysinfo/batman/batman_config.c \
                         src/modules/sysinfo/thermal/thermal.h \
                         src/modules/sysinfo/thermal/thermal.c \
                         src/modules/sysinfo/thermal/thermal_config.c \
                         src/modules/sysinfo/thermal/thermal_fallback.c \
                         src/modules/sysinfo/cpuclock/cpuclock.h \
                         src/modules/sysinfo/cpuclock/cpuclock.c \
                         src/modules/sysinfo/cpuclock/cpuclock_config.c \
                         src/modules/sysinfo/cpumonitor/cpumonitor.h \
                         src/modules/sysinfo/cpumonitor/cpumonitor.c \
                         src/modules/sysinfo/cpumonitor/cpumonitor_config.c \
                         src/modules/sysinfo/cpumonitor/cpumonitor_proc.c \
                         src/modules/sysinfo/memusage/memusage.h \
                         src/modules/sysinfo/memusage/memusage.c \
                         src/modules/sysinfo/memusage/memusage_config.c \
                         src/modules/sysinfo/memusage/memusage_proc.c \
                         src/modules/sysinfo/netstatus/netstatus.h \
                         src/modules/sysinfo/netstatus/netstatus.c \
                         src/modules/sysinfo/netstatus/netstatus_config.c \
                         src/modules/sysinfo/netstatus/netstatus_proc.c \
                         src/modules/sysinfo/sysinfo.c
if HAVE_EEZE
src_modules_sysinfo_module_la_SOURCES += src/modules/sysinfo/batman/batman_udev.c \
                                         src/modules/sysinfo/thermal/thermal_udev.c
else
if HAVE_OPENBSD
src_modules_sysinfo_module_la_SOURCES += src/modules/sysinfo/batman/batman_sysctl.c \
                                         src/modules/sysinfo/thermal/thermal_sysctl.c \
                                         src/modules/sysinfo/cpuclock/cpuclock_sysctl.c \
                                         src/modules/sysinfo/netstatus/netstatus_sysctl.c \
                                         src/modules/sysinfo/cpumonitor/cpumonitor_sysctl.c \
                                         src/modules/sysinfo/memusage/memusage_sysctl.c
else
if HAVE_NETBSD
src_modules_sysinfo_module_la_SOURCES += src/modules/sysinfo/batman/batman_sysctl.c \
                                         src/modules/sysinfo/thermal/thermal_sysctl.c \
                                         src/modules/sysinfo/cpuclock/cpuclock_sysctl.c \
                                         src/modules/sysinfo/netstatus/netstatus_sysctl.c \
                                         src/modules/sysinfo/cpumonitor/cpumonitor_sysctl.c \
                                         src/modules/sysinfo/memusage/memusage_sysctl.c
else
if HAVE_DRAGONFLY
src_modules_sysinfo_module_la_SOURCES += src/modules/sysinfo/batman/batman_sysctl.c \
                                         src/modules/sysinfo/thermal/thermal_sysctl.c \
                                         src/modules/sysinfo/cpuclock/cpuclock_sysctl.c \
                                         src/modules/sysinfo/netstatus/netstatus_sysctl.c \
                                         src/modules/sysinfo/cpumonitor/cpumonitor_sysctl.c \
                                         src/modules/sysinfo/memusage/memusage_sysctl.c
else
if HAVE_FREEBSD
src_modules_sysinfo_module_la_SOURCES += src/modules/sysinfo/batman/batman_sysctl.c \
                                         src/modules/sysinfo/thermal/thermal_sysctl.c \
                                         src/modules/sysinfo/cpuclock/cpuclock_sysctl.c \
                                         src/modules/sysinfo/netstatus/netstatus_sysctl.c \
                                         src/modules/sysinfo/cpumonitor/cpumonitor_sysctl.c \
                                         src/modules/sysinfo/memusage/memusage_sysctl.c
else
src_modules_sysinfo_module_la_SOURCES += src/modules/sysinfo/batman/batman_upower.c \
                                         src/modules/sysinfo/thermal/thermal_sysctl.c \
                                         src/modules/sysinfo/cpuclock/cpuclock_sysctl.c
endif
endif
endif
endif
endif

src_modules_sysinfo_sysfsfreqdir = $(sysinfopkgdir)
src_modules_sysinfo_sysfsfreq_PROGRAMS = src/modules/sysinfo/cpuclock/cpuclock_sysfs

src_modules_sysinfo_cpuclock_cpuclock_sysfs_SOURCES = src/modules/sysinfo/cpuclock/cpuclock_sysfs.c
src_modules_sysinfo_cpuclock_cpuclock_sysfs_CPPFLAGS  = $(MOD_CPPFLAGS) @e_cflags@ @SUID_CFLAGS@
src_modules_sysinfo_cpuclock_cpuclock_sysfsLDFLAGS = @SUID_LDFLAGS@

sysfsfreq_setuid_root_mode = a=rx,u+xs
sysfsfreq_setuid_root_user = root

sysfsfreq-install-data-hook:
	@chown $(sysfsfreq_setuid_root_user) $(DESTDIR)$(src_modules_sysinfo_sysfsfreqdir)/cpuclock_sysfs$(EXEEXT) || true
	@chmod $(sysfsfreq_setuid_root_mode) $(DESTDIR)$(src_modules_sysinfo_sysfsfreqdir)/cpuclock_sysfs$(EXEEXT) || true

INSTALL_DATA_HOOKS += sysfsfreq-install-data-hook

PHONIES += sysinfo install-sysinfo
sysinfo: $(sysinfopkg_LTLIBRARIES) $(sysinfo_DATA) $(src_modules_sysinfo_sysfsfreq_PROGRAMS)
install-sysinfo: install-sysinfoDATA install-sysinfopkgLTLIBRARIES install-src_modules_sysinfo_cpuclock_cpuclock_sysfsPROGRAMS
endif
