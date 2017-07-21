conf_paths_dist = [
	'e-module-conf_paths.edj',
	'module.desktop',
]

conf_paths_src = [
	'e_int_config_env.c',
	'e_int_config_paths.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_paths_dir = join_paths(dir_module_e, 'conf_paths', module_arch)
if get_option('conf-paths') == true
	config_h.set('USE_MODULE_CONF_PATHS', '1')

	install_data(conf_paths_dist,
		install_dir: join_paths(dir_module_e, 'conf_paths')
	)

        module_files += join_paths(conf_paths_dir, 'conf_paths.so')
	shared_module('conf_paths',
                conf_paths_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_paths_dir,
                install: true
        )
endif

