conf_dist = [
	'e-module-conf.edj',
	'module.desktop',
]

conf_src = [
	'e_conf.c',
	'e_mod_config.c',
	'e_mod_main.c'
]

conf_dir = join_paths(dir_module_e, 'conf', module_arch)
if get_option('conf') == true
	config_h.set('USE_MODULE_CONF', '1')

	install_data(conf_dist,
		install_dir: join_paths(dir_module_e, 'conf')
	)

        module_files += join_paths(conf_dir, 'conf.so')
	shared_module('conf',
                conf_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_dir,
                install: true
        )
endif

