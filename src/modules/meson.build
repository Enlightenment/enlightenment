mods = [
  'clock',
  'ibar'
]

foreach m: mods
  subdir(m)

  _module = m
  _opt    = '-'.join(m.split('_'))
  _conf   = 'USE_MODULE_' + m.to_upper()
  _src = [ ]
  foreach s: src
    _src += [ join_paths(m, s) ]
  endforeach
  _icon = [
    join_paths(m, 'e-module-' + _module + '.edj'),
    join_paths(m, 'module.desktop')
  ]
  _dir     = join_paths(dir_module_e, _module)
  _dir_bin = join_paths(_dir, module_arch)

  if get_option(_opt) == true
    config_h.set(_conf, '1')
    module_files += join_paths(_dir_bin, _module + '.so')
    install_data(_icon, install_dir: _dir)
    _inc = include_directories(module_includes2,
                               join_paths('.', m))
    shared_module(_module, _src,
                  include_directories: _inc,
                  name_prefix        : '',
                  dependencies       : module_deps,
                  install_dir        : _dir_bin,
                  install            : true
                 )
  endif
endforeach
