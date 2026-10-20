#!/bin/sh
# Compatibility wrapper for older packaging commands. New packages call
# `DESTDIR=... cmake --install build --prefix /usr` directly.
set -eu
dest=${1:?usage: packaging/linux/stage.sh DESTDIR}
DESTDIR="$dest" cmake --install build --prefix /usr --strip
