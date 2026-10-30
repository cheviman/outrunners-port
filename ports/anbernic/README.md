# OutRunners on Anbernic's H700 handhelds

The console version of the game (`OutRunners`) on Anbernic's H700 handhelds with their stock firmware, as a
port in `Roms/PORTS` (Ubuntu 22.04, aarch64, glibc 2.35, Anbernic's SDL 2.0.12), as SegaSonic's port is. It
is the same game as on a computer, with what a handheld asks for:

- **The game's own 4:3 picture**, as large as the 640 x 480 screen takes it, smooth pixels. OPTIONS has no
  VIDEO page, and the title's menu no 2 PLAYERS.
- **The handheld's buttons** (Sebas): the d-pad or the left stick steers, L1 (or R2) is the gas, L2 the brake,
  Y and B shift up and down, SELECT is the radio (the d-pad's up and down its songs), START starts and pauses
  (`ORUNNERS_ANBERNIC_BUTTONS`); in the menus A accepts and B goes back. OPTIONS' CONTROLS gives each action
  another button, and its DEFAULTS brings these back (a `controls.ini` kept from an earlier version keeps the
  buttons it has until then).
- **MENU leaves the game**, as it leaves the firmware's other games; the volume keys turn the game's volume
  (OPTIONS' VOLUME).
- **The board's video on the four cores** (`runtime/src/workers.c`), the left screen alone in a one player
  game (`s32_video_show_screens`).
- **No first run on the handheld**: the game's graphics, sound and data come in its folder, taken out of
  the player's ROM set on the player's computer by OutRunners Anbernic.

What the game writes (its settings, the EEPROM, the bookkeeping, TIME ATTACK's times) goes to its folder on
the card, with `outrunners.log`: what SDL found (its version, video and audio, the screen, the controls) and
every 300 frames the frames a second and a frame's time.

## Making PORTS (for players)

You need two things of your own, which never leave your computer:

- `orunners.zip`, MAME's ROM set of *OutRunners* (World), or a folder of its files;
- the tool, `OutRunners Anbernic` (`OutRunners Anbernic.exe` on Windows), with the `anbernic` folder beside
  it: the game's Anbernic build (below); and `font.bmp`, its window's letters.

Start the tool (a double click). Its window shows what `PORTS` will be made of, each with its state:

- **ROM set**: `orunners.zip` beside the tool, or dropped on the window;
- **The game's Anbernic build**: the `anbernic` folder beside the tool;
- **Output folder**: where `PORTS` goes, beside the tool unless you choose another. Choose your card's `Roms`
  folder and the game is on it at once: the other ports in its `PORTS` stay as they are.

**Make PORTS**, or Enter, makes it: the window shows the step it is at, then where `PORTS` is, or what failed.
A ROM set that cannot be used leaves a game already there as it was. With options, from a command line, it is
a console program and nothing else:

```
OutRunners Anbernic --rom orunners.zip --out E:/Roms
```

`PORTS` holds:

| File | What it is |
|---|---|
| `OutRunners.sh` | the launcher: it starts the game in its folder, its output to `outrunners.log` |
| `outrunners/OutRunners` | the game, an aarch64 executable that needs the firmware's `libSDL2-2.0.so.0`, `libm` and `libc` (glibc 2.35 at most) |
| `outrunners/outrunners.assets` | the assets pack of your ROM set (yours alone: never share it) |
| `outrunners/input_prompts.bmp` | CONTROLS' button pictures (Kenney's Input Prompts Pixel, CC0) |

## Building

On Windows, Linux or macOS, with [Zig](https://ziglang.org) as the compiler for the handheld: it builds C for
aarch64 Linux against the exact glibc the firmware has, with nothing else installed. Unpack it in
`build/downloads/zig/` (or give its path as `-DZIG=`), with SDL2's development package in
`build/downloads/sdl2/` (for SDL's headers):

```bash
cmake -S . -B build/anbernic -G Ninja -DCMAKE_TOOLCHAIN_FILE=ports/anbernic/zig_aarch64.cmake -DORUNNERS_ANBERNIC=ON -DCMAKE_BUILD_TYPE=Release -DORUNNERS_REFERENCE_TOOLS=OFF
cmake --build build/anbernic --target OutRunners
```

It makes `build/bin/anbernic/`, PORTS's files without the assets: `OutRunners.sh`, `outrunners/OutRunners`
and `outrunners/input_prompts.bmp`. `OutRunners Anbernic` is part of the computer build
(`ports/anbernic/maker/`, the target `orunners_anbernic_maker`), and lands beside that folder in `build/bin/`,
so from the repository it makes `PORTS` at once:

```bash
cmake --build build/cmake --config Release --target orunners_anbernic_maker
"build/bin/OutRunners Anbernic" --rom rom/orunners.zip
```

## How it is built

- `zig_aarch64.cmake`: CMake's toolchain, Zig's `cc` for `aarch64-linux-gnu.2.35`.
- `anbernic.cmake`: the console game with `ORUNNERS_HANDHELD` (the picture fixed, the handheld's log, no
  first run), `ORUNNERS_HANDHELD_KEYS` (MENU and the volume keys) and `ORUNNERS_CONTROLS_PAGE` (CONTROLS in
  OPTIONS).
- The game uses the firmware's SDL2, which knows its screen and GPU: the build links it against
  `sdl2_stub.c`, a library of the same name with the functions the port calls, so the executable asks for
  `libSDL2-2.0.so.0` and takes the firmware's when it starts. A function the port starts to call goes
  there too (the link says which).
- The handheld's buttons come as a joystick SDL 2.0.12 does not know, "ANBERNIC-keys": the game gives SDL
  its mapping before SDL starts (`device_mappings` in `ports/sdl2/main.c`), as SegaSonic's port found them
  on the RG40XX H: A 0, B 1, X 3, Y 2, L1 4, L2 5, R1 10, R2 11, SELECT 6, START 7, MENU 8, L3 9, R3 12, the
  d-pad hat 0, the sticks axes 0 to 3.
- `maker/`: OutRunners Anbernic. `make_ports.c` writes `PORTS` from the build and the assets pack, which
  OutRunners NSP's `rom_pack.c` makes from the ROM set as the computer's first run does; `main.c` is its
  command line, `window.c` its window (OutRunners NSP's, with its rows), with OutRunners NSP's folder dialog.

## Where it stands

- It builds and links; it has not run on a handheld yet.
- Other H700 handhelds (RG35XX Plus, RG40XX V, RG CubeXX...) should run it as the RG40XX H does, with the
  same buttons. Other firmwares (muOS, Knulli, ROCKNIX) have other SDLs and paths.

Nothing of the game is in this repository: its folder is made on the player's computer, from the player's
ROM set. OutRunners is a trademark of SEGA; this project is not affiliated with or endorsed by SEGA, nor
with Anbernic.
