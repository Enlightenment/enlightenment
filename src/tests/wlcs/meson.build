dep_wlcs = dependency('wlcs', required: false)

if dep_wlcs.found()
  wlcs_proto_src = []
  foreach p: ['../../protocol/wl-test.xml',
              '@0@/stable/xdg-shell/xdg-shell.xml'.format(dir_wayland_protocols)]
    wlcs_proto_src += gen_scanner_client.process(p)
    wlcs_proto_src += gen_scanner_impl.process(p)
  endforeach

  # The integration the wlcs runner dlopen's. name_prefix is cleared so the
  # file is e_wlcs.so, which is what gets passed to the runner. It starts the
  # Enlightenment of this build tree, never an installed one.
  e_wlcs = shared_module('e_wlcs',
    ['e_wlcs.c', wlcs_proto_src],
    name_prefix : '',
    dependencies: [dep_wlcs, dependency('wayland-client')],
    c_args      : ['-DE_WLCS_COMPOSITOR="@0@"'.format(e_test_compositor)],
  )

  # Drives the integration's own hooks, without the wlcs runner. It isolates
  # "is our shim correct" from "is the suite happy", and keeps working when
  # no usable runner is installed.
  wlcs_driver = executable('e_wlcs_driver',
    ['wlcs_driver.c', wlcs_proto_src],
    dependencies: [dep_wlcs, dependency('wayland-client'), cc.find_library('dl')],
  )

  wlcs_env = environment()
  wlcs_env.set('E_WLCS_INTEGRATION', e_wlcs.full_path())

  test('wlcs-integration',
    wlcs_driver,
    env    : wlcs_env,
    depends: e_wlcs,
    timeout: 180,
  )
else
  message('wlcs not found; the conformance integration will not be built')
endif
