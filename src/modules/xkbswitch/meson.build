xkbswitch_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_parse.c',
	'e_mod_main.h',
	'e_mod_parse.h',
	'gadget/xkbswitch.h',
	'gadget/xkbswitch.c',
	'gadget/mod.c'
]

xkbswitch_dir = join_paths(dir_module_e, 'xkbswitch', module_arch)
if get_option('xkbswitch') == true
	config_h.set('USE_MODULE_XKBSWITCH', '1')

        module_files += join_paths(xkbswitch_dir, 'xkbswitch.so')
	shared_module('xkbswitch',
                xkbswitch_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: xkbswitch_dir,
                install: true
        )
endif

