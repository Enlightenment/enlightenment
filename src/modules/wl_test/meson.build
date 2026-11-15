# Test-only module. Never built unless -Dtests=true, so a release build
# cannot ship the back door even by accident.
if get_option('wl') != true or get_option('tests') != true
  disable = true
else
  src = files(
    'e_mod_main.c'
  )

  src += gen_scanner_server.process('../../protocol/wl-test.xml')
  src += gen_scanner_impl.process('../../protocol/wl-test.xml')

  no_icon = true
endif
