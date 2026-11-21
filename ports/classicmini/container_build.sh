#!/bin/sh
# Name:        container_build.sh
# Description: the part of build.sh that runs inside the GCC 13 toolchain image
# Version:     0.1.0
# Created:     21-11-2026
# Author:      DefKorns
#
# license:BSD-3-Clause
set -eu

apt-get update -qq > /dev/null
apt-get install -y -qq cmake ninja-build > /dev/null
cmake -S . -B build/classicmini -G Ninja -DCMAKE_TOOLCHAIN_FILE=ports/classicmini/gcc13_armv7.cmake \
  -DCMAKE_BUILD_TYPE=Release -DORUNNERS_CLASSICMINI=ON -DORUNNERS_REFERENCE_TOOLS=OFF
cmake --build build/classicmini --target outrunners
