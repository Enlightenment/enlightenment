wl_text_input_src = [
	'e_mod_main.c',
]


wl_text_input_dir = join_paths(dir_module_e, 'wl_text_input', module_arch)
if get_option('wl-text-input') == true and get_option('wayland') == true
	config_h.set('USE_MODULE_WL_TEXT_INPUT', '1')

	protos = [
		'@0@/unstable/input-method/input-method-unstable-v1.xml'.format(dir_wayland_protocols),
		'@0@/unstable/text-input/text-input-unstable-v1.xml'.format(dir_wayland_protocols),
	]

	foreach proto: protos
		wl_text_input_src += gen_scanner_server.process(proto)
		wl_text_input_src += gen_scanner_impl.process(proto)
	endforeach

        module_files += join_paths(wl_text_input_dir, 'wl_text_input.so')
	shared_module('wl_text_input',
                wl_text_input_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wl_text_input_dir,
                install: true
        )
endif

