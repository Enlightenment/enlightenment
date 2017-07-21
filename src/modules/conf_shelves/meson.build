conf_shelves_dist = [
	'e-module-conf_shelves.edj',
	'module.desktop',
]

conf_shelves_src = [
	'e_int_config_shelf.c',
	'e_mod_main.c',
	'e_int_config_shelf.h',
	'e_mod_main.h',
]

conf_shelves_dir = join_paths(dir_module_e, 'conf_shelves', module_arch)
if get_option('conf-shelves') == true
	config_h.set('USE_MODULE_CONF_SHELVES', '1')

	install_data(conf_shelves_dist,
		install_dir: join_paths(dir_module_e, 'conf_shelves')
	)

        module_files += join_paths(conf_shelves_dir, 'conf_shelves.so')
	shared_module('conf_shelves',
                conf_shelves_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_shelves_dir,
                install: true
        )
endif

