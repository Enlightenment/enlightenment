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
		install_dir: join_paths(dir_config, 'default')
	)
	i += 1
endforeach
foreach cd: config_default_output
	out = join_paths(dir_config, 'default', cd)
	meson.add_install_script(chmod, 'a+r', out)
endforeach
