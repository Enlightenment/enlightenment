start_src = [
	'e_mod_main.c',
	'start.c',
]

start_dir = join_paths(dir_module_e, 'start', module_arch)
if get_option('start') == true
	config_h.set('USE_MODULE_START', '1')

        module_files += join_paths(start_dir, 'start.so')
	shared_module('start',
                start_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: start_dir,
                install: true
        )
endif

