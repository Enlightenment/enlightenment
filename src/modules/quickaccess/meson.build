quickaccess_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_quickaccess.c',
	'e_quickaccess_bindings.c',
	'e_quickaccess_db.c',
	'e_mod_main.h',
]

quickaccess_dir = join_paths(dir_module_e, 'quickaccess', module_arch)
if get_option('quickaccess') == true
	config_h.set('USE_MODULE_QUICKACCESS', '1')

        module_files += join_paths(quickaccess_dir, 'quickaccess.so')
	shared_module('quickaccess',
                quickaccess_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: quickaccess_dir,
                install: true
        )
endif

