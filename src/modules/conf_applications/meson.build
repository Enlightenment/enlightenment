conf_applications_dist = [
	'e-module-conf_applications.edj',
	'module.desktop',
]

conf_applications_src = [
	'e_int_config_apps.c',
	'e_int_config_apps_personal.c',
	'e_int_config_defapps.c',
	'e_int_config_deskenv.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_applications_dir = join_paths(dir_module_e, 'conf_applications', module_arch)
if get_option('conf-applications') == true
	config_h.set('USE_MODULE_CONF_APPLICATIONS', '1')

	install_data(conf_applications_dist,
		install_dir: join_paths(dir_module_e, 'conf_applications')
	)

        module_files += join_paths(conf_applications_dir, 'conf_applications.so')
	shared_module('conf_applications',
                conf_applications_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_applications_dir,
                install: true
        )
endif

