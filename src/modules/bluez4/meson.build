bluez4_dist = [
	'e-module-bluez4.edj',
	'module.desktop',
]

bluez4_src = [
	'agent.c',
	'ebluez4.c',
	'e_mod_main.c'
]

bluez4_dir = join_paths(dir_module_e, 'bluez4', module_arch)
if get_option('bluez4') == true
	config_h.set('USE_MODULE_BLUEZ4', '1')

	install_data(bluez4_dist,
		install_dir: join_paths(dir_module_e, 'bluez4')
	)

        module_files += join_paths(bluez4_dir, 'bluez4.so')
	shared_module('bluez4',
                bluez4_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: bluez4_dir,
                install: true
        )
endif

