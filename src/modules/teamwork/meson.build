module = 'teamwork'
opt    = 'teamwork'
conf   = 'USE_MODULE_TEAMWORK'

src = [
  'e_mod_main.c',
  'e_mod_config.c',
  'e_mod_tw.c',
  'e_mod_main.h'
]

if config_h.has('HAVE_WAYLAND') == true
  src += [
    'wl.c',
    'wl_teamwork.c',
    'wl_teamwork.h'
  ]
endif

if config_h.has('HAVE_WAYLAND_ONLY') == false
  src += [
    'x11.c'
  ]
endif

icon = [
  'e-module-' + module + '.edj',
  'module.desktop'
]

dir_mod     = join_paths(dir_module_e, module)
dir_mod_bin = join_paths(dir_mod, module_arch)

if get_option(opt) == true
  config_h.set(conf, '1')
  module_files += join_paths(dir_mod_bin, module + '.so')

  install_data(icon, install_dir: dir_mod)

  shared_module(module, src,
                include_directories: include_directories(module_includes),
                name_prefix        : '',
                dependencies       : module_deps,
                install_dir        : dir_mod_bin,
                install            : true
               )
endif
