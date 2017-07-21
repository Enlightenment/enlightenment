battery_dist = [
	'e-module-battery.edj',
	'module.desktop',
]

battery_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

if config_h.has('HAVE_EEZE') == true
	battery_src += 'e_mod_udev.c'
elif host_machine.system().contains('bsd') == true
	battery_src += 'e_mod_sysctl.c'
else
	battery_src += 'e_mod_upower.c'
endif

battery_dir = join_paths(dir_module_e, 'battery', module_arch)
if get_option('battery') == true
	config_h.set('USE_MODULE_BATTERY', '1')

	install_data(battery_dist,
		install_dir: join_paths(dir_module_e, 'battery')
	)

        module_files += join_paths(battery_dir, 'battery.so')
	shared_module('battery',
                battery_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: battery_dir,
                install: true
        )

	executable('batget',
		'batget.c',
		include_directories: include_directories(module_includes),
		dependencies: [dep_eina, dep_ecore, dep_ecore_con, dep_ecore_file],
		install_dir: battery_dir,
		install: true
	)
endif
