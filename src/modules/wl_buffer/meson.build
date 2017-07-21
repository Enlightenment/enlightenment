wl_buffer_src = [
	'e_mod_main.c',
]

wl_buffer_dir = join_paths(dir_module_e, 'wl_buffer', module_arch)
if get_option('wl-buffer') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_WL_BUFFER', '1')

        module_files += join_paths(wl_buffer_dir, 'wl_buffer.so')
	shared_module('wl_buffer',
                wl_buffer_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wl_buffer_dir,
                install: true
        )
endif

