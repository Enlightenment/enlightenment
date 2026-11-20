# X11 window manager tests.
#
# Each test is an X11 client run by run-x11.sh against the Enlightenment of
# this build tree, managing a private Xvfb. E's X11 and Wayland compositors
# share e_client.c, e_actions.c, e_comp_object.c and most of src/bin: these
# tests ask the X11 side what the Wayland tests ask the Wayland one.
#
# Xvfb is not something a build machine has to have: run-x11.sh exits 77
# without it, and meson reports a skip.

# The windows under test are elementary ones, drawing their own decorations
# on X11, and ecore_x drives the pointer. Not in a Wayland-only build.
if not config_h.has('HAVE_WAYLAND_ONLY')
  x11_test_env = environment()
  x11_test_env.set('E_TEST_COMPOSITOR', e_test_compositor)

  x11_test_deps = [dep_elementary, dep_ecore_x]
  x11_tests = [
    ['x11-csd-move', executable('test_x11_csd_move', 'test_csd_move.c',
                                dependencies: x11_test_deps)],
    ['x11-csd-resize', executable('test_x11_csd_resize', 'test_csd_resize.c',
                                  dependencies: x11_test_deps)],
  ]

  # Two heads as well: a window on the right-hand screen is far from 0,0,
  # which is where a drag that lost its starting point puts it.
  foreach t: x11_tests
    foreach heads: ['1', '2']
      env = x11_test_env
      env.set('E_TEST_HEADS', heads)
      test(heads == '1' ? t[0] : t[0] + '-2heads',
        files('run-x11.sh'),
        args: [t[1]],
        env: env,
        suite: 'x11',
        timeout: 120,
      )
    endforeach
  endforeach
endif
