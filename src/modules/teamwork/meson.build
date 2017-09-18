deps = [dependency('emile')]

src = files(
  'e_mod_main.c',
  'e_mod_config.c',
  'e_mod_tw.c',
  'wl.c',
  'x11.c',
  'e_mod_main.h'
 )

if config_h.has('HAVE_WAYLAND') == true
  src += files(
    'wl_teamwork.c',
    'wl_teamwork.h'
  )
endif
