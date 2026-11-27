# Wayland compositor tests.
#
# Every test here is an out-of-process Wayland *client* driven against a
# throwaway Enlightenment started by run-nested.sh. E is a single large
# main() rather than a library, so it cannot be constructed per test case the
# way Weston's or KWin's suites construct theirs; the compositor is a
# subprocess and the tests talk to it over the wire.
#
# The compositor is the one in this build tree, staged by e-test-compositor;
# nothing installed on the system is used.

run_nested = files('run-nested.sh')

wl_test_env = environment()
wl_test_env.set('E_TEST_COMPOSITOR', e_test_compositor)

e_test_globals = executable('e_test_globals',
  'e_test_globals.c',
  dependencies: [dependency('wayland-client')],
)

# Client-side bindings for the private test protocol and for both xdg-shell
# versions E speaks.
test_proto_src = []
foreach p: [
  '../../protocol/wl-test.xml',
  '@0@/stable/xdg-shell/xdg-shell.xml'.format(dir_wayland_protocols),
  '@0@/unstable/xdg-shell/xdg-shell-unstable-v6.xml'.format(dir_wayland_protocols),
  '@0@/unstable/xdg-foreign/xdg-foreign-unstable-v1.xml'.format(dir_wayland_protocols),
  '@0@/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml'.format(dir_wayland_protocols),
  '@0@/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml'.format(dir_wayland_protocols),
  '@0@/unstable/xdg-output/xdg-output-unstable-v1.xml'.format(dir_wayland_protocols),
  '@0@/staging/xdg-activation/xdg-activation-v1.xml'.format(dir_wayland_protocols),
  '@0@/stable/presentation-time/presentation-time.xml'.format(dir_wayland_protocols),
  '@0@/staging/xdg-toplevel-drag/xdg-toplevel-drag-v1.xml'.format(dir_wayland_protocols),
  '@0@/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml'.format(dir_wayland_protocols),
  '@0@/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'.format(dir_wayland_protocols),
  '@0@/unstable/primary-selection/primary-selection-unstable-v1.xml'.format(dir_wayland_protocols),
  '@0@/unstable/text-input/text-input-unstable-v3.xml'.format(dir_wayland_protocols),
  '../../protocol/input-method-unstable-v2.xml',
  '../../protocol/virtual-keyboard-unstable-v1.xml',
]
  test_proto_src += gen_scanner_client.process(p)
  test_proto_src += gen_scanner_impl.process(p)
endforeach

# The golden-globals test. Cheap, and the one test every protocol branch has
# to touch: it makes E's advertised protocol surface a reviewable file.
test('wl-globals',
  find_program('check-globals.sh'),
  args: [run_nested, e_test_globals, files('globals.expected')],
  env: wl_test_env,
  timeout: 120,
)

