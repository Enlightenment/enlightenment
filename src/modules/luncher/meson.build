luncher_src = [
	'bar.c',
	'config.c',
	'grid.c',
	'mod.c',
	'luncher.h',
]

luncher_dir = join_paths(dir_module_e, 'luncher', module_arch)
if get_option('luncher') == true
	config_h.set('USE_MODULE_LUNCHER', '1')

        module_files += join_paths(luncher_dir, 'luncher.so')
	shared_module('luncher',
                luncher_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: luncher_dir,
                install: true
        )
endif

