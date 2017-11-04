##### project
project('enlightenment', 'c',
        version        : '0.22.0',
        license        : 'BSD 2 clause',
        default_options: [ 'c_std=gnu99', 'warning_level=2' ],
        meson_version  : '>= 0.39.0')

##### convenience variables for later
proj     = meson.project_name()
ver      = meson.project_version()
chmod    = join_paths(meson.source_root(), 'meson/meson-chmod.sh')

#### check for mixed autofoo and meson build tree mess
clean_check = run_command('meson/clean_check.sh')
if clean_check.returncode() == 0
  error('Meson build requires a clean source tree')
endif

#e_version   = ver.split('.')
#git_version = '0'
#git         = find_program('git', required: false)
#if git.found() == true
#  git_cmd = run_command(git.path(), 'rev-list', '--count', 'HEAD')
#  if git_cmd.returncode() == 0
#    git_version = git_cmd.stdout().strip()
#  endif
#endif

#e_version_rev = '.'.join([ver, git_version])
e_version_rev = ver

efl_version = '>= 1.20.5'

release = '0.22'
#for releases
#release = '@0@.@1@.@2@'.format(e_version[0], e_version[1], e_version[2])

add_global_arguments('-DHAVE_CONFIG_H=1', language: 'c')
dir_prefix    = get_option('prefix')
dir_bin       = join_paths(dir_prefix, get_option('bindir'))
dir_sysconf   = get_option('sysconfdir')
dir_data      = join_paths(dir_prefix, get_option('datadir'))
dir_include   = join_paths(dir_prefix, get_option('includedir'))
dir_include_e = join_paths(dir_include, proj)
dir_lib       = join_paths(dir_prefix, get_option('libdir'))
dir_module_e  = join_paths(dir_lib, proj, 'modules')
dir_util_e    = join_paths(dir_lib, proj, 'utils')
dir_pc        = join_paths(dir_data, 'pkgconfig')

suid_exes = []

pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')
dev_cflags = []

# disable for release builds
dev_cflags_try = [
  '-Wall',
  '-W',
  '-Wpointer-arith',
  '-Wshadow',
  '-Wno-missing-field-initializers',
  '-Wfloat-equal',
  '-Wuninitialized',
  '-Wundef',
  '-Wcast-align',
  '-Wformat=2',
  '-Wno-format-y2k',
]

foreach cf: dev_cflags_try
  if cc.has_argument(cf) == true
    dev_cflags += cf
  endif
endforeach
add_global_arguments(dev_cflags, language: 'c')

config_h = configuration_data()
config_h.set('E_INTERNAL'              , '1')
config_h.set('_GNU_SOURCE'             , '1')
config_h.set('_ALL_SOURCE'             , '1')
config_h.set('_POSIX_PTHREAD_SEMANTICS', '1')
config_h.set('_TANDEM_SOURCE'          , '1')
config_h.set('__EXTENSIONS__'          , '1')
config_h.set('E_RELEASE_BUILD'         , '1')

if get_option('nls') == true
  subdir('po')
endif

darwin = host_machine.system().startswith('darwin')
if darwin == true
  code='''
#define _GNU_SOURCE 1
#include <unistd.h>
extern char **environ;
'''
  if cc.links(code, name: 'environ test')
    config_h.set('HAVE_ENVIRON', '1')
  endif
endif

openbsd      = host_machine.system().startswith('openbsd')
netbsd       = host_machine.system().startswith('netbsd')
freebsd      = host_machine.system().startswith('freebsd')
suid_cflags  = []
suid_ldflags = []
dep_crypt    = []
if freebsd == true or host_machine.system().startswith('pcbsd')
  dep_crypt = cc.find_library('crypt', required: true)
  if cc.links('', args: '-fPIE', link_args: '-pie') == true
    suid_cflags  = '-fPIE'
    suid_ldflags = '-pie'
  endif
endif
dragonfly = host_machine.system().startswith('dragonfly')

#FIXME: other variants...
host_os = host_machine.system()
if host_os == 'linux'
  if cc.has_header_symbol('features.h', '__UCLIBC__')
    host_os = 'linux-uclibc'
  elif cc.has_header_symbol('features.h', '__dietlibc__')
    host_os = 'linux-dietlibc'
  else
    host_os = 'linux-gnu'
  endif
endif

module_arch = '@0@-@1@-@2@'.format(host_os, host_machine.cpu_family(), release)

config_h.set_quoted('MODULE_ARCH'      , module_arch)
config_h.set_quoted('PACKAGE'          , proj)
config_h.set_quoted('PACKAGE_VERSION'  , e_version_rev)
config_h.set_quoted('VERSION'          , e_version_rev)
config_h.set_quoted('LOCALE_DIR'       , join_paths([dir_prefix, 'share/locale']))
config_h.set_quoted('PACKAGE_URL'      , 'https://www.enlightenment.org')
config_h.set_quoted('PACKAGE_TARNAME'  , proj)
config_h.set_quoted('PACKAGE_BUGREPORT', 'enlightenment-devel@lists.sourceforge.net')
config_h.set_quoted('PACKAGE_STRING'   , proj + ' ' + e_version_rev)
config_h.set_quoted('PACKAGE_NAME'     , proj)
config_h.set_quoted('BINDIR'           , dir_bin)
config_h.set_quoted('DATADIR'          , dir_data)