# Protocol tests. Each is a plain wayland-client program that exits non-zero
# with an explanatory message; run-nested.sh supplies the compositor. A
# third element adds to the environment the compositor runs in.
wl_protocol_tests = [
  ['test-module', 'test_wl_test.c'],
  ['xdg-defunct-surfaces', 'test_xdg_defunct.c'],
  ['client-list', 'test_client_list.c'],
  ['client-action', 'test_client_action.c'],
  ['no-dialogs', 'test_no_dialogs.c'],
  ['xdg-foreign', 'test_xdg_foreign.c'],
  ['xdg-foreign-v2', 'test_xdg_foreign_v2.c'],
  ['presentation-time', 'test_presentation_time.c'],
  ['shortcuts-inhibit', 'test_shortcuts_inhibit.c'],
  ['toplevel-drag', 'test_toplevel_drag.c'],
  ['seat-capabilities', 'test_seat_capabilities.c'],
  ['dialog-focus', 'test_dialog_focus.c'],
  ['buffer-release', 'test_buffer_release.c'],
  ['client-move', 'test_client_move.c'],
  ['pointer-frame', 'test_pointer_frame.c'],
  ['surface-unmap', 'test_surface_unmap.c'],
  ['pointer-focus', 'test_pointer_focus.c'],
  ['subsurface-position', 'test_subsurface_position.c'],
  ['subsurface-input', 'test_subsurface_input.c'],
  ['window-geometry', 'test_window_geometry.c'],
  ['configure-size', 'test_configure_size.c'],
  ['touch-moving-surface', 'test_touch_moving_surface.c'],
  ['pointer-axis120', 'test_pointer_axis120.c'],
  ['popup-dismiss', 'test_popup_dismiss.c'],
  ['xdg-tiled', 'test_xdg_tiled.c'],
  ['popup-reposition', 'test_popup_reposition.c'],
  ['xdg-toplevel-v6', 'test_xdg_toplevel_v6.c'],
  ['maximize-restore', 'test_maximize_restore.c'],
  ['xdg-decoration', 'test_xdg_decoration.c'],
  ['shm-truncate', 'test_shm_truncate.c'],
  ['output', 'test_output.c'],
  ['activation', 'test_activation.c'],
  ['activation-expiry', 'test_activation.c',
   {'E_TEST_ACTIVATION_TOKEN_LIFETIME': '1'}],
  ['primary-selection', 'test_primary_selection.c'],
  ['idle-inhibit', 'test_idle_inhibit.c'],
  ['text-input', 'test_text_input.c'],
  ['input-method-keyboard', 'test_input_method_keyboard.c'],
  ['input-popup', 'test_input_popup.c'],
  ['privileges', 'test_privileges.c',
   {'PATH': meson.current_build_dir()}],
  ['client-iconify', 'test_client_iconify.c',
   {'ELM_TRANSITION_DURATION_FACTOR': '4'}],
]

tk_src = files('e_wl_testkit.c')

foreach t: wl_protocol_tests
  exe = executable('test_wl_' + t[0].underscorify(),
    [t[1], tk_src, test_proto_src],
    dependencies: [dependency('wayland-client')],
  )
  env = wl_test_env
  if t.length() > 2
    env = environment({'E_TEST_COMPOSITOR': e_test_compositor})
    foreach k, v: t[2]
      if k == 'PATH'
        env.prepend(k, v)
      else
        env.set(k, v)
      endif
    endforeach
  endif
  test('wl-' + t[0],
    find_program('run-nested.sh'),
    args: [exe],
    env: env,
    timeout: 120,
  )
endforeach

# X clients of the compositor's own Xwayland, so the xwayland module is
# loaded as well, and the client speaks X through ecore_x. One program, run
# once per scenario.
if get_option('xwayland') and not config_h.has('HAVE_WAYLAND_ONLY')
  test_xwayland = executable('test_wl_xwayland',
    ['test_xwayland.c', tk_src, test_proto_src],
    dependencies: [dependency('wayland-client'), dep_ecore_x],
  )
  foreach scenario: ['close', 'restore', 'early-map']
    test('wl-xwayland-' + scenario,
      run_nested,
      args: [test_xwayland, scenario],
      env: environment({'E_TEST_COMPOSITOR': e_test_compositor,
                        'E_TEST_MODULE': 'wl_test,xwayland'}),
      timeout: 120,
    )
  endforeach
endif

# Real browsers, when installed: slow, and depending on programs this tree
# does not ship, so they have a suite of their own.
browser_run = meson.current_source_dir() / 'browser-run.sh'
test_browser = executable('test_wl_browser',
  ['test_browser.c', tk_src, test_proto_src],
  dependencies: [dependency('wayland-client')],
)
foreach b: ['firefox', 'chromium', 'brave']
  test('browser-' + b,
    run_nested,
    args: [test_browser, b, 'e-browser-probe'],
    env: {
      'E_TEST_COMPOSITOR': e_test_compositor,
      'E_TEST_APP': ' '.join([browser_run, b,
                              'file://' + meson.current_source_dir() / 'pages' / 'probe.html']),
      'E_TEST_APP_CHECK': ' '.join([browser_run, '--find', b]),
      'E_TEST_WAIT_SECONDS': '60',
    },
    suite: 'browser',
    timeout: 300,
  )
endforeach
