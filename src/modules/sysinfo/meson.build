sysinfo_src = [
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
	'netstatus/netstatus_proc.c',
]

if config_h.has('HAVE_EEZE') == true
	sysinfo_src += ['batman/batman_udev.c', 'thermal/thermal_udev.c']
elif host_machine.system().contains('bsd') == true
	sysinfo_src += ['batman/batman_sysctl.c',
		'thermal/thermal_sysctl.c',
		'cpuclock/cpuclock_sysctl.c',
		'netstatus/netstatus_sysctl.c',
		'cpumonitor/cpumonitor_sysctl.c',
		'memusage/memusage_sysctl.c'
	]
else
	sysinfo_src += ['batman/batman_upower.c',
		'thermal/thermal_sysctl.c',
		'cpuclock/cpuclock_sysctl.c'
	]
endif

module += [sysinfo_src, 'e-module-sysinfo.edj']

sysinfo_dir = join_paths(dir_module_e, 'sysinfo', module_arch)

executable('cpuclock_sysfs',
	'cpuclock/cpuclock_sysfs.c',
	include_directories: include_directories(module_includes),
	dependencies: dep_crypt,
	c_args: suid_cflags,
	link_args: suid_ldflags,
	install_dir: join_paths(dir_module_e, 'sysinfo'),
	install: true
)
suid_exes == join_paths(sysinfo_dir, 'cpuclock_sysfs')
