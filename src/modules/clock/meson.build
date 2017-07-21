clock_dist = [
	'e-module-clock.edj',
	'module.desktop',
]

clock_src = [
	'e_mod_config.c',
	'e_mod_main.c'
]

clock_dir = join_paths(dir_module_e, 'clock', module_arch)
if get_option('clock') == true
	config_h.set('USE_MODULE_CLOCK', '1')

	install_data(clock_dist,
		install_dir: join_paths(dir_module_e, 'clock')
	)

        module_files += join_paths(clock_dir, 'clock.so')
	shared_module('clock',
                clock_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: clock_dir,
                install: true
        )
endif

