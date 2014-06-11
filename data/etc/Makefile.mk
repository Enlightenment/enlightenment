etcfilesdir = $(sysconfdir)/enlightenment
etcfiles_DATA =

if INSTALL_SYSACTIONS
etcfiles_DATA += data/etc/sysactions.conf
endif

etcmenusdir = $(sysconfdir)/xdg/menus
etcmenus_DATA =

if INSTALL_ENLIGHTENMENT_MENU
etcmenus_DATA += data/etc/e-applications.menu
endif

EXTRA_DIST += $(etcfiles_DATA) $(etcmenus_DATA)
