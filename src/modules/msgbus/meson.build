module = 'msgbus'
opt    = 'msgbus'
conf   = 'USE_MODULE_MSGBUS'

src = [
  'e_mod_main.c',
  'msgbus_audit.c',
  'msgbus_desktop.c',
  'msgbus_lang.c',
  'msgbus_module.c',
  'msgbus_profile.c',
  'msgbus_window.c',
  'e_mod_main.h'
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
