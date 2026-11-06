#!/bin/sh
# Builds build/3ds/OutRunners.3dsx in the devkitpro/devkitarm image:
#   docker run --rm -v "$PWD":/src -w /src devkitpro/devkitarm ports/3ds/build.sh
set -e
export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
/opt/devkitpro/portlibs/3ds/bin/arm-none-eabi-cmake -S . -B build/3ds -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DORUNNERS_REFERENCE_TOOLS=OFF
cmake --build build/3ds --target OutRunners_3dsx --parallel 8 2>&1 | tail -20 || cmake --build build/3ds --parallel 8
