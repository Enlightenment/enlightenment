wl_desktop_shell_src = [
	'e_mod_input_panel.c',
	'e_mod_main.c',
	'wl_shell.c',
	'xdg5.c',
	'xdg6.c',
	'e_mod_main.h',
]

wl_desktop_shell_dir = join_paths(dir_module_e, 'wl_desktop_shell', module_arch)
if get_option('wl-desktop-shell') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_WL_DESKTOP_SHELL', '1')

	protos = [
		'@0@/unstable/xdg-shell/xdg-shell-unstable-v5.xml'.format(dir_wayland_protocols),
		'@0@/unstable/xdg-shell/xdg-shell-unstable-v6.xml'.format(dir_wayland_protocols),
		'@0@/unstable/input-method/input-method-unstable-v1.xml'.format(dir_wayland_protocols),
	]

	foreach proto: protos
		wl_desktop_shell_src += gen_scanner_server.process(proto)
		wl_desktop_shell_src += gen_scanner_impl.process(proto)
	endforeach

        module_files += join_paths(wl_desktop_shell_dir, 'wl_desktop_shell.so')
	shared_module('wl_desktop_shell',
                wl_desktop_shell_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wl_desktop_shell_dir,
                install: true
        )
endif

