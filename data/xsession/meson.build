desktop_config = configuration_data()
desktop_config.set('prefix', dir_prefix)
configure_file(input        : 'enlightenment.desktop.in',
               output       : 'enlightenment.desktop',
               install      : true,
               install_dir  : join_paths(dir_data, 'xsessions'),
               configuration: desktop_config
              )
