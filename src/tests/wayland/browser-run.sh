#!/bin/sh
# Run a browser as run-nested.sh's E_TEST_APP, cut off from the user's own
# session: a throwaway profile, a private D-Bus so no portal or secret
# service of the real desktop answers, no network, a pinned locale.
#
#   browser-run.sh <firefox|chromium|brave> <url>
#   browser-run.sh --find <firefox|chromium|brave>   fails when not installed

set -eu

_find() {
    case "$1" in
        firefox)  set -- firefox firefox-esr ;;
        chromium) set -- chromium chromium-browser google-chrome google-chrome-stable ;;
        brave)    set -- brave brave-browser ;;
        *)        return 1 ;;
    esac
    for b in "$@"; do
        command -v "$b" 2>/dev/null && return 0
    done
    return 1
}

if [ "${1:-}" = "--find" ]; then
    _find "${2:-}" >/dev/null
    exit
fi

BROWSER=$1
URL=$2
BIN=$(_find "$BROWSER")
PROFILE="$E_TEST_RUNDIR/browser-profile"
mkdir -p "$PROFILE"

# The content sandbox of the browsers does not read from the source tree, so
# a local page is copied next to the profile.
case "$URL" in
file://*)
    cp "${URL#file://}" "$E_TEST_RUNDIR/page.html"
    URL="file://$E_TEST_RUNDIR/page.html"
    ;;
esac

ENV="LC_ALL=C.UTF-8 TZ=UTC XDG_SESSION_TYPE=wayland"

case "$BROWSER" in
firefox)
    # A new profile opens the startup page but ignores a URL given on the
    # command line, and so does -private-window.
    cat > "$PROFILE/user.js" <<JS
user_pref("browser.startup.homepage", "$URL");
user_pref("browser.startup.page", 1);
user_pref("browser.privatebrowsing.autostart", true);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.startup.homepage_override.mstone", "ignore");
user_pref("app.update.enabled", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("toolkit.telemetry.enabled", false);
user_pref("network.captive-portal-service.enabled", false);
user_pref("network.connectivity-service.enabled", false);
user_pref("browser.safebrowsing.malware.enabled", false);
user_pref("browser.safebrowsing.phishing.enabled", false);
user_pref("extensions.update.enabled", false);
JS
    # Without -no-remote, a Firefox already running gets the URL and this
    # one exits.
    set -- "$BIN" -profile "$PROFILE" -no-remote "$URL"
    ENV="$ENV MOZ_ENABLE_WAYLAND=1 MOZ_CRASHREPORTER_DISABLE=1"
    ;;
chromium|brave)
    # Software rendering: the test canvas cannot import GPU buffers, and a
    # browser finding that out at startup is slow. E_TEST_BROWSER_FLAGS is for
    # a test that needs a feature turned on.
    set -- "$BIN" --user-data-dir="$PROFILE" --incognito \
        --no-first-run --no-default-browser-check \
        --disable-search-engine-choice-screen \
        --password-store=basic --use-mock-keychain \
        --disable-component-update --disable-background-networking \
        --host-resolver-rules="MAP * ~NOTFOUND" \
        --ozone-platform=wayland --disable-gpu ${E_TEST_BROWSER_FLAGS:-} \
        "$URL"
    ;;
esac

if command -v dbus-run-session >/dev/null 2>&1; then
    exec env -u DISPLAY $ENV dbus-run-session -- "$@"
fi
exec env -u DISPLAY $ENV DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent "$@"
