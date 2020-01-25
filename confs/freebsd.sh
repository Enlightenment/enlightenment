#!/bin/sh -e
meson -Dsystemd=false -Ddevice-udev=false \
 $@ . build
