lokker_src = [
	'e_mod_main.c',
	'lokker.c',
	'e_mod_main.h',
]

lokker_dir = join_paths(dir_module_e, 'lokker', module_arch)
if get_option('lokker') == true
	config_h.set('USE_MODULE_LOKKER', '1')

        module_files += join_paths(lokker_dir, 'lokker.so')
	shared_module('lokker',
                lokker_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: lokker_dir,
                install: true
        )
endif

