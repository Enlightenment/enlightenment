module = 'wizard'
opt    = 'wizard'
conf   = 'USE_MODULE_WIZARD'

src = [
  'e_mod_main.c',
  'e_wizard.c',
  'e_wizard.h'
]

pages = [
  'page_000',
  'page_010',
  'page_011',
  'page_020',
  'page_030',
  'page_040',
  'page_050',
  'page_060',
  'page_065',
  'page_070',
  'page_080',
  'page_090',
  'page_100',
  'page_110',
  'page_120',
  'page_130',
  'page_150',
  'page_160',
  'page_170',
  'page_180',
  'page_200'
]

ibar_def = [
  'data/def-ibar.txt'
]

desktops = [
  'data/desktop/home.desktop',
  'data/desktop/root.desktop',
  'data/desktop/tmp.desktop'
]

dir_mod     = join_paths(dir_module_e, module)
dir_mod_bin = join_paths(dir_mod, module_arch)

if get_option(opt) == true
  config_h.set(conf, '1')
  module_files += join_paths(dir_mod_bin, module + '.so')

  install_data(ibar_def, install_dir: dir_mod)
  install_data(desktops, install_dir: join_paths(dir_mod, 'desktop'))

  shared_module(module, src,
                include_directories: include_directories(module_includes),
                name_prefix        : '',
                dependencies       : module_deps,
                install_dir        : dir_mod_bin,
                install            : true
               )
  foreach page: pages
    shared_module(page, '@0@.c'.format(page),
                  include_directories: include_directories(module_includes),
                  name_prefix        : '',
                  dependencies       : module_deps,
                  install_dir        : dir_mod_bin,
                  install            : true
                 )
  endforeach
endif

