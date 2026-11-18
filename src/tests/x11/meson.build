# X11 window manager tests.
#
# Each test is an X11 client run by run-x11.sh against the Enlightenment of
# this build tree, managing a private Xvfb. E's X11 and Wayland compositors
# share e_client.c, e_actions.c, e_comp_object.c and most of src/bin: these
# tests ask the X11 side what the Wayland tests ask the Wayland one.
#
# Xvfb is not something a build machine has to have: run-x11.sh exits 77
# without it, and meson reports a skip.

dep_x11 = dependency('x11', required: false)
dep_xtst = dependency('xtst', required: false)

if dep_x11.found() and dep_xtst.found()
  x11_test_env = environment()
  x11_test_env.set('E_TEST_COMPOSITOR', e_test_compositor)

  test_csd_move = executable('test_x11_csd_move', 'test_csd_move.c',
                             dependencies: [dep_x11, dep_xtst])

  # Two heads as well: a window on the right-hand screen is far from 0,0,
  # which is where a drag that lost its starting point puts it.
  foreach heads: ['1', '2']
    env = x11_test_env
    env.set('E_TEST_HEADS', heads)
    test(heads == '1' ? 'x11-csd-move' : 'x11-csd-move-2heads',
      files('run-x11.sh'),
      args: [test_csd_move],
      env: env,
      suite: 'x11',
      timeout: 120,
    )
  endforeach
endif
