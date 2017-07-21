policy_mobile_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_softkey.c',
	'e_mod_main.h',
]

policy_mobile_dir = join_paths(dir_module_e, 'policy_mobile', module_arch)
if get_option('policy-mobile') == true
	config_h.set('USE_MODULE_POLICY_MOBILE', '1')

        module_files += join_paths(policy_mobile_dir, 'policy_mobile.so')
	shared_module('policy_mobile',
                policy_mobile_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: policy_mobile_dir,
                install: true
        )
endif

