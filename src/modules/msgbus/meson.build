msgbus_src = [
	'e_mod_main.c',
	'msgbus_audit.c',
	'msgbus_desktop.c',
	'msgbus_lang.c',
	'msgbus_module.c',
	'msgbus_profile.c',
	'msgbus_window.c',
	'e_mod_main.h',
]

msgbus_dir = join_paths(dir_module_e, 'msgbus', module_arch)
if get_option('msgbus') == true
	config_h.set('USE_MODULE_MSGBUS', '1')

        module_files += join_paths(msgbus_dir, 'msgbus.so')
	shared_module('msgbus',
                msgbus_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: msgbus_dir,
                install: true
        )
endif

