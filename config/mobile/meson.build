config_mobile = ['e.src', 'e_bindings.src', 'module.battery.src', 'module.conf.src']
config_mobile_output = ['e.cfg', 'e_bindings.cfg', 'module.battery.cfg', 'module.conf.cfg']

i = 0
foreach cd: config_mobile
	custom_target(' '.join(['config_dist_mobile', cd]),
		input: cd,
		command: [eet, '-e', '@OUTPUT@', 'config', '@INPUT@', '1'],
		output: config_mobile_output[i],
		install: true,
		install_dir: join_paths(dir_config, 'mobile')
	)
	i += 1
endforeach

config_dist_mobile = ['enlightenment-mobile.png', 'profile.desktop']
install_data(config_dist_mobile,
	install_dir: join_paths(dir_config, 'mobile')
)
foreach cd: config_mobile_output
	out = join_paths(dir_config, 'mobile', cd)
	meson.add_install_script(chmod, 'a+r', out)
endforeach
