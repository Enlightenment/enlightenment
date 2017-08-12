if get_option('wayland') != true
  disable = true
else
  src = files(
    'e_mod_main.c'
  )

  no_icon = true

  config_h.set('HAVE_WL_DRM', '1')
endif
