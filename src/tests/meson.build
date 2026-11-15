# Test suites, built only with -Dtests=true.
#
# The .c files sitting directly in this directory are older standalone X11
# clients driven by hand; they are deliberately left out of the build.

test_conf = configuration_data()
test_conf.set('BUILD_ROOT', meson.project_build_root())
test_conf.set('PREFIX', dir_prefix)
test_conf.set('BIN_DIR', dir_bin)
test_conf.set('LIB_DIR', dir_lib)
test_conf.set('DATA_DIR', join_paths(dir_data, proj))
test_conf.set('LOCALE_DIR', join_paths(dir_prefix, 'share/locale'))

configure_file(input        : 'e-test-compositor.sh.in',
               output       : 'e-test-compositor',
               configuration: test_conf)
e_test_compositor = join_paths(meson.current_build_dir(), 'e-test-compositor')

if get_option('wl') == true
  subdir('wayland')
  subdir('wlcs')
endif
