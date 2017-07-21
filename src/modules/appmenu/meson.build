appmenu_dist = [
	'e-module-appmenu.edj',
	'module.desktop',
]

appmenu_src = [
	'e_mod_main.c',
]

appmenu_dir = join_paths(dir_module_e, 'appmenu', module_arch)
if get_option('appmenu') == true
	config_h.set('USE_MODULE_APPMENU', '1')

	install_data(appmenu_dist,
		install_dir: join_paths(dir_module_e, 'appmenu')
	)

        module_files += join_paths(appmenu_dir, 'appmenu.so')
	shared_module('appmenu',
                appmenu_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: appmenu_dir,
                install: true
        )
endif

