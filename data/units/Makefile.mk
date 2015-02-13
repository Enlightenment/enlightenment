if HAVE_SYSTEMD_USER_SESSION
unitsdir = $(USER_SESSION_DIR)
units_DATA = data/units/enlightenment.service
endif

EXTRA_DIST += $(units_DATA)
