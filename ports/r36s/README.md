# OutRunners on the R36S (ArkOS)

The console version of the game (`OutRunners`) on the R36S and its RK3326 kin running ArkOS (Ubuntu 19.10,
aarch64, glibc 2.30, SDL 2.30 with KMSDRM), as a port in `ports` with PortMaster's controls. It is the
Anbernic build ([ports/anbernic](../anbernic/README.md)) with what this handheld asks for:

- **glibc 2.30**: the Anbernic build asks for glibc 2.34, which ArkOS does not have
  (`GLIBC_2.34' not found`); `zig_aarch64.cmake` here builds against 2.30.
- **The buttons from PortMaster**: the launcher takes PortMaster's `control.txt`, gives SDL its mapping of the
  handheld ("GO-Super Gamepad") and starts `gptokeyb`. **SELECT + START leave the game**: Anbernic's MENU is
  joystick button 8, which on the R36S is the d-pad's up, so it is left out (`HANDHELD_MENU_BUTTON=255`).
- **The game and its sound at 60 a second** (`ORUNNERS_FRAMESKIP=2`): the RK3326 takes about 20 ms a frame
  with the picture drawn, so a frame already past its time is not drawn (never more than 2 in a row), while the
  game and the sound board run every frame; the sound starts with 3 frames of silence queued, so a late frame
  does not leave the device without sound. Without it the game ran at 46-55 frames a second, slower than the
  arcade, its music broken up.
- **Built for the Cortex-A35** (`-mcpu=cortex-a35`).

On an R36S (ArkOS 07312025, PortMaster 2026.06.23) it runs at 60 frames a second, a quarter or so of the
frames not drawn in a race, with the sound whole.

## Installing it (for players)

1. Make `PORTS` with OutRunners Anbernic from your ROM set, as [ports/anbernic](../anbernic/README.md) says.
2. Copy the folder `outrunners` and `OutRunners.sh` to the card's `ports` folder (the `EASYROMS` partition).
3. Put this build's `OutRunners.sh` and `outrunners/OutRunners` over them.
4. PortMaster must be installed (ArkOS has it). Start **Ports > OutRunners**.

What the game says goes to `outrunners/outrunners.log`: PortMaster's device, the mapping, and every 300
frames the frames a second, how many were not drawn and the sound queued.

## Building

As the Anbernic build, with this toolchain and `ORUNNERS_R36S`:

```bash
cmake -S . -B build/r36s -G Ninja -DCMAKE_TOOLCHAIN_FILE=ports/r36s/zig_aarch64.cmake -DORUNNERS_ANBERNIC=ON -DORUNNERS_R36S=ON -DCMAKE_BUILD_TYPE=Release -DORUNNERS_REFERENCE_TOOLS=OFF
cmake --build build/r36s --target OutRunners
```

It makes `build/bin/r36s/`: `OutRunners.sh` and `outrunners/OutRunners` (and `input_prompts.bmp`).
