if get_option('wayland') != true or dep_ecore_drm2.found() != true
  disable = true
else
  deps = [dependency('elput')]

  src = files(
    'e_mod_main.c'
  )

  no_icon = true

  config_h.set('HAVE_WL_DRM', '1')
endif
