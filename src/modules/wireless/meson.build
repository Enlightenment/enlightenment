wireless_src = [
	'connman.c',
	'mod.c',
	'wireless.c',
	'wireless.h',
]

wireless_dir = join_paths(dir_module_e, 'wireless', module_arch)
if get_option('wireless') == true
	config_h.set('USE_MODULE_WIRELESS', '1')

        module_files += join_paths(wireless_dir, 'wireless.so')
	shared_module('wireless',
                wireless_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wireless_dir,
                install: true
        )
endif

