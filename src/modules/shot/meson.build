shot_src = [
	'e_mod_main.c',
]

shot_dir = join_paths(dir_module_e, 'shot', module_arch)
if get_option('shot') == true
	config_h.set('USE_MODULE_SHOT', '1')

        module_files += join_paths(shot_dir, 'shot.so')
	shared_module('shot',
                shot_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: shot_dir,
                install: true
        )
endif

