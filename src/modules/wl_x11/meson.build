wl_x11_src = [
	'e_mod_main.c',
]

wl_x11_dir = join_paths(dir_module_e, 'wl_x11', module_arch)
if get_option('wl-x11') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_WL_X11', '1')

        module_files += join_paths(wl_x11_dir, 'wl_x11.so')
	shared_module('wl_x11',
                wl_x11_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wl_x11_dir,
                install: true
        )
endif

