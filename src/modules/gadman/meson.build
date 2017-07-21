gadman_dist = [
	'e-module-gadman.edj',
	'module.desktop',
]

gadman_src = [
	'e_mod_config.c',
	'e_mod_gadman.c',
	'e_mod_main.c',
	'e_mod_gadman.h',
]

gadman_dir = join_paths(dir_module_e, 'gadman', module_arch)
if get_option('gadman') == true
	config_h.set('USE_MODULE_GADMAN', '1')

	install_data(gadman_dist,
		install_dir: join_paths(dir_module_e, 'gadman')
	)

        module_files += join_paths(gadman_dir, 'gadman.so')
	shared_module('gadman',
                gadman_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: gadman_dir,
                install: true
        )
endif

