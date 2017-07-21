if get_option('systemd') == true
	configure_file(input: 'enlightenment.service.in',
		output: 'enlightenment.service',
		install: true,
		install_dir: systemd_unitdir,
		configuration: config_h
	)
endif
