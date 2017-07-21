notification_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_popup.c',
	'e_mod_main.h',
]

notification_dir = join_paths(dir_module_e, 'notification', module_arch)
if get_option('notification') == true
	config_h.set('USE_MODULE_NOTIFICATION', '1')

        module_files += join_paths(notification_dir, 'notification.so')
	shared_module('notification',
                notification_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: notification_dir,
                install: true
        )
endif

