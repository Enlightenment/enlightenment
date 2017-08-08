subdir('battery')
subdir('cpufreq')
subdir('wizard')
subdir('mixer')
subdir('everything')
subdir('teamwork')
subdir('wl_desktop_shell')
subdir('wl_text_input')
subdir('wl_weekeyboard')
subdir('sysinfo')

# disabled for now
####subdir('wl_fb')

mods = [
# standard run of the mill modules with cion and desktop
  'clock',
  'ibar',
  'pager',
  'pager_plain',
  'temperature',
  'notification',
  'ibox',
  'start',
  'winlist',
  'fileman',
  'fileman_opinfo',
  'conf',
  'msgbus',
  'music-control',
  'conf_applications',
  'conf_shelves',
  'conf_window_remembers',
  'conf_window_manipulation',
  'conf_menus',
  'conf_dialogs',
  'conf_performance',
  'conf_paths',
  'conf_interaction',
  'gadman',
  'geolocation',
  'connman',
  'bluez4',
  'syscon',
  'systray',
  'appmenu',
  'quickaccess',
  'shot',
  'backlight',
  'tasks',
  'conf_randr',
  'xkbswitch',
  'tiling',
  'packagekit',
  'wireless',
  'time',
  'luncher',
# also standard modules, just with only a desktop file using a generic icon
  'conf_theme',
  'conf_intl',
  'conf_display',
  'conf_bindings',
# also standard modules with no icon or desktop file
  'lokker',
  'wl_x11',
  'wl_wl',
  'wl_buffer',
  'wl_drm',
  'xwayland'
]

foreach m: mods
  desktop_only = false
  no_icon      = false
  disable      = false
  cargs        = ''

  subdir(m)

  opt = '-'.join(m.split('_'))

  if get_option(opt) == true and disable == false

    _conf         = 'USE_MODULE_' + m.underscorify()
    _dir          = join_paths(dir_module_e, m)
    _dir_bin      = join_paths(_dir, module_arch)
    _inc          = include_directories(module_includes2, join_paths('.', m))
    module_files += join_paths(_dir_bin, m + '.so')

    if desktop_only == true
      install_data([ join_paths(m, 'module.desktop')
                   ],
                   install_dir: _dir)
    elif no_icon == true
    else
      install_data([ join_paths(m, 'e-module-' + m + '.edj'),
                     join_paths(m, 'module.desktop')
                   ],
                   install_dir: _dir)
    endif

    config_h.set(_conf, '1')
    if cargs == ''
      shared_module(m, src,
                    include_directories: _inc,
                    name_prefix        : '',
                    dependencies       : module_deps,
                    install_dir        : _dir_bin,
                    install            : true
                   )
    else
      shared_module(m, src,
                    include_directories: _inc,
                    c_args             : cargs,
                    name_prefix        : '',
                    dependencies       : module_deps,
                    install_dir        : _dir_bin,
                    install            : true
                   )
    endif
  endif
endforeach
