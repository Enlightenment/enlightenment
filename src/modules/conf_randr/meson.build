conf_randr_dist = [
	'e-module-conf_randr.edj',
	'module.desktop',
]

conf_randr_src = [
	'e_int_config_randr2.c',
	'e_mod_main.c',
	'e_int_config_randr2.h',
	'e_mod_main.h',
]

conf_randr_dir = join_paths(dir_module_e, 'conf_randr', module_arch)
if get_option('conf-randr') == true
	config_h.set('USE_MODULE_CONF_RANDR', '1')

	install_data(conf_randr_dist,
		install_dir: join_paths(dir_module_e, 'conf_randr')
	)

        module_files += join_paths(conf_randr_dir, 'conf_randr.so')
	shared_module('conf_randr',
                conf_randr_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_randr_dir,
                install: true
        )
endif

