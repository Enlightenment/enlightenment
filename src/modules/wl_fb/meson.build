wl_fb_src = [
	'e_mod_main.c',
]

wl_fb_dir = join_paths(dir_module_e, 'wl_fb', module_arch)
if get_option('wl-fb') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_WL_FB', '1')

        module_files += join_paths(wl_fb_dir, 'wl_fb.so')
	shared_module('wl_fb',
                wl_fb_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wl_fb_dir,
                install: true
        )
endif

