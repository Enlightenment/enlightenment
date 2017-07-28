project('enlightenment', 'c',
	version: '0.21.99',
	license: 'BSD 2 clause',
	default_options: [ 'c_std=gnu99', 'warning_level=2' ],
	meson_version: '>= 0.40.0')

proj     = meson.project_name()
ver      = meson.project_version()
chmod    = join_paths(meson.source_root(), 'meson-chmod.sh')

clean_check = run_command('meson/clean_check.sh')
if clean_check.returncode() == 0
	error('Meson build requires a clean source tree')
endif

e_version = meson.project_version().split('.')
git_version = '0'
git = find_program('git')
if git.found() == true
	git_cmd = run_command(git.path(), 'rev-list', '--count', 'HEAD')
	if git_cmd.returncode() == 0
		git_version = git_cmd.stdout().strip()
	endif
endif	
e_version_rev = '.'.join([meson.project_version(), git_version])
#e_version_rev = e_version

efl_version = '>= 1.19.99'

release = 'ver-0.22'
#for releases
#release = '@0@.@1@.@2@'.format(e_version[0], e_version[1], e_version[2])

add_global_arguments('-DHAVE_CONFIG_H=1', language: 'c')
dir_prefix = get_option('prefix')
dir_bin = join_paths(dir_prefix, get_option('bindir'))
dir_sysconf = get_option('sysconfdir')
dir_data = join_paths(dir_prefix, get_option('datadir'))
dir_include = join_paths(dir_prefix, get_option('includedir'))
dir_include_e = join_paths(dir_include, proj)
dir_lib = join_paths(dir_prefix, get_option('libdir'))
dir_module_e = join_paths(dir_lib, proj, 'modules')
dir_util_e = join_paths(dir_lib, proj, 'utils')
dir_pc = join_paths(dir_data, 'pkgconfig')

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
config_h.set('E_INTERNAL', '1')
config_h.set('_GNU_SOURCE', '1')
config_h.set('_ALL_SOURCE', '1')
config_h.set('_POSIX_PTHREAD_SEMANTICS', '1')
config_h.set('_TANDEM_SOURCE', '1')
config_h.set('__EXTENSIONS__', '1')
#config_h.set('E_RELEASE_BUILD', '1')

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
openbsd = host_machine.system().startswith('openbsd')
netbsd = host_machine.system().startswith('netbsd')
freebsd = host_machine.system().startswith('freebsd')
suid_cflags = []
suid_ldflags = []
dep_crypt = []
if freebsd == true or host_machine.system().startswith('pcbsd')
	dep_crypt = cc.find_library('crypt', required: true)
	if cc.links('', args: '-fPIE', link_args: '-pie') == true
		suid_cflags = '-fPIE'
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
config_h.set_quoted('MODULE_ARCH', module_arch)

config_h.set_quoted('PACKAGE', proj)
config_h.set_quoted('PACKAGE_VERSION', e_version_rev)
config_h.set_quoted('VERSION', e_version_rev)

add_global_arguments('-DPACKAGE_BIN_DIR="@0@"'.format(dir_bin), language: 'c')
add_global_arguments('-DPACKAGE_LIB_DIR="@0@"'.format(dir_lib), language: 'c')
add_global_arguments('-DPACKAGE_DATA_DIR="@0@"'.format(join_paths(dir_data, proj)), language: 'c')
add_global_arguments('-DPACKAGE_SYSCONF_DIR="@0@"'.format(dir_sysconf), language: 'c')

config_h.set_quoted('LOCALE_DIR', join_paths([dir_prefix, 'share/locale']))
config_h.set_quoted('PACKAGE_URL', 'https://www.enlightenment.org')
config_h.set_quoted('PACKAGE_TARNAME', proj)
config_h.set_quoted('PACKAGE_BUGREPORT', 'enlightenment-devel@lists.sourceforge.net')
config_h.set_quoted('PACKAGE_STRING', proj + ' ' + e_version_rev)
config_h.set_quoted('PACKAGE_NAME', proj)

