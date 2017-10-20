HALT      = '/sbin/shutdown -h now'
REBOOT    = '/sbin/shutdown -r now'
SUSPEND   = ''
HIBERNATE = ''
MOUNT     = '/bin/mount'
UMOUNT    = '/bin/umount'
EJECT     = '/usr/bin/eject'

if host_machine.system().contains('bsd') == true
  #SUSPEND = 'acpiconf -s3'
  SUSPEND   = '/usr/sbin/zzz'
  HIBERNATE = 'acpiconf -s4'
  HALT      = '/sbin/shutdown -p now'
  MOUNT     = '/sbin/mount'
  UMOUNT    = '/sbin/umount'
  EJECT     = '/usr/sbin/cdcontrol eject'
else
  if systemd_unitdir != ''
    HALT      = '/usr/bin/systemctl poweroff'
    REBOOT    = '/usr/bin/systemctl reboot'
    SUSPEND   = '/usr/bin/systemctl suspend'
    HIBERNATE = '/usr/bin/systemctl hibernate'
  else
    sleep = find_program('sleep.sh', '/etc/acpi/sleep.sh', '/sbin/sleep.sh', '/usr/sbin/sleep.sh', required : false)
    if sleep.found() == true
      SUSPEND = sleep.path()
      if sleep.path() == '/etc/acpi/sleep.sh'
        SUSPEND = '/etc/acpi/sleep.sh force'
      endif
    else
      suspend = find_program('pm-suspend', '/etc/acpi/pm-suspend', '/sbin/pm-suspend', '/usr/sbin/pm-suspend', required: false)
      if suspend.found()
        SUSPEND = suspend.path()
      endif
    endif

    hibernate_sh = find_program('hibernate.sh', '/etc/acpi/hibernate.sh', '/sbin/hibernate.sh', '/usr/sbin/hibernate.sh', required : false)
    if hibernate_sh.found() == true
      HIBERNATE = hibernate_sh.path()
      if hibernate_sh.path() == '/etc/acpi/hibernate.sh'
        SUSPEND = '/etc/acpi/hibernate.sh force'
      endif
    else
      hibernate = find_program('pm-hibernate', '/etc/acpi/pm-hibernate', '/sbin/pm-hibernate', '/usr/sbin/pm-hibernate', required: false)
      if hibernate.found()
        HIBERNATE = hibernate.path()
      endif
    endif
  endif
endif

sysactions = configuration_data()
sysactions.set('HALT'     , HALT)
sysactions.set('REBOOT'   , REBOOT)
sysactions.set('SUSPEND'  , SUSPEND)
sysactions.set('HIBERNATE', HIBERNATE)
sysactions.set('MOUNT'    , MOUNT)
sysactions.set('UMOUNT'   , UMOUNT)
sysactions.set('EJECT'    , EJECT)

if get_option('install-sysactions')
  configure_file(input        : 'sysactions.conf.in',
                 output       : 'sysactions.conf',
                 install      : true,
                 install_dir  : join_paths(dir_sysconf, 'enlightenment'),
                 configuration: sysactions
                )
endif

if get_option('install-enlightenment-menu')
  install_data('e-applications.menu',
               install_dir: join_paths(dir_sysconf, 'xdg/menus')
              )
endif

