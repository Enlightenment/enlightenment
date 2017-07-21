connman_dist = [
	'e-module-connman.edj',
	'module.desktop',
]

connman_src = [
	'agent.c',
	'e_connman.c',
	'e_mod_config.c',
	'e_mod_main.c',
	'E_Connman.h',
	'e_mod_main.h',
]

connman_dir = join_paths(dir_module_e, 'connman', module_arch)
if get_option('connman') == true
	config_h.set('USE_MODULE_CONNMAN', '1')

	install_data(connman_dist,
		install_dir: join_paths(dir_module_e, 'connman')
	)

        module_files += join_paths(connman_dir, 'connman.so')
	shared_module('connman',
                connman_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: connman_dir,
                install: true
        )
endif

