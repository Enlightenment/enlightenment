module = 'sysinfo'
opt    = 'sysinfo'
conf   = 'USE_MODULE_SYSINFO'

src = [
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
]

if config_h.has('HAVE_EEZE') == true
  src += [
           'batman/batman_udev.c',
           'thermal/thermal_udev.c'
         ]
elif host_machine.system().contains('bsd') == true
  src += ['batman/batman_sysctl.c',
          'thermal/thermal_sysctl.c',
          'cpuclock/cpuclock_sysctl.c',
          'netstatus/netstatus_sysctl.c',
          'cpumonitor/cpumonitor_sysctl.c',
          'memusage/memusage_sysctl.c'
         ]
else
  src += ['batman/batman_upower.c',
          'thermal/thermal_sysctl.c',
          'cpuclock/cpuclock_sysctl.c'
         ]
endif

icon = [
  'e-module-' + module + '.edj',
  'module.desktop'
]

dir_mod     = join_paths(dir_module_e, module)
dir_mod_bin = join_paths(dir_mod, module_arch)

if get_option(opt) == true
  config_h.set(conf, '1')
  module_files += join_paths(dir_mod_bin, module + '.so')

  install_data(icon, install_dir: dir_mod)
  shared_module(module, src,
                include_directories: include_directories(module_includes),
                name_prefix        : '',
                dependencies       : module_deps,
                install_dir        : dir_mod_bin,
                install            : true
               )
        executable('cpuclock_sysfs',
                   'cpuclock/cpuclock_sysfs.c',
                   dependencies: dep_crypt,
                   c_args      : suid_cflags,
                   link_args   : suid_ldflags,
                   install_dir : dir_mod_bin,
                   install     : true
                  )
        suid_exes += join_paths(dir_mod_bin, 'cpuclock_sysfs')
endif
