wl_drm_src = [
	'e_mod_main.c',
]

wl_drm_dir = join_paths(dir_module_e, 'wl_drm', module_arch)
if get_option('wl-drm') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_WL_DRM', '1')

        module_files += join_paths(wl_drm_dir, 'wl_drm.so')
	shared_module('wl_drm',
                wl_drm_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wl_drm_dir,
                install: true
        )
endif

