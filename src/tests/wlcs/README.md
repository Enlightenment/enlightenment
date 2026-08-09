# Running the wlcs conformance suite

`e_wlcs.so` is the integration; `wlcs` is the runner that loads it. The
integration starts the Enlightenment of the build tree it was built in, staged
by `src/tests/e-test-compositor`, so build first and nothing needs installing:

```sh
ninja -C build
$(pkg-config --variable=test_runner wlcs) ./build/src/tests/wlcs/e_wlcs.so
```

The runner is not on PATH and its location differs between distributions -
/usr/libexec/wlcs/wlcs on some, /usr/lib/<multiarch>/wlcs/wlcs on Debian and
Ubuntu - which is why it is asked of pkg-config.

`E_WLCS_VERBOSE=1` lets the compositor's own output through instead of sending
it to /dev/null, and `E_WLCS_KEEP=1` keeps each compositor's runtime directory.

Useful runner flags, all from gtest: `--gtest_list_tests`,
`--gtest_filter=SubsurfaceTest.*`, `--gtest_output=xml:results.xml`.

`meson test wlcs-integration` does **not** run the suite. It runs
`e_wlcs_driver`, which exercises the integration's own hooks and checks the
declared descriptor against the compositor's real registry. That is a check on
*our shim*, and it only needs the wlcs headers, not a working runner.

## Getting a runner that works

Debian and Ubuntu package it as `wlcs`, with the gtest runner linked in.
Other distribution packages may not give you a usable runner: wlcs 1.1.0
packages built against gtest 1.10 cannot start once gtest has been upgraded.
Check with `ldd $(pkg-config --variable=test_runner wlcs)`.

Building it is not hard, but three things get in the way and none of them are
obvious from the error:

```sh
git clone https://github.com/canonical/wlcs && cd wlcs
git checkout v1.7.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DWLCS_BUILD_ASAN=False -DWLCS_BUILD_TSAN=False -DWLCS_BUILD_UBSAN=False \
      -DBoost_INCLUDE_DIR=/path/to/boost/include \
      -DCMAKE_CXX_FLAGS="-Wno-error=restrict -Wno-error=array-bounds"
cmake --build build -j
```

* **v1.7.0, not HEAD.** wlcs 1.8.2 uses `std::format`, which needs GCC 13.
  v1.7.0 builds with GCC 12 and its `wlcs/*.h` are byte-identical to 1.1.0's -
  same `WLCS_DISPLAY_SERVER_VERSION` (3) and `WLCS_SERVER_INTEGRATION_VERSION`
  (1) - so the integration needs no change for either.
* **Boost headers.** wlcs needs only `boost/throw_exception.hpp` and
  `boost/exception/info.hpp`, both header-only. Some distributions, Arch among
  them, ship these in `boost` rather than `boost-libs`.
* **`-Wno-error=restrict`.** GCC 12 emits a false positive inside
  libstdc++'s own `char_traits.h`, and wlcs builds with `-Werror`.

## Keeping the declared extension list honest

`_extensions[]` in `e_wlcs.c` is what the integration tells wlcs it supports.

The wlcs header implies this skips whole groups of tests. It does not - wlcs
1.7.0's `bind_if_supported()` carries a TODO proposing exactly that,
unimplemented. What the list really drives is **version negotiation** (a global
is bound at the version declared here) and **shell selection**
(`the_wl_shell()` / `the_xdg_shell_v6()`). Everything else runs whatever you
declare.

`e_wlcs_driver` fails when the list claims more than E has: an interface E
does not advertise, or a version above the one it advertises. It does not
catch the opposite, an entry declared lower than E speaks, which makes tests
quietly exercise an older protocol. Raise an entry when E raises the version
it advertises; `globals.expected` changes in the same commit and is the
reminder.

## The attach-before-configure relaxation

The integration sets `E_TEST_ALLOW_UNCONFIGURED_BUFFER=1` in the compositor
it launches; set `E_WLCS_STRICT=1` to turn that off.

wlcs builds every test surface without the initial empty commit xdg-shell
requires: `XdgStableSurfaceBuilder::build()` in
[`src/surface_builder.cpp`](https://github.com/canonical/wlcs/blob/main/src/surface_builder.cpp)
creates the `xdg_surface` and `xdg_toplevel` and attaches a buffer at once.
[xdg-shell](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/stable/xdg-shell/xdg-shell.xml)
makes that an error:

> any attempts by a client to attach or manipulate a buffer prior to the first
> `xdg_surface.configure` call must also be treated as errors

E rejects it, so about 315 of the 1163 tests die at surface construction
and never reach what they check. The relaxation stands in for the missing
commit: it sends the 0x0 configure that commit would have produced. It only
applies to a surface that already has a role, so
`XdgSurfaceStableTest.attaching_buffer_to_unconfigured_xdg_surface_is_an_error`
still sees its error. A client that follows the protocol never takes this
path.

It is compiled only with `-Dtests=true` (`E_TESTS`), so a release build
cannot reach it whatever the environment says, and `run-nested.sh` does not
enable it.

Remove it, and this section, once wlcs's surface builders commit before
attaching. That is still not the case in wlcs 1.8.2.
