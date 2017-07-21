conf_interaction_dist = [
	'e-module-conf_interaction.edj',
	'module.desktop',
]

conf_interaction_src = [
	'e_int_config_interaction.c',
	'e_int_config_mouse.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_interaction_dir = join_paths(dir_module_e, 'conf_interaction', module_arch)
if get_option('conf-interaction') == true
	config_h.set('USE_MODULE_CONF_INTERACTION', '1')

	install_data(conf_interaction_dist,
		install_dir: join_paths(dir_module_e, 'conf_interaction')
	)

        module_files += join_paths(conf_interaction_dir, 'conf_interaction.so')
	shared_module('conf_interaction',
                conf_interaction_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_interaction_dir,
                install: true
        )
endif

