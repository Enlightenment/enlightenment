# AGENTS.md

Guidance for AI coding agents, and for the people driving them, who
contribute to Enlightenment. The aim is simple: a contribution made with an
AI tool must be as easy to review as a careful human one.

The human submitting the change is accountable for every line of it. If you
cannot explain why a line is there, it does not belong in the patch.

## Project overview

Enlightenment (E) is a window manager and compositor built on the EFL. One
binary runs either as an **X11 window manager and compositor** or as a
**Wayland compositor**, and most of its code is shared between the two.
People run E every day on both. A change tested on one of them is half
tested.

The code lives in `src/`:

* `src/bin/`: the `enlightenment` binary and its helpers.
  * `e_comp_x*.c`: X11 only (ICCCM/EWMH, reparenting, X input).
  * `e_comp_wl*.c`: Wayland only (wl_surface, wl_seat, buffers).
  * **Everything else is shared** by both: `e_client.c`, `e_actions.c`,
    `e_bindings.c`, `e_comp.c`, `e_comp_object.c`, `e_comp_canvas.c`,
    `e_desk.c`, `e_zone.c`, `e_place.c`, `e_maximize.c`, `e_resist.c`,
    `e_pointer.c`, `e_hints.c`, `e_remember.c`, `e_xinerama.c`,
    `e_randr2.c`, ...
* `src/modules/<module>/`: loadable modules. `wl_*` modules (shells,
  input methods, output backends) and `xwayland` are Wayland only; most
  others (`pager`, `tiling`, `winlist`, `conf_*`, ...) run on both.
* `src/protocol/`: Wayland protocol XML, including the private `wl-test.xml`
  used by the tests.
* `src/tests/`: the test suites (see below).
* `data/config/<profile>/`: the default configuration profiles. `wltest`
  is the one the tests run with.

The X11 code is always built. The Wayland code is built with `-Dwl=true`.

## Build and test

```sh
meson setup build -Dwl=true -Dtests=true
ninja -C build
meson test -C build --print-errorlogs
```

Build with Wayland *and* the tests enabled even when your change looks
X11 only: code you did not compile is code you did not test. Look at
`meson_options.txt` for the modules your change touches.

Nothing needs installing. The tests start E through
`build/src/tests/e-test-compositor`, which stages the build tree with
`meson install --destdir` and runs it with a private `HOME`,
`XDG_RUNTIME_DIR` and the `wltest` profile. What runs is what was just
built, never the copy in `/usr`.

The suites:

| Suite | Where | What it runs |
|-------|-------|--------------|
| default | `src/tests/wayland/` | Wayland clients against E as a compositor on a buffer backend, through `run-nested.sh`. The `wl_test` module and the `wl-test.xml` protocol let a test drive E's actions and read back its state. Needs `-Dwl=true`. |
| `x11` | `src/tests/x11/` | X11 clients against E as the window manager of a private Xvfb, through `run-x11.sh`. `E_TEST_HEADS=2` gives E two heads side by side. |
| `wlcs` | `src/tests/wlcs/` | The wlcs conformance integration. See its `README.md` for running the suite itself. |

```sh
meson test -C build --suite x11           # one suite
meson test -C build x11-csd-move          # one test
E_TEST_KEEP=1 meson test -C build <test>  # keep E's log; its path is printed
```

`meson test` reports the `x11` suite as skipped, not failed, when Xvfb is
missing. A skip is not a pass: install Xvfb (`xorg-server-xvfb`, or `xvfb`
on Debian).

The harness never uses your `DISPLAY` or `WAYLAND_DISPLAY`. E claims the
window manager selection of the screen it is given and grabs keys, so never
point a test at your live session. To try a change by hand, run it nested:
`xdebug.sh` starts E in Xephyr for X11, and `E_WL_FORCE=x11` or `wl` starts
the Wayland compositor in a window.

Read ninja's own final line, not a `grep` of its log: a failed build hidden
behind a pipe leaves a stale binary that the tests will happily run.

## Tests are mandatory

Before you claim a change is done, **build it and run every suite that
covers the code it touches. Every test must pass.** Report failures as they
are. Do not hide them, skip them, or weaken a test to make it pass. If a
test already failed before your change, say so and show it fails the same
way without your change.

Every bug fix comes with a test that fails without the fix. Every new
feature comes with tests that cover it, edge cases included. Check that
the test does fail without the fix: a test that passes on both sides
protects nothing.

* A Wayland test is a plain `wayland-client` program in
  `src/tests/wayland/`, built on `e_wl_testkit.h` and listed in
  `wl_protocol_tests` in that directory's `meson.build`. When it needs
  something from E that the protocol does not give, extend `wl-test.xml`:
  append new requests at the end and bump its version.
