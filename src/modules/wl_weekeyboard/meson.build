wl_weekeyboard_dist = [
	'themes/default/default_600.edj',
	'themes/default/default_720.edj',
	'themes/default/default_1080.edj'
]

wl_weekeyboard_src = [
	'e_mod_main.c',
	'wkb-log.c',
	'wkb-log.h',
]

wl_weekeyboard_dir = join_paths(dir_module_e, 'wl_weekeyboard', module_arch)
if get_option('wl-weekeyboard') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_WL_WEEKEYBOARD', '1')

	protos = [
		'@0@/unstable/input-method/input-method-unstable-v1.xml'.format(dir_wayland_protocols),
		'@0@/unstable/text-input/text-input-unstable-v1.xml'.format(dir_wayland_protocols),
	]

	foreach proto: protos
		wl_weekeyboard_src += gen_scanner_server.process(proto)
		wl_weekeyboard_src += gen_scanner_client.process(proto)
		wl_weekeyboard_src += gen_scanner_impl.process(proto)
	endforeach

	install_data(wl_weekeyboard_dist,
		install_dir: join_paths(dir_module_e, 'wl_weekeyboard')
	)
	
        module_files += join_paths(wl_weekeyboard_dir, 'wl_weekeyboard.so')
	shared_module('wl_weekeyboard',
                wl_weekeyboard_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wl_weekeyboard_dir,
                install: true
        )
endif

