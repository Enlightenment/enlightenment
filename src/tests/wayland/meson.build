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

# The golden-globals test. Cheap, and the one test every protocol branch has
# to touch: it makes E's advertised protocol surface a reviewable file.
test('wl-globals',
  find_program('check-globals.sh'),
  args: [run_nested, e_test_globals, files('globals.expected')],
  env: wl_test_env,
  timeout: 120,
)