* An X11 test is an EFL program in `src/tests/x11/`, built on
  `x11_testkit.h`: elementary windows and ecore_x, not Xlib. Do what the
  real client does: an elementary window draws its own decorations on X11
  with `EFL_WIN_FRAME_MODE=on` and sends `_NET_WM_MOVERESIZE` from a press
  on them, as a GTK header bar does. `ecore_x_test_fake_mouse_*()` drive
  the pointer.
* Test the behaviour, not the implementation: a test that has to be
  rewritten every time the code is refactored protects nothing.
* A test that waits on a timer flakes. Wait for an event or a state where
  you can, and run a new test 20 times before calling it stable.

## Both backends

Many regressions in E come from a change to shared code that was checked on
the backend its author was working on, and broke the other one.

An example: b2e61b1f2 made a key-bound window move remember where it
started, so the window would not jump to the pointer. It was tested on
Wayland, where the test ran the `window_move` action with no mouse event.
On X11, the `_NET_WM_MOVERESIZE` handler in `e_comp_x.c` also runs
`window_move` with no event, *after* recording the start of the drag
itself. The change took "no event" to mean "no start recorded" and threw
that start away, so every client-side-decorated window jumped towards the
top-left of the screen on its first drag. It was reverted in 002c93601.
`x11-csd-move` now covers it.

So:

* **A change to a shared file is tested on both backends.** Run the
  default suite and `--suite x11`. If no test covers the X11 side of what
  you changed, write one, as you would for the Wayland side.
* **Decide this per commit, not per series.** A Wayland series that
  touches `e_client.c` in one commit needs the X11 run for that commit.
