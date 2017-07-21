conf_window_remembers_dist = [
	'e-module-conf_window_remembers.edj',
	'module.desktop',
]

conf_window_remembers_src = [
	'e_int_config_remembers.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_window_remembers_dir = join_paths(dir_module_e, 'conf_window_remembers', module_arch)
if get_option('conf-window-remembers') == true
	config_h.set('USE_MODULE_CONF_WINDOW_REMEMBERS', '1')

	install_data(conf_window_remembers_dist,
		install_dir: join_paths(dir_module_e, 'conf_window_remembers')
	)

        module_files += join_paths(conf_window_remembers_dir, 'conf_window_remembers.so')
	shared_module('conf_window_remembers',
                conf_window_remembers_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_window_remembers_dir,
                install: true
        )
endif

