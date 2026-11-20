#!/bin/sh
# Run an X11 client against a throwaway Enlightenment running as the window
# manager of a private Xvfb.
#
#   run-x11.sh <client> [args...]
#
# Environment:
#   E_TEST_COMPOSITOR  the build's e-test-compositor script (required; meson
#                      test sets it)
#   E_TEST_TIMEOUT     seconds to wait for E to claim the screen (default 60)
#   E_TEST_KEEP        set to 1 to keep the temp dir and print E's log path
#   E_TEST_SCREEN      geometry of each Xvfb screen (default 1024x768x24)
#   E_TEST_HEADS       number of such screens side by side (default 1)
#
# Never the caller's DISPLAY: E claims the WM selection and reparents every
# window on the screen it is given, so it would fight the developer's own
# window manager for it.

set -eu

if [ $# -lt 1 ]; then
    echo "usage: $0 <client> [args...]" >&2
    exit 2
fi

COMPOSITOR=${E_TEST_COMPOSITOR:-}
if [ -z "$COMPOSITOR" ] || [ ! -f "$COMPOSITOR" ]; then
    echo "run-x11.sh: E_TEST_COMPOSITOR is unset or missing: '$COMPOSITOR'" >&2
    echo "run-x11.sh: it is <builddir>/src/tests/e-test-compositor" >&2
    exit 2
fi

if ! command -v Xvfb >/dev/null 2>&1; then
    echo "run-x11.sh: no Xvfb; install xorg-server-xvfb" >&2
    exit 77
fi

TIMEOUT=${E_TEST_TIMEOUT:-60}

RUNDIR=$(mktemp -d "${TMPDIR:-/tmp}/e-x11-test.XXXXXX")
chmod 0700 "$RUNDIR"
E_LOG="$RUNDIR/enlightenment.log"

X_PID=""
E_PID=""

cleanup() {
    status=$?
    if [ -n "$E_PID" ] && kill -0 "$E_PID" 2>/dev/null; then
        kill -TERM "$E_PID" 2>/dev/null || :
        i=0
        while [ $i -lt 30 ] && kill -0 "$E_PID" 2>/dev/null; do
            i=$((i + 1))
            sleep 0.1
        done
        kill -KILL "$E_PID" 2>/dev/null || :
        wait "$E_PID" 2>/dev/null || :
    fi
    [ -n "$X_PID" ] && { kill "$X_PID" 2>/dev/null || :; }
    # E and the client start helpers of their own, efreet's cache builders
    # among them, which outlive them and would write into the temp dir
    # after it is gone, putting it back.
    for p in /proc/[0-9]*; do
        if tr '\0' '\n' <"$p/environ" 2>/dev/null | grep -qx "HOME=$RUNDIR"; then
            kill "${p#/proc/}" 2>/dev/null || :
        fi
    done
    if [ "${E_TEST_KEEP:-0}" = "1" ]; then
        echo "run-x11.sh: kept $RUNDIR (compositor log: $E_LOG)" >&2
    else
        # rm can race E's helpers; under set -e losing that race would exit
        # before the client's status is reported.
        i=0
        while [ $i -lt 20 ]; do
            rm -rf "$RUNDIR" 2>/dev/null || :
            [ -d "$RUNDIR" ] || break
            i=$((i + 1))
            sleep 0.1
        done
    fi
    exit $status
}
trap cleanup EXIT INT TERM

# Heads are E's fake Xinerama screens side by side on one wide Xvfb screen:
# Xvfb does not join several -screen into one root window.
SCREEN=${E_TEST_SCREEN:-1024x768x24}
HEADS=${E_TEST_HEADS:-1}
HEAD_W=${SCREEN%%x*}
HEAD_H=${SCREEN#*x}
DEPTH=${HEAD_H#*x}
HEAD_H=${HEAD_H%%x*}
E_ARGS=""
if [ "$HEADS" -gt 1 ]; then
    i=0
    while [ $i -lt "$HEADS" ]; do
        E_ARGS="$E_ARGS -fake-xinerama-screen ${HEAD_W}x${HEAD_H}+$((i * HEAD_W))+0"
        i=$((i + 1))
    done
fi
# Xvfb picks a free display itself and writes its number to -displayfd
# once it accepts connections: tests run in parallel, and a number chosen
# here could be one another run's Xvfb already holds.
Xvfb -displayfd 3 -screen 0 "$((HEADS * HEAD_W))x${HEAD_H}x${DEPTH}" \
    3>"$RUNDIR/display" >"$RUNDIR/xvfb.log" 2>&1 &
X_PID=$!

i=0
while [ $i -lt 200 ]; do
    [ -s "$RUNDIR/display" ] && break
    if ! kill -0 "$X_PID" 2>/dev/null; then
        break
    fi
    i=$((i + 1))
    sleep 0.05
done
if [ ! -s "$RUNDIR/display" ]; then
    echo "run-x11.sh: Xvfb never came up" >&2
    cat "$RUNDIR/xvfb.log" >&2 || :
    exit 1
fi
X_DISPLAY=":$(cat "$RUNDIR/display")"

DISPLAY="$X_DISPLAY" E_TEST_BACKEND=xwm E_TEST_ARGS="$E_ARGS" \
    sh "$COMPOSITOR" "$RUNDIR" >"$E_LOG" 2>&1 &
E_PID=$!

# E can manage a client once it has set _NET_SUPPORTING_WM_CHECK, not when
# its process starts.
elapsed=0
ready=0
while [ "$elapsed" -lt "$TIMEOUT" ]; do
    if ! kill -0 "$E_PID" 2>/dev/null; then
        echo "run-x11.sh: E exited before claiming the screen" >&2
        cat "$E_LOG" >&2 || :
        exit 1
    fi
    if DISPLAY="$X_DISPLAY" xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null \
        | grep -q "window id"; then
        ready=1
        break
    fi
    elapsed=$((elapsed + 1))
    sleep 1
done

if [ "$ready" != "1" ]; then
    echo "run-x11.sh: E did not claim the screen within ${TIMEOUT}s" >&2
    cat "$E_LOG" >&2 || :
    exit 1
fi

set +e
DISPLAY="$X_DISPLAY" XDG_RUNTIME_DIR="$RUNDIR" HOME="$RUNDIR" "$@"
client_status=$?
set -e

if [ $client_status -ne 0 ]; then
    echo "run-x11.sh: client '$1' exited $client_status" >&2
    echo "--- compositor log ---" >&2
    cat "$E_LOG" >&2 || :
fi

exit $client_status
