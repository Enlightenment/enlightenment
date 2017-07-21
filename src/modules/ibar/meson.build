ibar_dist = [
	'e-module-ibar.edj',
	'module.desktop',
]

ibar_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

ibar_dir = join_paths(dir_module_e, 'ibar', module_arch)
if get_option('ibar') == true
	config_h.set('USE_MODULE_IBAR', '1')

	install_data(ibar_dist,
		install_dir: join_paths(dir_module_e, 'ibar')
	)

        module_files += join_paths(ibar_dir, 'ibar.so')
	shared_module('ibar',
                ibar_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: ibar_dir,
                install: true
        )
endif