config_h.set_quoted('BINDIR', dir_bin)
config_h.set_quoted('DATADIR', dir_data)

if cc.has_function('setenv') == true
	config_h.set('HAVE_SETENV', '1')
endif
if cc.has_function('unsetenv') == true
	config_h.set('HAVE_UNSETENV', '1')
endif
if cc.has_function('clearenv') == true
	config_h.set('HAVE_CLEARENV', '1')
endif
if cc.has_header('features.h') == true
	config_h.set('HAVE_FEATURES_H', '1')
endif
if cc.has_header('sys/ptrace.h') == true
	config_h.set('HAVE_SYS_PTRACE_H', '1')
endif
if cc.has_header('arpa/inet.h') == true
	config_h.set('HAVE_ARPA_INET_H', '1')
endif
if cc.has_header('netinet/in.h') == true
	config_h.set('HAVE_NETINET_IN_H', '1')
endif
if cc.has_header('execinfo.h') == true
	config_h.set('HAVE_EXECINFO_H', '1')
else
	if cc.has_function('backtrace_symbols_fd', dependencies: 'execinfo') == false
		execinfo_dep = dependency('execinfo', required: false)
	endif
endif

if cc.has_header('fnmatch.h') == false
	error('fnmatch.h not found')
endif
if cc.has_function('fnmatch') == false
	dep_fnmatch = dependency('fnmatch', required: true)
endif

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

dep_m = cc.find_library('m', required: false)
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


HALT = '/sbin/shutdown -h now'
REBOOT = '/sbin/shutdown -r now'
SUSPEND = ''
HIBERNATE = ''
MOUNT = '/bin/mount'
UMOUNT = '/bin/umount'
EJECT = '/usr/bin/eject'

if host_machine.system().contains('bsd') == true
	#SUSPEND = 'acpiconf -s3'
	SUSPEND = '/usr/sbin/zzz'
	HIBERNATE = 'acpiconf -s4'
	HALT = '/sbin/shutdown -p now'
	MOUNT = '/sbin/mount'
	UMOUNT = '/sbin/umount'
	EJECT = '/usr/sbin/cdcontrol eject'
else
	if systemd_unitdir != ''
		HALT = '/usr/bin/systemctl poweroff'
		REBOOT = '/usr/bin/systemctl reboot'
		SUSPEND = '/usr/bin/systemctl suspend'
		HIBERNATE = '/usr/bin/systemctl hibernate'
	else
		sleep = find_program('sleep.sh', '/etc/acpi/sleep.sh', '/sbin/sleep.sh', '/usr/sbin/sleep.sh')
		if sleep.found() == true
			SUSPEND = sleep.path()
			if sleep.path() == '/etc/acpi/sleep.sh'
				SUSPEND = '/etc/acpi/sleep.sh force'
			endif
		else
			suspend = find_program('pm-suspend', '/etc/acpi/pm-suspend', '/sbin/pm-suspend', '/usr/sbin/pm-suspend')
			if suspend.found()
				SUSPEND = suspend.path()
			endif
		endif

		hibernate_sh = find_program('hibernate.sh', '/etc/acpi/hibernate.sh', '/sbin/hibernate.sh', '/usr/sbin/hibernate.sh')
		if hibernate_sh.found() == true
			HIBERNATE = hibernate_sh.path()
			if hibernate_sh.path() == '/etc/acpi/hibernate.sh'
				SUSPEND = '/etc/acpi/hibernate.sh force'
			endif
		else
			hibernate = find_program('pm-hibernate', '/etc/acpi/pm-hibernate', '/sbin/pm-hibernate', '/usr/sbin/pm-hibernate')
			if hibernate.found()
				HIBERNATE = hibernate.path()
			endif
		endif
	endif
endif

