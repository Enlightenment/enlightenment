temperature_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_tempget.c',
	'e_mod_udev.c',
	'e_mod_main.h',
]

temperature_dir = join_paths(dir_module_e, 'temperature', module_arch)
if get_option('temperature') == true
	config_h.set('USE_MODULE_TEMPERATURE', '1')

        module_files += join_paths(temperature_dir, 'temperature.so')
	shared_module('temperature',
                temperature_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: temperature_dir,
                install: true
        )
endif

