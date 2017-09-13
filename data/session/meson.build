desktop_config = configuration_data()
desktop_config.set('prefix', dir_prefix)
e_desktop = configure_file(input        : 'enlightenment.desktop.in',
               output       : 'enlightenment.desktop',
               configuration: desktop_config
              )

if config_h.has('HAVE_WAYLAND_ONLY') == false
	install_data(e_desktop,
		install_dir  : join_paths(dir_data, 'xsessions'))
endif

if config_h.has('HAVE_WAYLAND') == false
	install_data(e_desktop,
		install_dir  : join_paths(dir_data, 'wayland-sessions'))
endif
