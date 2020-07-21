#!/bin/sh -e
meson -Dsystemd=false -Ddevice-udev=false -Dgesture-recognition=false \
 $@ . build
