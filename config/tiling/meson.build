config_tiling = ['e.src', 'e_bindings.src',
	'module.battery.src',
	'module.cpufreq.src',
	'module.fileman.src',
	'module.ibar.src',
	'module.ibox.src',
	'module.pager.src',
	'module.temperature.src',
	'module.tiling.src',
	'module.conf.src'
]

config_tiling_output = ['e.cfg', 'e_bindings.cfg',
	'module.battery.cfg',
	'module.cpufreq.cfg',
	'module.fileman.cfg',
	'module.ibar.cfg',
	'module.ibox.cfg',
	'module.pager.cfg',
	'module.temperature.cfg',
	'module.tiling.cfg',
	'module.conf.cfg'
]

i = 0
foreach cd: config_tiling
	custom_target(' '.join(['config_dist_tiling', cd]),
		input: cd,
		command: [eet, '-e', '@OUTPUT@', 'config', '@INPUT@', '1'],
		output: config_tiling_output[i],
		install: true,
		install_dir: dir_config
	)
	i += 1
endforeach

config_dist_tiling = ['enlightenment-tiling.png', 'profile.desktop']
install_data(config_dist_tiling,
	install_dir: join_paths(dir_config, 'tiling')
)
