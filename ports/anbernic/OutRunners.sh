#!/bin/sh
# OutRunners for Anbernic's H700 handhelds (stock firmware): the game in the folder outrunners beside this
# script, with the firmware's SDL2; what it says goes to outrunners/outrunners.log
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/outrunners" || exit 1
chmod +x ./OutRunners 2>/dev/null
./OutRunners > outrunners.log 2>&1
sync
