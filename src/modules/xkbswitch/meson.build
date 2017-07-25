module = 'xkbswitch'
opt    = 'xkbswitch'
conf   = 'USE_MODULE_XKBSWITCH'

src = [
  'e_mod_main.c',
  'e_mod_config.c',
  'e_mod_parse.c',
  'e_mod_main.h',
  'e_mod_parse.h',
  'gadget/xkbswitch.h',
  'gadget/xkbswitch.c',
  'gadget/mod.c'
]

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

