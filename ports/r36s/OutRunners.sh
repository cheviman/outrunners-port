#!/bin/bash
# OutRunners para R36S/R36XX com ArkOS: usa o PortMaster para os botoes; SELECT+START sai do jogo
HERE=$(cd "$(dirname "$0")" && pwd)
GAMEDIR="$HERE/outrunners"
cd "$GAMEDIR" || exit 1
exec > "$GAMEDIR/outrunners.log" 2>&1

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
for d in /opt/system/Tools/PortMaster /opt/tools/PortMaster "$XDG_DATA_HOME/PortMaster" /roms/ports/PortMaster; do
  [ -f "$d/control.txt" ] && controlfolder="$d" && break
done
echo "PortMaster: ${controlfolder:-nao encontrado}"

if [ -n "$controlfolder" ]; then
  source "$controlfolder/control.txt"
  [ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && source "$controlfolder/mod_${CFW_NAME}.txt"
  get_controls
  export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
  echo "Controle: $sdl_controllerconfig"
  $GPTOKEYB "OutRunners" &
fi

chmod +x ./OutRunners 2>/dev/null
./OutRunners
echo "codigo de saida=$?"

if [ -n "$controlfolder" ]; then
  $ESUDO kill -9 $(pidof gptokeyb) 2>/dev/null
  type pm_finish >/dev/null 2>&1 && pm_finish
fi
printf "\033c" > /dev/tty0 2>/dev/null
sync
