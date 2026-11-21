#!/bin/sh
# Name:        outrunners.sh
# Description: the game's launcher on the NES and SNES Classic, run by its .desktop from Hakchi: starts OutRunners with the
#              ROM set in the game's folder, and the home button leaves it
# Version:     0.1.0
# Created:     21-11-2026
# Author:      DefKorns
#
# license:BSD-3-Clause


source /etc/preinit
script_init

game_dir=$(dirname "$0")
code=$(basename "$game_dir")
rom="$game_dir/orunners.zip"

mkdir -p "/var/saves/$code"
chmod +x "$game_dir/OutRunners"
cd "$game_dir" || exit 1

export HOME=/var/saves/$code
export LD_LIBRARY_PATH=/usr/lib
export RECOMPLAUNCHER_MODE=bigpicture
export SDL_GAMECONTROLLERCONFIG="180000004e696e74656e646f20436c00,Nintendo Clovercon,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,lefttrigger:b6,righttrigger:b7,back:b8,start:b9,guide:b10,dpleft:b11,dpright:b12,dpup:b13,dpdown:b14,platform:Linux,"

[ -n "${ES_GAME:-}" ] || uistop

./OutRunners --game "$rom" &
game_pid=$!

if [ -x /bin/retroarch_watchdog ]; then
  /bin/retroarch_watchdog --pid "$game_pid" | while read -r type button value duration; do
    { { [ "$button" = home ] || [ "$button" = reset ]; } && [ "$value" = 1 ]; } ||
      { [ "$button" = power ] && [ "$value" = 0 ]; } && kill "$game_pid"
  done &
fi

wait "$game_pid"

[ -n "${ES_GAME:-}" ] || uistart
