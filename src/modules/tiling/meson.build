tiling_src = [
	'e_mod_config.c',
	'e_mod_tiling.c',
	'window_tree.c',
	'e_mod_tiling.h',
	'window_tree.h',
]

tiling_dir = join_paths(dir_module_e, 'tiling', module_arch)
if get_option('tiling') == true
	config_h.set('USE_MODULE_TILING', '1')

        module_files += join_paths(tiling_dir, 'tiling.so')
	shared_module('tiling',
                tiling_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: tiling_dir,
                install: true
        )
endif

