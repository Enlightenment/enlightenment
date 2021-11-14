##### project
#project('e-fprint-gui', 'c',
#        version        : '0.1',
 #       license        : 'BSD 2 clause',
#        default_options: [ 'c_std=gnu99' ],
#        meson_version  : '>= 0.40.0')
#base_url = 'https://github.com/jf-simon/e-fprint-gui'

##### convenience variables for later
proj     = meson.project_name()
ver      = meson.project_version()
cfg      = configuration_data()

##### dependencies
efl_version = '>= 1.19.0'
elm  = dependency('elementary', required: true,  version: efl_version)
edje = dependency('edje',       required: true,  version: efl_version)
ecore = dependency('ecore', required: true,  version: efl_version)
ecorex = dependency('ecore-x', required: true,  version: efl_version)
depe = dependency('enlightenment', required: false)
#curl_dep = dependency('libcurl', version : '>= 7.35.0', required: true)

##### check for windows dependency
#if build_machine.system() == 'windows'
#   win = dependency('evil', version: efl_version)
#endif

##### dir locations
#dir_prefix = get_option('prefix')
#dir_bin    = join_paths(dir_prefix, get_option('bindir'))
#dir_lib    = join_paths(dir_prefix, get_option('libdir'))
#dir_data   = join_paths(dir_prefix, get_option('datadir'))
#dir_locale = join_paths(dir_prefix, get_option('localedir'))

##### config.h
#cfg.set_quoted('PACKAGE'                 , proj)
#cfg.set_quoted('PACKAGE_NAME'            , proj)
#cfg.set_quoted('PACKAGE_VERSION'         , ver)
#cfg.set_quoted('PACKAGE_STRING'          , proj + ' ' + ver)
#cfg.set_quoted('PACKAGE_URL'             , base_url + proj)
#cfg.set_quoted('PACKAGE_BIN_DIR'         , dir_bin)
#cfg.set_quoted('PACKAGE_LIB_DIR'         , dir_lib)
#cfg.set_quoted('BINDIR'           		  , dir_bin)
#cfg.set_quoted('DATADIR'          		  , dir_data)
#cfg.set_quoted('PACKAGE_DATA_DIR'        , join_paths(dir_data, proj))
#cfg.set_quoted('LOCALEDIR'               , dir_locale)

#add_global_arguments('-DPACKAGE_BIN_DIR="@0@"'.format(dir_bin), language: 'c')
#add_global_arguments('-DPACKAGE_LIB_DIR="@0@"'.format(dir_lib), language: 'c')
#add_global_arguments('-DPACKAGE_DATA_DIR="@0@"'.format(join_paths(dir_data, proj)), language: 'c')

##### get C compiler
#cc = meson.get_compiler('c')

##### get edje command
#edje_cmd = join_paths(edje.get_pkgconfig_variable('prefix'),
#                      'bin', 'edje_cc')

##### Check for windows
#if build_machine.system() == 'windows'
#    cfg.set       ('_WIN32'              , 1)
#endif
##### Check for arpa/inet and netinet/in.h
#if cc.has_header('arpa/inet.h') == true
#    cfg.set       ('HAVE_ARPA_INET_H'    , 1)
#endif
#if cc.has_header('netinet/in.h') == true
#    cfg.set       ('HAVE_NETINET_IN_H'   , 1)
#endif
#if exif.found() == true
#    cfg.set       ('HAVE_LIBEXIF'        , 1)
#endif
#if depe.found() == true
#    build_gadget = true
#    cfg.set       ('HAVE_E'              , 1)
#endif
###### translations
#use_translations = false
#depnls = []
#intl_lib = cc.find_library('intl', required: false)
#if intl_lib.found()
#  cfg.set('HAVE_GETTEXT', 1)
#  cfg.set('ENABLE_NLS', 1)
#  depnls = [intl_lib]
#  use_translations = true
#else
#  gettext_code = '''
#  #include <libintl.h>
#  int main(int argc, char *argv[]) {
#      (void)ngettext("", "", 0);
#      return 0;
#  }
#  '''
#  if cc.links(gettext_code)
#    cfg.set('HAVE_GETTEXT', 1)
#    cfg.set('ENABLE_NLS', 1)
#    use_translations = true
#  endif
#endif
#configure_file(output: 'config.h', configuration: cfg)

#install_data('AUTHORS',
#             install_dir: join_paths(dir_data, 'e-fprint-gui'))

#if use_translations
#  subdir('po')
#endif

eldbus_codegen = find_program('eldbus-codegen')

subdir('src')
subdir('data')
