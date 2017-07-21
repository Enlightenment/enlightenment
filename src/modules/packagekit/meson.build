packagekit_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_packagekit.c',
	'e_mod_config.h',
	'e_mod_main.h',
	'e_mod_packagekit.h',
]

packagekit_dir = join_paths(dir_module_e, 'packagekit', module_arch)
if get_option('packagekit') == true
	config_h.set('USE_MODULE_PACKAGEKIT', '1')

        module_files += join_paths(packagekit_dir, 'packagekit.so')
	shared_module('packagekit',
                packagekit_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: packagekit_dir,
                install: true
        )
endif

