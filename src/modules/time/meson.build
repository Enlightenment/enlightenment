time_src = [
	'clock.c',
	'config.c',
	'mod.c',
	'time.c',
	'clock.h',
]

time_dir = join_paths(dir_module_e, 'time', module_arch)
if get_option('time') == true
	config_h.set('USE_MODULE_TIME', '1')

        module_files += join_paths(time_dir, 'time.so')
	shared_module('time',
                time_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: time_dir,
                install: true
        )
endif

