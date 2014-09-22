#!/bin/sh

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
(
  cd "$srcdir" &&
  (
    rm -rf autom4te.cache
    rm -f aclocal.m4 ltmain.sh config.cache

    autoreconf --symlink --install || exit 1
  )
)

test -n "$NOCONFIGURE" || exec $srcdir/configure -C "$@"

exit 0
