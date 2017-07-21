xwayland_src = [
	'dnd.c',
	'e_mod_main.c',
]

xwayland_dir = join_paths(dir_module_e, 'xwayland', module_arch)
if get_option('xwayland') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_XWAYLAND', '1')

	xwayland = find_program('Xwayland')
        module_files += join_paths(xwayland_dir, 'xwayland.so')
	shared_module('xwayland',
                xwayland_src,
                include_directories: include_directories(module_includes),
		c_args: '-DXWAYLAND_BIN="@0@"'.format(xwayland.path()),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: xwayland_dir,
                install: true
        )
endif