sysactions = configuration_data()
sysactions.set('HALT', HALT)
sysactions.set('REBOOT', REBOOT)
sysactions.set('SUSPEND', SUSPEND)
sysactions.set('HIBERNATE', HIBERNATE)
sysactions.set('MOUNT', MOUNT)
sysactions.set('UMOUNT', UMOUNT)
sysactions.set('EJECT', EJECT)

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
		dep_eeze = dependency('eeze', required: true)
		requires_eeze = 'eeze'
	endif
endif

dep_rt = cc.find_library('rt', required: false)
dep_eina = dependency('eina', version: efl_version, required: true)
dep_eet = dependency('eet', required: true)
dep_ecore = dependency('ecore', required: true)
dep_ecore_ipc = dependency('ecore-ipc', required: true)
dep_ecore_file = dependency('ecore-file', required: true)
dep_ecore_con = dependency('ecore-con', required: true)
dep_ecore_input = dependency('ecore-input', required: true)
dep_ecore_input_evas = dependency('ecore-input-evas', required: true)
dep_ecore_evas = dependency('ecore-evas', required: true)
dep_evas = dependency('evas', required: true)
dep_edje = dependency('edje', required: true)
dep_efreet = dependency('efreet', required: true)
dep_efreet_mime = dependency('efreet-mime', required: true)
dep_efreet_trash = dependency('efreet-trash', required: true)
dep_eio = dependency('eio', required: true)
dep_eo = dependency('eo', required: true)
dep_eldbus = dependency('eldbus', required: true)
dep_emotion = dependency('emotion', required: true)
dep_elementary = dependency('elementary', required: true)

