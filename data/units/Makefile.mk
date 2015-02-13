if HAVE_SYSTEMD_USER_SESSION
unitsdir = $(DESTDIR)$(USER_SESSION_DIR)
units_DATA = data/units/enlightenment.service
endif

EXTRA_DIST += $(units_DATA)
