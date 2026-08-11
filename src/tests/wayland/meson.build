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
# with an explanatory message; run-nested.sh supplies the compositor.
wl_protocol_tests = [
  ['test-module', 'test_wl_test.c'],
  ['xdg-defunct-surfaces', 'test_xdg_defunct.c'],
  ['client-list', 'test_client_list.c'],
  ['client-action', 'test_client_action.c'],
  ['no-dialogs', 'test_no_dialogs.c'],
  ['xdg-foreign', 'test_xdg_foreign.c'],
  ['seat-capabilities', 'test_seat_capabilities.c'],
  ['dialog-focus', 'test_dialog_focus.c'],
  ['buffer-release', 'test_buffer_release.c'],
  ['client-move', 'test_client_move.c'],
  ['pointer-frame', 'test_pointer_frame.c'],
  ['surface-unmap', 'test_surface_unmap.c'],
  ['pointer-focus', 'test_pointer_focus.c'],
  ['subsurface-position', 'test_subsurface_position.c'],
  ['window-geometry', 'test_window_geometry.c'],
]

tk_src = files('e_wl_testkit.c')

foreach t: wl_protocol_tests
  exe = executable('test_wl_' + t[0].underscorify(),
    [t[1], tk_src, test_proto_src],
    dependencies: [dependency('wayland-client')],
  )
  test('wl-' + t[0],
    find_program('run-nested.sh'),
    args: [exe],
    env: wl_test_env,
    timeout: 120,
  )
endforeach
