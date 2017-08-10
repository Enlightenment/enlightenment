src = files(
  'e_mod_main.c'
 )

no_icon = true

if get_option('wayland') != true
  disable = true
endif

config_h.set('HAVE_WL_DRM', '1')
