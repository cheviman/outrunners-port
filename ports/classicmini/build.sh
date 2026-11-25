#!/bin/sh
# Name:        build.sh
# Description: builds the console game for the NES and SNES Classic in Docker: SDL 2.0.4's headers and a library
#              to link against from the toolchain image, then ports/classicmini/container_build.sh in the GCC 13 one
# Version:     0.1.0
# Created:     21-11-2026
# Author:      DefKorns
#
# license:BSD-3-Clause
set -eu
export MSYS_NO_PATHCONV=1

root=$(cd "$(dirname "$0")/../.." && pwd)
es_image=${ES_TOOLCHAIN_IMAGE:-ghcr.io/defkorns/classicmini-es-toolchain:v0.1.0}
gcc13_image=${GCC13_IMAGE:-ghcr.io/defkorns/classicmini-gcc13:gcc13}

mkdir -p "$root/build/classicmini-sdl/include" "$root/build/classicmini-sdl/lib"
docker run --rm -v "$root/build/classicmini-sdl:/out" "$es_image" \
  sh -c 'cp -r /usr/include/SDL2 /out/include/ && cp -L /usr/lib/arm-linux-gnueabihf/libSDL2.so /out/lib/libSDL2.so'
docker run --rm -v "$root:/w" -w /w "$gcc13_image" sh ports/classicmini/container_build.sh
