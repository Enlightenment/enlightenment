desktop_config = configuration_data()
desktop_config.set('prefix', dir_prefix)
desktop_config.set('VERSION', e_version_rev)
e_desktop = configure_file(
  input         : 'enlightenment-wayland.desktop.in',
  output        : 'enlightenment-wayland.desktop',
  configuration : desktop_config)

if config_h.has('HAVE_WAYLAND') == true
  install_data(e_desktop,
    install_dir : join_paths(dir_data, 'wayland-sessions'))
endif
