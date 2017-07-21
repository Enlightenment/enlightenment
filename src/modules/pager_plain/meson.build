pager_plain_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

pager_plain_dir = join_paths(dir_module_e, 'pager_plain', module_arch)
if get_option('pager-plain') == true
	config_h.set('USE_MODULE_PAGER_PLAIN', '1')

        module_files += join_paths(pager_plain_dir, 'pager_plain.so')
	shared_module('pager_plain',
                pager_plain_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: pager_plain_dir,
                install: true
        )
endif

