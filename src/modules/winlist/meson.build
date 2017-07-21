winlist_src = [
	'e_int_config_winlist.c',
	'e_mod_main.c',
	'e_winlist.c',
	'e_mod_main.h',
	'e_winlist.h',
]

winlist_dir = join_paths(dir_module_e, 'winlist', module_arch)
if get_option('winlist') == true
	config_h.set('USE_MODULE_WINLIST', '1')

        module_files += join_paths(winlist_dir, 'winlist.so')
	shared_module('winlist',
                winlist_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: winlist_dir,
                install: true
        )
endif

