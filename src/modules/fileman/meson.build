fileman_dist = [
	'e-module-fileman.edj',
	'module.desktop',
]

fileman_src = [
	'e_fwin.c',
	'e_fwin_nav.c',
	'e_int_config_mime.c',
	'e_int_config_mime_edit.c',
	'e_mod_config.c',
	'e_mod_dbus.c',
	'e_mod_main.c',
	'e_mod_menu.c',
	'e_mod_main.h',
]

fileman_dir = join_paths(dir_module_e, 'fileman', module_arch)
if get_option('fileman') == true
	config_h.set('USE_MODULE_FILEMAN', '1')

	install_data(fileman_dist,
		install_dir: join_paths(dir_module_e, 'fileman')
	)

        module_files += join_paths(fileman_dir, 'fileman.so')
	shared_module('fileman',
                fileman_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: fileman_dir,
                install: true
        )
endif

