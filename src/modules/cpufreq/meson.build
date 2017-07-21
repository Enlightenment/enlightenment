cpufreq_dist = [
	'e-module-cpufreq.edj',
	'module.desktop',
]

cpufreq_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'freqset.c',
	'e_mod_main.h',
]

cpufreq_dir = join_paths(dir_module_e, 'cpufreq', module_arch)
if get_option('cpufreq') == true
	config_h.set('USE_MODULE_CPUFREQ', '1')

	install_data(cpufreq_dist,
		install_dir: join_paths(dir_module_e, 'cpufreq')
	)

        module_files += join_paths(cpufreq_dir, 'cpufreq.so')
	shared_module('cpufreq',
                cpufreq_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: cpufreq_dir,
                install: true
        )

	executable('freqset',
		'freqset.c',
		c_args: suid_cflags,
		link_args: suid_ldflags,
		install_dir: cpufreq_dir,
		install: true
	)
	suid_exes += join_paths(cpufreq_dir, 'freqset')
endif