* **Before you change what a shared function expects or does, find every
  caller**, and name them in the commit message. Direct calls are not all
  of them: E reaches the same code through actions run with
  `e_action->func.go(obj, NULL)` (key bindings, menus, `e_comp_x.c`'s
  EWMH handlers, `e_client.c`'s placement), through client hooks
  (`E_CLIENT_HOOK_*`) and through ecore events. A caller that prepares
  state before calling, as `_e_comp_x_move_resize_request()` does with
  `ec->moveinfo`, has to keep working.
* **Multi-head.** Positions in E are relative to the whole canvas, not to
  one screen. A bug that falls back to 0,0 can look almost right on the
  first head and be a whole screen off on the second. Test
  position-related changes with two heads.
* **Before pushing a change to window movement, resizing, focus, stacking
  or input, try it by hand**: in an X11 session with a client-side
  decorated GTK application (`gtk4-demo`, `gnome-calculator`), and in a
  Wayland session with both a client-side and a server-side decorated
  client. Xvfb catches the arithmetic, not everything a user sees.

## Performance changes need numbers

A change made for performance must prove it with before and after numbers
on the same machine and build type (`--buildtype=release`), in the commit
message, with how they were measured. E composites every frame: think about
allocations and per-client, per-frame and per-event work in the compositor,
input and render paths. Complexity that only buys a gain within the noise
will be rejected.

## API, ABI and configuration

Symbols marked `E_API` are used by modules, including third-party ones
built against the installed headers.

* Export a symbol only when a module really uses it. A helper used inside
  one file stays `static`.
* Exported names carry the `e_` prefix (types `E_`). Static functions are
  named `_e_<file>_<what>()`.
* Do not change the signature or meaning of an existing `E_API` function
  without discussing it first.
* A new field in a saved configuration structure (`E_Config`, a module's
  config) goes in its `E_CONFIG_VAL` descriptor with a default, and comes
  with an upgrade step (`CONFIG_VERSION_CHECK` in `e_config.c`, or the
  module's version) so that existing configs pick the default up. Bump
  `E_CONFIG_FILE_EPOCH` only when existing configs cannot be kept.
* A Wayland global's advertised version is a protocol contract. Raise it
  only when every request and event of that version is implemented, and
  update `src/tests/wayland/globals.expected`.

## Code that explains itself

The code is the *how*. It must be readable on its own, through the names of
its functions, types and variables and through its structure, not through
comments that paraphrase it.

### Naming

The further a name travels from its declaration, the more it has to say.

* A local used over a few lines can be short: `i`, `x`, `l`.
* A static function, or a variable that spans a long function, needs a
  descriptive name.
* Anything visible from another file must be meaningful without reading its
  implementation, and carries the `e_`/`E_` prefix.
* Types are named through a typedef, `Capitalized_With_Underscores`.

Short names follow the conventions already used everywhere in the code:

| Name           | Meaning                                                 |
|----------------|---------------------------------------------------------|
| `i`, `j`, `k`  | numeric loop counters                                   |
| `l`, `ll`      | an `Eina_List` node while walking a list                |
| `x`, `y`       | a position                                              |
| `w`, `h`       | a width and a height; never `width`, `height`           |
| `mx`, `my`     | a pointer position, in canvas coordinates               |
| `ec`           | the `E_Client *` being worked on                        |
| `zone`, `desk` | the `E_Zone *` and the `E_Desk *`                       |
| `ev`           | the event being handled                                 |

### Comments

Write comments for:

* **A function's contract**, when its name and signature are not enough:
  what it expects, what it returns, who owns what.
* **Quirks that need outside knowledge**: a protocol or ICCCM/EWMH corner
  case, a client bug being worked around (name the client and version), a
  compatibility choice. Link to the spec or bug report.
* **State shared across files**, such as `ec->moveinfo`: who fills it in,
  and who relies on it being filled in.

Do not write comments that:

* restate what the next line does
* narrate the change (`/* now uses the new helper */`, `/* fixed */`)
  -- that belongs in the commit message
* refer to internal plan or task ids, or to commit hashes that will not
  survive a rebase

If code needs a comment to be understood, first try a better name, a smaller
function or a clearer structure.

Code, comments and commit messages are **ASCII only**: `-` not an em dash,
`->` not an arrow, `x` not a multiplication sign.

### Style

Look at the surrounding code and match it exactly. The style is the EFL's:

* 3 space indentation; braces on their own line, indented by 2; the body
  then sits 5 columns in. The return type of a function definition goes on
  its own line.
* One statement per line. No block on a single line.
* Multi-line calls and prototypes: first argument right after `(`, the rest
  aligned under it. Wrap lines over about 100 columns.
* One blank line between blocks, never two in a row.
* Parenthesize mixed-precedence expressions: `if ((a == b) || (c == d))`.
* `!strcmp(a, b)`, not `strcmp(a, b) == 0`.
* Return early instead of wrapping a whole function body in an `if`.
  Declarations stay at the top of their block.

Use the EFL and E facilities rather than reinventing them: the
`EINA_LIST_*` macros, `Eina_Stringshare`, `E_OBJECT_CHECK` and
`EINA_SAFETY_ON_NULL_RETURN(_VAL)` for arguments, and the log macros of
`e_log.h` (`ERR()`, `WRN()`, `INF()`, `DBG()`), never `printf`.

### Keep the diff minimal

* Only touch what your change needs. No drive-by reformatting, reordering
  or renaming of existing code. Mention it instead; a cleanup is its own
  commit.
* Do not add dead code: no unreachable fallbacks, no guards for values that
  are always set. If a case cannot happen, do not handle it silently; if it
  can, handle it visibly.

## Commits

One commit is one logical change. It builds and passes the tests on its own
so that `git bisect` keeps working. A test may come in its own commit.

### Subject

`<file or module> - <what changed>`, lowercase, no trailing period, around
70 characters at most, following the recent log of the area you touch
(`git log --oneline -- <path>`):

```
e_comp_wl - keep wl_pointer focus right when the window changes
wl_desktop_shell - a new toplevel ends any open menu
tests - drag a client-side-decorated window on X11
```

### Body: the *why*, not the *how*

The diff already shows how. The body explains what the reader cannot get
from it:

* why the change is needed: the bug, its symptom, who hits it, on which
  backend
* why this approach over the obvious alternatives
* the spec or bug report that decides the behaviour, with a link
* for a change to shared code: its callers, and how each backend was tested
* anything surprising a reviewer should know: a behaviour change, a corner
  case deliberately left out, before and after numbers

Do not list the functions you edited, walk through the diff, or paste test
output. Wrap at 72 columns.

A revert says what it broke and how to see it. A change that brings
reverted work back names the revert, says what is different this time, and
which test shows it.

### AI disclosure

Commits written with the help of an AI tool say so with a trailer naming the
tool, for example:

```
Co-Authored-By: <Tool and model name> <noreply@example.com>
```

## Review

* Keep a change focused on one topic. New dependencies, new `E_API` and
  design changes are discussed with the maintainers first.
* In the description, explain the problem and how the change was tested:
  which suites, which backends, and what was tried by hand.
* Answer review comments yourself, in your own words. Reviewers are
  discussing with you, not with your tool.
* Apply a review comment to the code your change added, not to the code
  around it that was already there, unless the reviewer asks for that.

## Do not

* submit code you have not built and tested
* claim tests pass without running them, or count a skipped suite as a pass
* change shared code with only one backend tested
* run E or its tests against your live `DISPLAY` or Wayland session
* break `E_API`, or users' saved configuration, without discussing it first
* add dependencies without discussing it first
* commit build artifacts, logs or screenshots
