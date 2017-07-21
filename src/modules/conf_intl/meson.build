conf_intl_dist = [
	'module.desktop',
]

conf_intl_src = [
	'e_int_config_imc.c',
	'e_int_config_imc_import.c',
	'e_int_config_intl.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_intl_dir = join_paths(dir_module_e, 'conf_intl', module_arch)
if get_option('conf-intl') == true
	config_h.set('USE_MODULE_CONF_INTL', '1')

	install_data(conf_intl_dist,
		install_dir: join_paths(dir_module_e, 'conf_intl')
	)

        module_files += join_paths(conf_intl_dir, 'conf_intl.so')
	shared_module('conf_intl',
                conf_intl_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_intl_dir,
                install: true
        )
endif

