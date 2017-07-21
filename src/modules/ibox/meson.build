ibox_dist = [
	'e-module-ibox.edj',
	'module.desktop',
]

ibox_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

ibox_dir = join_paths(dir_module_e, 'ibox', module_arch)
if get_option('ibox') == true
	config_h.set('USE_MODULE_IBOX', '1')

	install_data(ibox_dist,
		install_dir: join_paths(dir_module_e, 'ibox')
	)

        module_files += join_paths(ibox_dir, 'ibox.so')
	shared_module('ibox',
                ibox_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: ibox_dir,
                install: true
        )
endif

