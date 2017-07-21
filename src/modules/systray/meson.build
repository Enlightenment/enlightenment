systray_src = [
	'e_mod_main.c',
	'e_mod_notifier_host.c',
	'e_mod_notifier_host_dbus.c',
	'e_mod_notifier_watcher.c',
	'e_mod_main.h',
	'e_mod_notifier_host_private.h',
]

systray_dir = join_paths(dir_module_e, 'systray', module_arch)
if get_option('systray') == true
	config_h.set('USE_MODULE_SYSTRAY', '1')

        module_files += join_paths(systray_dir, 'systray.so')
	shared_module('systray',
                systray_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: systray_dir,
                install: true
        )
endif

