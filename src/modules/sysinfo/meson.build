src = files(
  'mod.c',
  'sysinfo.c',
  'sysinfo.h',
  'batman/batman.h',
  'batman/batman.c',
  'batman/batman_fallback.c',
  'batman/batman_config.c',
  'thermal/thermal.h',
  'thermal/thermal.c',
  'thermal/thermal_config.c',
  'thermal/thermal_fallback.c',
  'cpuclock/cpuclock.h',
  'cpuclock/cpuclock.c',
  'cpuclock/cpuclock_config.c',
  'cpumonitor/cpumonitor.h',
  'cpumonitor/cpumonitor.c',
  'cpumonitor/cpumonitor_config.c',
  'cpumonitor/cpumonitor_proc.c',
  'memusage/memusage.h',
  'memusage/memusage.c',
  'memusage/memusage_config.c',
  'memusage/memusage_proc.c',
  'netstatus/netstatus.h',
  'netstatus/netstatus.c',
  'netstatus/netstatus_config.c',
  'netstatus/netstatus_proc.c'
)
if config_h.has('HAVE_EEZE') == true
  src += files(
    'batman/batman_udev.c',
    'thermal/thermal_udev.c')
elif host_machine.system().contains('bsd') == true
  src += files(
    'batman/batman_sysctl.c',
    'thermal/thermal_sysctl.c',
    'cpuclock/cpuclock_sysctl.c',
    'netstatus/netstatus_sysctl.c',
    'cpumonitor/cpumonitor_sysctl.c',
    'memusage/memusage_sysctl.c')
else
  src += files(
    'batman/batman_upower.c',
    'thermal/thermal_sysctl.c',
    'cpuclock/cpuclock_sysctl.c')
endif

if get_option(m) == true
  executable('cpuclock_sysfs',
             'cpuclock/cpuclock_sysfs.c',
             dependencies: dep_crypt,
             c_args      : suid_cflags,
             link_args   : suid_ldflags,
             install_dir : _dir_bin,
             install     : true
            )
  suid_exes += join_paths(_dir_bin, 'cpuclock_sysfs')
endif
