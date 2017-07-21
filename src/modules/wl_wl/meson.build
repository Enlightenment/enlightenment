wl_wl_src = [
	'e_mod_main.c',
]

wl_wl_dir = join_paths(dir_module_e, 'wl_wl', module_arch)
if get_option('wl-wl') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_WL_WL', '1')

        module_files += join_paths(wl_wl_dir, 'wl_wl.so')
	shared_module('wl_wl',
                wl_wl_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wl_wl_dir,
                install: true
        )
endif