if cc.has_function('setenv') == true
  config_h.set('HAVE_SETENV'           , '1')
endif
if cc.has_function('unsetenv') == true
  config_h.set('HAVE_UNSETENV'         , '1')
endif
if cc.has_function('clearenv') == true
  config_h.set('HAVE_CLEARENV'         , '1')
endif
if cc.has_header('features.h') == true
  config_h.set('HAVE_FEATURES_H'       , '1')
endif
if cc.has_header('sys/ptrace.h') == true
  config_h.set('HAVE_SYS_PTRACE_H'     , '1')
endif
if cc.has_header('arpa/inet.h') == true
  config_h.set('HAVE_ARPA_INET_H'      , '1')
endif
if cc.has_header('netinet/in.h') == true
  config_h.set('HAVE_NETINET_IN_H'     , '1')
endif
if cc.has_header('execinfo.h') == true
  config_h.set('HAVE_EXECINFO_H'       , '1')
elif cc.has_function('backtrace_symbols_fd', dependencies: 'execinfo') == false
  execinfo_dep = dependency('execinfo', required: false)
endif

if cc.has_header('fnmatch.h') == false
  error('fnmatch.h not found')
endif

if cc.has_function('fnmatch') == false
  dep_fnmatch = dependency('fnmatch', required: true)
endif

add_global_arguments('-DPACKAGE_BIN_DIR="@0@"'.format(dir_bin), language: 'c')
add_global_arguments('-DPACKAGE_LIB_DIR="@0@"'.format(dir_lib), language: 'c')
add_global_arguments('-DPACKAGE_DATA_DIR="@0@"'.format(join_paths(dir_data, proj)), language: 'c')
add_global_arguments('-DPACKAGE_SYSCONF_DIR="@0@"'.format(dir_sysconf), language: 'c')

if cc.has_header('CFBase.h',
  args: '-I/System/Library/Frameworks/CoreFoundation.framework/Headers') == true
  add_global_arguments('-I/System/Library/Frameworks/CoreFoundation.framework/Headers', language: 'c')
  add_global_arguments('-I /System/Library/Frameworks/IOKit.framework/Headers', language: 'c')
  add_global_link_arguments('-framework CoreFoundation', '-framework IOKit')
endif

if get_option('pam') == true
  if cc.has_header('security/pam_appl.h')
    config_h.set('HAVE_PAM', '1')
    dep_pam = cc.find_library('pam', required: true)
  endif
endif

dep_m  = cc.find_library('m', required: false)
dep_dl = cc.find_library('dl', required: false)

dep_bluez = dependency('bluez', required: false)
if dep_bluez.found() == true
  config_h.set('HAVE_BLUETOOTH', '1')
endif

systemd_unitdir = ''
if get_option('systemd') == true
  systemd_unitdir = get_option('systemdunitdir')
  if systemd_unitdir == ''
    dep_systemd = dependency('systemd', required: true)
    systemd_unitdir = dep_systemd.get_pkgconfig_variable('systemduserunitdir')
  endif
endif

if cc.has_function('eeze_disk_function', dependencies: dependency('eeze')) == true
  config_h.set('HAVE_EEZE_MOUNT', '1')
  eeze_mount = true
endif

if get_option('files') == true
  config_h.set('ENABLE_FILES', '1')
endif

dep_eeze = []
requires_eeze = ''
if get_option('device-udev') == true
  dep_eeze = dependency('eeze', required: true)
  config_h.set('HAVE_EEZE', '1')
  requires_eeze = 'eeze'
endif

if get_option('mount-udisks') == true
  config_h.set('HAVE_UDISKS_MOUNT', '1')
endif

if get_option('mount-eeze') == true
  if config_h.has('HAVE_EEZE_MOUNT') == false
    error('eeze not compiled with mount support')
  endif
  if get_option('device-udev') == false
    dep_eeze      = dependency('eeze', required: true)
    requires_eeze = 'eeze'
  endif
endif

dep_rt               = cc.find_library('rt', required: false)
dep_eina             = dependency('eina'            , required: true, version: efl_version)
dep_eet              = dependency('eet'             , required: true)
dep_ecore            = dependency('ecore'           , required: true)
dep_ecore_ipc        = dependency('ecore-ipc'       , required: true)
dep_ecore_file       = dependency('ecore-file'      , required: true)
dep_ecore_con        = dependency('ecore-con'       , required: true)
dep_ecore_input      = dependency('ecore-input'     , required: true)
dep_ecore_input_evas = dependency('ecore-input-evas', required: true)
dep_ecore_evas       = dependency('ecore-evas'      , required: true)
dep_evas             = dependency('evas'            , required: true)
dep_edje             = dependency('edje'            , required: true)
dep_efreet           = dependency('efreet'          , required: true)
dep_efreet_mime      = dependency('efreet-mime'     , required: true)
dep_efreet_trash     = dependency('efreet-trash'    , required: true)
dep_eio              = dependency('eio'             , required: true)
dep_eo               = dependency('eo'              , required: true)
dep_eldbus           = dependency('eldbus'          , required: true)
dep_emotion          = dependency('emotion'         , required: true)
dep_elementary       = dependency('elementary'      , required: true)

