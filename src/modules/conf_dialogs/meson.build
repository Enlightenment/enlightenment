conf_dialogs_dist = [
	'e-module-conf_dialogs.edj',
	'module.desktop',
]

conf_dialogs_src = [
	'e_int_config_dialogs.c',
	'e_int_config_profiles.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_dialogs_dir = join_paths(dir_module_e, 'conf_dialogs', module_arch)
if get_option('conf-dialogs') == true
	config_h.set('USE_MODULE_CONF_DIALOGS', '1')

	install_data(conf_dialogs_dist,
		install_dir: join_paths(dir_module_e, 'conf_dialogs')
	)

        module_files += join_paths(conf_dialogs_dir, 'conf_dialogs.so')
	shared_module('conf_dialogs',
                conf_dialogs_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_dialogs_dir,
                install: true
        )
endif

