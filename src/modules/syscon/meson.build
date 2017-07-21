syscon_src = [
	'e_int_config_syscon.c',
	'e_mod_main.c',
	'e_syscon.c',
	'e_syscon_gadget.c',
	'e_mod_main.h',
]

syscon_dir = join_paths(dir_module_e, 'syscon', module_arch)
if get_option('syscon') == true
	config_h.set('USE_MODULE_SYSCON', '1')

        module_files += join_paths(syscon_dir, 'syscon.so')
	shared_module('syscon',
                syscon_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: syscon_dir,
                install: true
        )
endif

