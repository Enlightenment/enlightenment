conf_performance_dist = [
	'e-module-conf_performance.edj',
	'module.desktop',
]

conf_performance_src = [
	'e_int_config_performance.c',
	'e_int_config_powermanagement.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_performance_dir = join_paths(dir_module_e, 'conf_performance', module_arch)
if get_option('conf-performance') == true
	config_h.set('USE_MODULE_CONF_PERFORMANCE', '1')

	install_data(conf_performance_dist,
		install_dir: join_paths(dir_module_e, 'conf_performance')
	)

        module_files += join_paths(conf_performance_dir, 'conf_performance.so')
	shared_module('conf_performance',
                conf_performance_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_performance_dir,
                install: true
        )
endif

