conf_bindings_dist = [
	'module.desktop',
]

conf_bindings_src = [
	'e_int_config_acpibindings.c',
	'e_int_config_edgebindings.c',
	'e_int_config_keybindings.c',
	'e_int_config_mousebindings.c',
	'e_int_config_signalbindings.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

conf_bindings_dir = join_paths(dir_module_e, 'conf_bindings', module_arch)
if get_option('conf-bindings') == true
	config_h.set('USE_MODULE_CONF_BINDINGS', '1')

	install_data(conf_bindings_dist,
		install_dir: join_paths(dir_module_e, 'conf_bindings')
	)

        module_files += join_paths(conf_bindings_dir, 'conf_bindings.so')
	shared_module('conf_bindings',
                conf_bindings_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: conf_bindings_dir,
                install: true
        )
endif

