conf_window_manipulation_dist = [
	'e-module-conf_winmanip.edj',
	'module.desktop',
]

conf_window_manipulation_src = [
	'e_int_config_clientlist.c',
	'e_int_config_focus.c',
	'e_int_config_window_display.c',
	'e_int_config_window_geometry.c',
	'e_int_config_window_process.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_window_manipulation_dir = join_paths(dir_module_e, 'conf_window_manipulation', module_arch)
if get_option('conf-window-manipulation') == true
	config_h.set('USE_MODULE_CONF_WINDOW_MANIPULATION', '1')

	install_data(conf_window_manipulation_dist,
		install_dir: join_paths(dir_module_e, 'conf_window_manipulation')
	)

        module_files += join_paths(conf_window_manipulation_dir, 'conf_window_manipulation.so')
	shared_module('conf_window_manipulation',
                conf_window_manipulation_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_window_manipulation_dir,
                install: true
        )
endif

