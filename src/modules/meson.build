easy_modules =['appmenu','backlight','battery','bluez4','clock','conf','conf_applications','conf_bindings','conf_dialogs','conf_display','conf_interaction','conf_intl','conf_menus','conf_paths','conf_performance','conf_randr','conf_shelves','conf_theme','conf_window_manipulation','conf_window_remembers','connman','cpufreq','everything','fileman','fileman_opinfo','gadman','ibar','ibox','luncher','msgbus','notification','packagekit','pager','pager_plain','quickaccess','shot','start','syscon','sysinfo','systray','tasks','teamwork','temperature','tiling','time','winlist','wireless','xkbswitch']
custom_modules = ['geolocation','lokker','mixer','music-control','policy_mobile','wizard','wl_buffer','wl_desktop_shell','wl_drm','wl_text_input','wl_weekeyboard','wl_wl','wl_x11','xwayland']

foreach module_name : custom_modules
  message('Configure module ' + module_name)
  subdir(module_name)
endforeach

foreach module_name : easy_modules
  module_option_name = '-'.join(module_name.split('_'))
  if get_option(module_option_name) == true
    module = []
    config_h.set(''.join(['USE_MODULE_', module_name.to_upper()]), '1')
    message('Configure module ' + module_name)
    subdir(module_name)

    module_source_raw = module.get(0, '')
    module_theme = module.get(1, '')
    module_source = []

    #convert the module sources to relative paths
    foreach s : module_source_raw
      module_source += join_paths([module_name, s])
    endforeach

    #generate the install dirs
    module_object_install_dir = join_paths(dir_module_e, module_name, module_arch)
    module_install_dir = join_paths(dir_module_e, module_name)

    #add the module files
    module_files += join_paths(module_object_install_dir, ''.join([module_name, '.so']))

    #set the module.so to be compiled
    shared_module(module_name,
                  module_source,
                  include_directories: include_directories(general_module_includes),
                  name_prefix: '',
                  dependencies: module_deps,
                  install_dir: module_object_install_dir,
                  install: true
    )

    #install theme if its there
    if module_theme != ''
      install_data(join_paths([module_name, module_theme]),
                   install_dir: module_install_dir,
                   install : true)
    endif

    #install module.desktop
    install_data(join_paths([module_name, 'module.desktop']),
                 install_dir: module_install_dir,
                 install : true)
  endif
endforeach
