module = 'music_control'
opt    = 'music-control'
conf   = 'USE_MODULE_MUSIC_CONTROL'

src = [
  'e_mod_main.c',
  'ui.c',
  'e_mod_main.h',
  'private.h'
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

  gen_dbus = custom_target('gen-dbus',
                           input  : 'introspect.xml',
                           command: [eldbus_codegen, '@INPUT@', '-O', '@OUTDIR@'],
                           output : [
                             'eldbus_media_player2_player.c',
                             'eldbus_mpris_media_player2.c',
                             'eldbus_media_player2_player.h',
                             'eldbus_mpris_media_player2.h',
                             'eldbus_utils.h'
                             ]
                            )
  shared_module(module, [src, gen_dbus] ,
                include_directories: include_directories(module_includes),
                name_prefix        : '',
                dependencies       : module_deps,
                install_dir        : dir_mod_bin,
                install            : true
               )
endif
