if get_option('wayland') != true
  disable = true
else
  src = files(
    'e_mod_main.c',
    'e_mod_input_panel.c',
    'wl_shell.c',
    'xdg5.c',
    'xdg6.c',
    'e_mod_main.h'
   )

  foreach p: [
    '@0@/unstable/xdg-shell/xdg-shell-unstable-v5.xml'.format(dir_wayland_protocols),
    '@0@/unstable/xdg-shell/xdg-shell-unstable-v6.xml'.format(dir_wayland_protocols),
    '@0@/unstable/input-method/input-method-unstable-v1.xml'.format(dir_wayland_protocols),
  ]
    src += gen_scanner_server.process(p)
    src += gen_scanner_impl.process(p)
  endforeach
endif
