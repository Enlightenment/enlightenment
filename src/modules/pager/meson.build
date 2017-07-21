pager_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_main.h',
	'gadget/pager.h',
	'gadget/pager.c',
	'gadget/mod.c',
	'gadget/config.c'
]

pager_dir = join_paths(dir_module_e, 'pager', module_arch)
if get_option('pager') == true
	config_h.set('USE_MODULE_PAGER', '1')

        module_files += join_paths(pager_dir, 'pager.so')
	shared_module('pager',
                pager_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: pager_dir,
                install: true
        )
endif

