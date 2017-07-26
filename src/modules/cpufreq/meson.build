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

module += [cpufreq_src, 'e-module-cpufreq.edj']

cpufreq_dir = join_paths(dir_module_e, 'cpufreq', module_arch)

executable('freqset',
    'freqset.c',
    c_args: suid_cflags,
    link_args: suid_ldflags,
    install_dir: cpufreq_dir,
    install: true
)
suid_exes += join_paths(cpufreq_dir, 'freqset')