dep_wayland = []
if get_option('wayland') == true
  wayland_protocols = dependency('wayland-protocols', version: '>= 1.9')
  dir_wayland_protocols = wayland_protocols.get_pkgconfig_variable('pkgdatadir')
  wayland_version = '>= 1.11.0'
  dep_wayland = [ dependency('ecore-wl2'),
                  dependency('wayland-server' , version: wayland_version),
                  dependency('wayland-client' , version: wayland_version),
                  wayland_protocols,
                  dependency('wayland-scanner', version: wayland_version),
                  dependency('xkbcommon'),
                  dependency('uuid')
                ]
  dep_ecore_drm2 = dependency('ecore-drm2', required: get_option('wl-drm'))
  requires_drm = ''
  if get_option('wl-drm') == true
    requires_drm = 'ecore-drm2'
    dep_wayland += dep_ecore_drm2
  endif
  requires_wayland = ' '.join([ 'wayland-protocols >= 1.9',
                                'ecore-wl2',
                                requires_drm,
                                ' '.join(['wayland-server' , wayland_version]),
                                ' '.join(['wayland-client' , wayland_version]),
                                ' '.join(['wayland-scanner', wayland_version]),
                                'xkbcommon',
                                'uuid'
                              ])
  wayland_scanner = find_program('wayland-scanner')
  gen_scanner_client = generator(wayland_scanner,
                                 output   : '@BASENAME@-client-protocol.h',
                                 arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
  gen_scanner_server = generator(wayland_scanner,
                                 output   : '@BASENAME@-server-protocol.h',
                                 arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
  gen_scanner_impl = generator(wayland_scanner,
                               output   : '@BASENAME@-protocol.c',
                               arguments: ['code', '@INPUT@', '@OUTPUT@'])
  config_h.set('HAVE_WAYLAND', '1')
endif

dep_ecore_x = []
if get_option('wayland') == true and get_option('wl-x11') == false and get_option('xwayland') == false
  config_h.set('HAVE_WAYLAND_ONLY', '1')
else
  dep_ecore_x = dependency('ecore-x')
endif

dep_xkeyboard_config = dependency('xkeyboard-config', required: false)
if dep_xkeyboard_config.found() == true
  config_h.set_quoted('XKB_BASE', dep_xkeyboard_config.get_pkgconfig_variable('xkb_base'))
endif

eldbus_codegen = find_program('eldbus-codegen')

requires_e = []

dir_pkgconfig = join_paths(dir_lib, 'pkgconfig')
if freebsd == true
  dir_pkgconfig = join_paths(dir_lib, 'data/pkgconfig')
endif

##### edje_cc binary compiler tool
edje_cmd = get_option('edje-cc')
if edje_cmd == ''
  edje_cmd = join_paths(dep_edje.get_pkgconfig_variable('prefix'),
                        'bin', 'edje_cc')
endif

subdir('src/bin')

subdir('src/modules')

subdir('config')

subdir('data/backgrounds')
subdir('data/desktop')
subdir('data/etc')
subdir('data/favorites')
subdir('data/flags')
subdir('data/icons')
subdir('data/images')
subdir('data/input_methods')
subdir('data/tools')
subdir('data/units')
subdir('data/session')

subdir('doc')

meson.add_install_script('meson/meson_inst.sh'   , suid_exes)
meson.add_install_script('meson/meson_modules.sh', module_files)

wayland = 'false'
if config_h.has('HAVE_WAYLAND') == true
	wayland = 'true'
endif

configure_file(output       : 'config.h',
               install      : false,
               configuration: config_h)

pkgconfig.generate(name             : proj,
                   description      : 'Enlightenment Window Manager',
                   filebase         : proj,
                   subdirs          : proj,
                   requires         : requires_e,
                   version          : e_version_rev,
                   libraries_private: '-lm',
                   install_dir      : dir_pkgconfig,
                   variables        : [ 'exec_prefix=${prefix}',
                                        'datarootdir=@0@'.format(dir_data),
                                        'datadir=${datarootdir}',
                                        'modules=@0@/enlightenment/modules'.format(dir_lib),
                                        'pkgdatadir=${datarootdir}/' + proj + '/data',
                                        'themes=${pkgdatadir}/themes',
                                        'backgrounds=${pkgdatadir}/backgrounds',
                                        'release=@0@'.format(release),
                                        'wayland=@0@'.format(wayland)
                                      ]
                  )

install_data([ 'AUTHORS',
               'COPYING'
             ],
             install_dir: join_paths(dir_data, 'enlightenment'))
