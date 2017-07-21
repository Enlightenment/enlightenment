config_default = ['e.src', 'e_bindings.src']
config_default_output = ['e.cfg', 'e_bindings.cfg']

config_dist_default = ['enlightenment-default.png', 'profile.desktop']
i = 0
foreach cd: config_default
	custom_target(' '.join(['config_dist_default', cd]),
		input: cd,
		command: [eet, '-e', '@OUTPUT@', 'config', '@INPUT@', '1'],
		output: config_default_output[i],
		install: true,
		install_dir: dir_config
	)
	i += 1
endforeach
