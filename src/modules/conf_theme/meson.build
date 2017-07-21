conf_theme_dist = [
	'module.desktop',
]

conf_theme_src = [
	'e_int_config_borders.c',
	'e_int_config_color_classes.c',
	'e_int_config_fonts.c',
	'e_int_config_scale.c',
	'e_int_config_theme.c',
	'e_int_config_theme_import.c',
	'e_int_config_transitions.c',
	'e_int_config_wallpaper.c',
	'e_int_config_xsettings.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_theme_dir = join_paths(dir_module_e, 'conf_theme', module_arch)
if get_option('conf-theme') == true
	config_h.set('USE_MODULE_CONF_THEME', '1')

	install_data(conf_theme_dist,
		install_dir: join_paths(dir_module_e, 'conf_theme')
	)

        module_files += join_paths(conf_theme_dir, 'conf_theme.so')
	shared_module('conf_theme',
                conf_theme_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_theme_dir,
                install: true
        )
endif

