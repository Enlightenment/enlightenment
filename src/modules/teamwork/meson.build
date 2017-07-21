teamwork_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_tw.c',
	'e_mod_main.h',
]

if config_h.has('HAVE_WAYLAND') == true
	teamwork_src += [
		'wl.c',
		'wl_teamwork.c',
		'wl_teamwork.h'
	]
endif
if config_h.has('HAVE_WAYLAND_ONLY') == false
	teamwork_src += [
		'x11.c'
	]
endif

teamwork_dir = join_paths(dir_module_e, 'teamwork', module_arch)
if get_option('teamwork') == true
	config_h.set('USE_MODULE_TEAMWORK', '1')

        module_files += join_paths(teamwork_dir, 'teamwork.so')
	shared_module('teamwork',
                teamwork_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: teamwork_dir,
                install: true
        )
endif

