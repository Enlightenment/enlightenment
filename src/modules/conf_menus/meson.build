conf_menus_dist = [
	'e-module-conf_menus.edj',
	'module.desktop',
]

conf_menus_src = [
	'e_int_config_menus.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_menus_dir = join_paths(dir_module_e, 'conf_menus', module_arch)
if get_option('conf-menus') == true
	config_h.set('USE_MODULE_CONF_MENUS', '1')

	install_data(conf_menus_dist,
		install_dir: join_paths(dir_module_e, 'conf_menus')
	)

        module_files += join_paths(conf_menus_dir, 'conf_menus.so')
	shared_module('conf_menus',
                conf_menus_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_menus_dir,
                install: true
        )
endif

