conf_display_dist = [
	'module.desktop',
]

conf_display_src = [
	'e_int_config_desk.c',
	'e_int_config_desklock.c',
	'e_int_config_desklock_fsel.c',
	'e_int_config_desks.c',
	'e_int_config_dpms.c',
	'e_int_config_screensaver.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_display_dir = join_paths(dir_module_e, 'conf_display', module_arch)
if get_option('conf-display') == true
	config_h.set('USE_MODULE_CONF_DISPLAY', '1')

	install_data(conf_display_dist,
		install_dir: join_paths(dir_module_e, 'conf_display')
	)

        module_files += join_paths(conf_display_dir, 'conf_display.so')
	shared_module('conf_display',
                conf_display_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_display_dir,
                install: true
        )
endif

