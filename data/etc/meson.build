if get_option('install-sysactions')
	configure_file(input: 'sysactions.conf.in',
		output: 'sysactions.conf',
		install: true,
		install_dir: join_paths(dir_sysconf, 'enlightenment'),
		configuration: sysactions
	)
endif

if get_option('install-enlightenment-menu')
	install_data('e-applications.menu',
		install_dir: join_paths(dir_sysconf, 'xdg/menus')
	)
endif

