mods = [
  'clock',
  'ibar'
]

foreach m: mods
  subdir(m)

  opt = '-'.join(m.split('_'))

  if get_option(opt) == true
    _icon         = [
      join_paths(m, 'e-module-' + m + '.edj'),
      join_paths(m, 'module.desktop')
    ]
    _conf         = 'USE_MODULE_' + m.to_upper()
    _dir          = join_paths(dir_module_e, m)
    _dir_bin      = join_paths(_dir, module_arch)
    _inc          = include_directories(module_includes2, join_paths('.', m))
    module_files += join_paths(_dir_bin, m + '.so')

    _src = [ ]
    foreach s: src
      _src += [ join_paths(m, s) ]
    endforeach

    config_h.set(_conf, '1')
    install_data(_icon, install_dir: _dir)
    shared_module(m, _src,
                  include_directories: _inc,
                  name_prefix        : '',
                  dependencies       : module_deps,
                  install_dir        : _dir_bin,
                  install            : true
                 )
  endif
endforeach
