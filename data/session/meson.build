desktop_config = configuration_data()
desktop_config.set('prefix', dir_prefix)
e_desktop = configure_file(input        : 'enlightenment.desktop.in',
               output       : 'enlightenment.desktop',
               configuration: desktop_config
              )

install_data(e_desktop,
	install_dir  : join_paths(dir_data, 'xsessions'))

install_data(e_desktop,
	install_dir  : join_paths(dir_data, 'wayland-sessions'))
