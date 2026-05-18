src = files(
  'e_mod_main.c',
  'e_mod_config.c',
  'agent.c',
  'e_networkmanager.c',
  'e_networkmanager.h',
  'e_mod_main.h'
)

# Build our own theme file instead of symlinking connman's.
# Only ships the gadget class icon; the full popup/gadget theme still
# comes from the main Enlightenment theme (e/modules/networkmanager/*
# with connman fallback).
no_icon = true

cmd = [ edje_cmd,
        '-id', join_paths(meson.source_root(), 'src', 'modules', m),
        '@INPUT@', '@OUTPUT@'
      ]
custom_target('e-module-networkmanager.edj',
              input        : 'e-module-networkmanager.edc',
              output       : 'e-module-networkmanager.edj',
              command      : cmd,
              install_dir  : _dir,
              install_mode : 'rw-r--r--',
              install      : true
              )

data = [
  'module.desktop',
]
