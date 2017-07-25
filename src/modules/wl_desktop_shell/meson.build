module = 'wl_desktop_shell'
opt    = 'wl-desktop-shell'
conf   = 'USE_MODULE_WL_DESKTOP_SHELL'

src = [
  'e_mod_main.c',
  'e_mod_input_panel.c',
  'wl_shell.c',
  'xdg5.c',
  'xdg6.c',
  'e_mod_main.h'
]

icon = [
  'e-module-' + module + '.edj',
  'module.desktop'
]

dir_mod     = join_paths(dir_module_e, module)
dir_mod_bin = join_paths(dir_mod, module_arch)

if get_option(opt) == true and get_option('wayland') == true
  config_h.set(conf, '1')
  module_files += join_paths(dir_mod_bin, module + '.so')

  install_data(icon, install_dir: dir_mod)

  protos = [
    '@0@/unstable/xdg-shell/xdg-shell-unstable-v5.xml'.format(dir_wayland_protocols),
    '@0@/unstable/xdg-shell/xdg-shell-unstable-v6.xml'.format(dir_wayland_protocols),
    '@0@/unstable/input-method/input-method-unstable-v1.xml'.format(dir_wayland_protocols),
  ]
  foreach proto: protos
    src += gen_scanner_server.process(proto)
    src += gen_scanner_impl.process(proto)
  endforeach

  shared_module(module, src,
                include_directories: include_directories(module_includes),
                name_prefix        : '',
                dependencies       : module_deps,
                install_dir        : dir_mod_bin,
                install            : true
               )
endif

