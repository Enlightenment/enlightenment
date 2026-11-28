#!/bin/sh
# Run fcitx5 as run-nested.sh's E_TEST_APP, cut off from the user's own
# session: a configuration of its own, a private D-Bus, as fcitx5 will not
# start twice on one bus, and no network.
#
#   fcitx5-run.sh [engine]
#   fcitx5-run.sh --find [engine]   fails when not installed
#
# With no engine, fcitx5 uses its keyboard layout engine, which hands back
# every key it is given. An engine, such as pinyin, comes first in the
# profile: fcitx5 starts an input context on the first input method of the
# group, and only goes to the default one when the user switches to it.

set -eu

if [ "${1:-}" = "--find" ]; then
    BIN=$(command -v fcitx5)
    [ -z "${2:-}" ] && exit
    test -e "${BIN%/bin/fcitx5}/share/fcitx5/inputmethod/$2.conf"
    exit
fi

export XDG_CONFIG_HOME="$E_TEST_RUNDIR/fcitx5-config"
export XDG_DATA_HOME="$E_TEST_RUNDIR/fcitx5-data"
mkdir -p "$XDG_CONFIG_HOME/fcitx5" "$XDG_DATA_HOME"

if [ -n "${1:-}" ]; then
    cat > "$XDG_CONFIG_HOME/fcitx5/profile" <<PROFILE
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=$1

[Groups/0/Items/0]
Name=$1
Layout=

[Groups/0/Items/1]
Name=keyboard-us
Layout=

[GroupOrder]
0=Default
PROFILE
fi

# There is no X server in the session, and cloud pinyin asks a web service
# for candidates.
set -- fcitx5 --disable=xcb,xim,cloudpinyin

if command -v dbus-run-session >/dev/null 2>&1; then
    exec env -u DISPLAY LC_ALL=C.UTF-8 dbus-run-session -- "$@"
fi
exec env -u DISPLAY LC_ALL=C.UTF-8 DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent "$@"