dep_wayland = []
if get_option('wayland') == true
	wayland_protocols = dependency('wayland-protocols', version: '>= 1.7')
	dir_wayland_protocols = wayland_protocols.get_pkgconfig_variable('pkgdatadir')
	wayland_version = '>= 1.11.0'
	dep_wayland = [
		dependency('ecore-wl2'),
		dependency('wayland-server', version: wayland_version),
		dependency('wayland-client', version: wayland_version),
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
	requires_wayland = ' '.join([
		'wayland-protocols >= 1.7',
		'ecore-wl2',
		requires_drm,
		' '.join(['wayland-server', wayland_version]),
		' '.join(['wayland-client', wayland_version]),
		' '.join(['wayland-scanner', wayland_version]),
		'xkbcommon',
		'uuid'
	])
	wayland_scanner = find_program('wayland-scanner')
	gen_scanner_client = generator(wayland_scanner,
		output: '@BASENAME@-client-protocol.h',
		arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
	gen_scanner_server = generator(wayland_scanner,
		output: '@BASENAME@-server-protocol.h',
		arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
	gen_scanner_impl = generator(wayland_scanner,
		output: '@BASENAME@-protocol.c',
		arguments: ['code', '@INPUT@', '@OUTPUT@'])
	config_h.set('HAVE_WAYLAND', '1')
endif

dep_ecore_x = []
if get_option('wayland') == true and get_option('wl-x11') == false and get_option('xwayland')
	config_h.set('HAVE_WAYLAND_ONLY', '1')
else
	dep_ecore_x = dependency('ecore-x')
endif

dep_xkeyboard_config = dependency('xkeyboard-config', required: false)
if dep_xkeyboard_config.found() == true
	config_h.set_quoted('XKB_BASE', dep_xkeyboard_config.get_pkgconfig_variable('xkb_base'))
endif

eet = find_program('eet')
gen_eet = generator(eet,
	output: '@BASENAME@.cfg',
	arguments: ['-e', '@OUTPUT@', 'config', '@INPUT@', '1'])

eldbus_codegen = find_program('eldbus-codegen')

requires_e = []

dir_pkgconfig = join_paths(dir_lib, 'pkgconfig')
if freebsd == true
	dir_pkgconfig = join_paths(dir_lib, 'data/pkgconfig')
endif

subdir('src/bin')

module_files = []
module_ldflags = '-module -avoid-version'
module_includes = ['../../..', '../../bin', '../../bin/efx']
module_includes2 = [ '../..', '../bin', '../bin/efx' ]
module_deps = [deps_e, dep_dl]

subdir('src/modules')
#### the below now use the gneric build above
#subdir('src/modules/ibar')
#subdir('src/modules/clock')
#subdir('src/modules/pager')
#subdir('src/modules/pager_plain')
subdir('src/modules/battery')
subdir('src/modules/temperature')
#subdir('src/modules/notification')
subdir('src/modules/cpufreq')
#subdir('src/modules/ibox')
#subdir('src/modules/start')
#subdir('src/modules/winlist')
#subdir('src/modules/fileman')
#subdir('src/modules/fileman_opinfo')
subdir('src/modules/wizard')
#subdir('src/modules/conf')
subdir('src/modules/conf_theme')
subdir('src/modules/conf_intl')
#subdir('src/modules/msgbus')
#subdir('src/modules/conf_applications')
subdir('src/modules/conf_display')
#subdir('src/modules/conf_shelves')
subdir('src/modules/conf_bindings')
#subdir('src/modules/conf_window_remembers')
subdir('src/modules/conf_window_manipulation')
#subdir('src/modules/conf_menus')
#subdir('src/modules/conf_dialogs')
#subdir('src/modules/conf_performance')
#subdir('src/modules/conf_paths')
#subdir('src/modules/conf_interaction')
#subdir('src/modules/gadman')
subdir('src/modules/mixer')
#subdir('src/modules/connman')
#subdir('src/modules/bluez4')
#subdir('src/modules/syscon')
subdir('src/modules/everything')
#subdir('src/modules/systray')
#subdir('src/modules/appmenu')
#subdir('src/modules/quickaccess')
subdir('src/modules/teamwork')
subdir('src/modules/lokker')
#subdir('src/modules/shot')
#subdir('src/modules/backlight')
#subdir('src/modules/tasks')
#subdir('src/modules/conf_randr')
#subdir('src/modules/xkbswitch')
#subdir('src/modules/tiling')
subdir('src/modules/music-control')
#subdir('src/modules/packagekit')
subdir('src/modules/wl_desktop_shell')
subdir('src/modules/wl_x11')
subdir('src/modules/wl_wl')
subdir('src/modules/wl_buffer')
subdir('src/modules/wl_drm')
subdir('src/modules/wl_text_input')
subdir('src/modules/wl_weekeyboard')
subdir('src/modules/geolocation')
subdir('src/modules/xwayland')
#subdir('src/modules/wireless')
#subdir('src/modules/time')
subdir('src/modules/sysinfo')
#subdir('src/modules/luncher')

# disabled for ow
####subdir('src/modules/wl_fb')

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
subdir('data/xsession')

subdir('doc')

meson.add_install_script('meson/meson_inst.sh', suid_exes)
meson.add_install_script('meson/meson_modules.sh', module_files)
configure_file(output: 'config.h', install: false, configuration: config_h)
pkgconfig.generate(
	name: proj,
	description: 'Enlightenment Window Manager',
	filebase: proj,
	subdirs: proj,
	#requires: deps_e,
	requires: requires_e,
	version: e_version_rev,
	libraries_private: '-lm',
	install_dir: dir_pkgconfig,
	variables: [
		'exec_prefix=${prefix}',
		'datarootdir=@0@'.format(dir_data),
		'datadir=${datarootdir}',
		'modules=@0@/enlightenment/modules'.format(dir_lib),
		'pkgdatadir=${datarootdir}/'+ proj + '/data',
		'themes=${pkgdatadir}/themes',
		'backgrounds=${pkgdatadir}/backgrounds',
		'release=@0@'.format(release)
	]
)

install_data(['AUTHORS', 'COPYING'], install_dir: join_paths(dir_data, 'enlightenment'))
