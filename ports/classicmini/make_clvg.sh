#!/bin/sh
# Name:        make_clvg.sh
# Description: makes build/OutRunners.clvg, the game for Hakchi: the executable, the launcher, the cover and the
#              pictures it reads. No game data: the player puts orunners.zip in the game's folder in Hakchi
# Version:     0.1.0
# Created:     21-11-2026
# Author:      DefKorns
#
# license:BSD-3-Clause
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
stage=$(mktemp -d)

for file in OutRunners font.bmp input_prompts.bmp; do
  [ -f "$root/build/bin/$file" ] || { echo "build/bin/$file is missing: run ports/classicmini/build.sh" >&2; exit 1; }
done
cp "$root"/ports/classicmini/clvg/* "$stage/"
cp "$root/build/bin/OutRunners" "$root/build/bin/font.bmp" "$root/build/bin/input_prompts.bmp" "$stage/"
for file in CLV-G-ORUNR.png CLV-G-ORUNR_small.png; do
  [ ! -f "$root/build/classicmini-cover/$file" ] || cp "$root/build/classicmini-cover/$file" "$stage/"
done
tar -czf "$root/build/OutRunners.clvg" -C "$stage" .
rm -rf "$stage"
echo "$root/build/OutRunners.clvg"
