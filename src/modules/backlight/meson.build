backlight_dist = [
	'e-module-backlight.edj',
	'module.desktop',
]

backlight_src = [
	'e_mod_main.c',
	'gadget/backlight.c',
	'gadget/mod.c'
]

backlight_dir = join_paths(dir_module_e, 'backlight', module_arch)
if get_option('backlight') == true
	config_h.set('USE_MODULE_BACKLIGHT', '1')

	install_data(backlight_dist,
		install_dir: join_paths(dir_module_e, 'backlight')
	)

        module_files += join_paths(backlight_dir, 'backlight.so')
	shared_module('backlight',
                backlight_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: backlight_dir,
                install: true
        )
endif

