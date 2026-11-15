#!/bin/sh
# Run a Wayland client against a throwaway Enlightenment compositor.
#
#   run-nested.sh <client> [args...]
#
# Environment:
#   E_TEST_COMPOSITOR  the build's e-test-compositor script (required; meson
#                      test sets it)
#   E_TEST_BACKEND   buffer (default) | x11 | wl        -- see E_WL_FORCE
#   E_TEST_TIMEOUT   seconds to wait for E's socket (default 20)
#   E_TEST_KEEP      set to 1 to keep the temp dir and print E's log path
#   WAYLAND_DEBUG    passed through to the client
#
# Exits with the client's exit status, or 1 if the compositor never came up.
#
# The compositor gets a private XDG_RUNTIME_DIR, so it owns the only socket
# in it. E picks its socket name itself and never exports WAYLAND_DISPLAY, so
# the name has to be discovered, and a directory of its own turns that into an
# unambiguous glob.

set -eu

if [ $# -lt 1 ]; then
    echo "usage: $0 <client> [args...]" >&2
    exit 2
fi

COMPOSITOR=${E_TEST_COMPOSITOR:-}
if [ -z "$COMPOSITOR" ] || [ ! -f "$COMPOSITOR" ]; then
    echo "run-nested.sh: E_TEST_COMPOSITOR is unset or missing: '$COMPOSITOR'" >&2
    echo "run-nested.sh: it is <builddir>/src/tests/e-test-compositor" >&2
    exit 2
fi
TIMEOUT=${E_TEST_TIMEOUT:-20}

RUNDIR=$(mktemp -d "${TMPDIR:-/tmp}/e-wl-test.XXXXXX")
chmod 0700 "$RUNDIR"

E_PID=""
E_LOG="$RUNDIR/enlightenment.log"

# Every process still living in our private runtime dir, found by its HOME.
#
# E's helpers (efreetd and friends) detach into a session of their own, so
# they survive both the compositor and its process group. They do exit on
# their own, but slowly, and on the way out they recreate the cache tree we
# are trying to delete. The path is a fresh mktemp -d, so matching HOME
# against it cannot hit anything else.
#
# Prints the count; kills with $1 first if given.
procs_in_rundir() {
    n=0
    for env in /proc/[0-9]*/environ; do
        grep -qxzF "HOME=$RUNDIR" "$env" 2>/dev/null || continue
        pid=${env#/proc/}
        pid=${pid%/environ}
        if [ -n "${1:-}" ]; then
            kill "-$1" "$pid" 2>/dev/null || :
        fi
        n=$((n + 1))
    done
    echo "$n"
}

cleanup() {
    status=$?
    if [ -n "$E_PID" ] && kill -0 "$E_PID" 2>/dev/null; then
        kill -TERM "-$E_PID" 2>/dev/null || :
        i=0
        while [ $i -lt 20 ] && kill -0 "$E_PID" 2>/dev/null; do
            i=$((i + 1))
            sleep 0.1
        done
        kill -KILL "-$E_PID" 2>/dev/null || :
        wait "$E_PID" 2>/dev/null || :
    fi

    # Wait for the stragglers before deleting anything, or they put it back.
    procs_in_rundir TERM >/dev/null
    i=0
    while [ $i -lt 50 ] && [ "$(procs_in_rundir)" != "0" ]; do
        i=$((i + 1))
        sleep 0.1
    done
    procs_in_rundir KILL >/dev/null

    if [ "${E_TEST_KEEP:-0}" = "1" ]; then
        echo "run-nested.sh: kept $RUNDIR (compositor log: $E_LOG)" >&2
    else
        # Trust that the directory is gone, not rm's exit status: a helper
        # winding down can recreate part of it right after a successful rm.
        # Never let the tidy-up decide the exit status either.
        i=0
        while [ $i -lt 20 ] && [ -d "$RUNDIR" ]; do
            rm -rf "$RUNDIR" 2>/dev/null || :
            i=$((i + 1))
            [ -d "$RUNDIR" ] && sleep 0.1
        done
        if [ -d "$RUNDIR" ]; then
            echo "run-nested.sh: could not remove $RUNDIR" >&2
        fi
    fi
    exit $status
}
trap cleanup EXIT INT TERM

# A process group of its own, so the compositor and whatever it forks go down
# together. A background job of a non-interactive shell is never a group
# leader, so setsid does not fork and $! is the compositor's pid.
setsid sh "$COMPOSITOR" "$RUNDIR" >"$E_LOG" 2>&1 &
E_PID=$!

SOCKET=""
elapsed=0
while [ "$elapsed" -lt "$((TIMEOUT * 10))" ]; do
    if ! kill -0 "$E_PID" 2>/dev/null; then
        echo "run-nested.sh: compositor exited before creating a socket" >&2
        echo "--- $E_LOG ---" >&2
        cat "$E_LOG" >&2 || :
        exit 1
    fi
    for s in "$RUNDIR"/wayland-*; do
        [ -S "$s" ] || continue
        SOCKET=$(basename "$s")
        break
    done
    [ -n "$SOCKET" ] && break
    sleep 0.1
    elapsed=$((elapsed + 1))
done

if [ -z "$SOCKET" ]; then
    echo "run-nested.sh: no wayland socket in $RUNDIR after ${TIMEOUT}s" >&2
    echo "--- $E_LOG ---" >&2
    cat "$E_LOG" >&2 || :
    exit 1
fi

set +e
XDG_RUNTIME_DIR="$RUNDIR" WAYLAND_DISPLAY="$SOCKET" "$@"
client_status=$?
set -e

if [ $client_status -ne 0 ]; then
    echo "run-nested.sh: client '$1' exited $client_status" >&2
    echo "--- compositor log ---" >&2
    cat "$E_LOG" >&2 || :
fi

exit $client_status
