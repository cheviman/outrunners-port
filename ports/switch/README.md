# OutRunners on the Nintendo Switch

The console version of the game (`OutRunners`) built for the Switch, as homebrew installed from an `.nsp`. It
is the same game as on a computer, with what a game console asks for:

- **The picture is WIDE, fixed**: the Switch's screen and a TV are 16:9, and the game's 16:9 picture fills
  them. OPTIONS has no VIDEO page.
- **The controls are fixed**: the left stick or the d-pad steers, ZR or A is the gas, ZL or B the brake, R
  and L shift up and down, Y is the radio (the d-pad's up and down its songs), + starts and pauses. OPTIONS
  has no CONTROLS page.
- **2 PLAYERS side by side**, with a second controller (another pair of Joy-Con or a Pro Controller).
- **The board's video is drawn on three cores**: the game runs on core 0, the video's rows on cores 1 and 2
  too (`runtime/src/workers.c`, libnx's threads, each on its core); in a one player game only the screen
  shown is drawn (`s32_video_show_screens`).
- **No EXIT** on the title's menu: HOME leaves the game, as it leaves any other.
- **No first run on the Switch**: the game's graphics, sound and data come in the `.nsp`, taken out of the
  player's ROM set on the player's computer. The Switch only installs and plays it.

What the game writes (its settings, the EEPROM, the bookkeeping, TIME ATTACK's times) goes to
`/switch/outrunners/` on the SD card, with `outrunners.log`, the steps of its start and its frames' pace.

## Making the .nsp (for players)

You need three things of your own, which never leave your computer:

- `prod.keys`, your console's keys (the tool uses `header_key` and `key_area_key_application_00`);
- `orunners.zip`, MAME's ROM set of *OutRunners* (World), or a folder of its files;
- the tool, `OutRunners NSP` (`OutRunners NSP.exe` on Windows), with the `switch` folder beside it: the
  game's Switch build (below); and `font.bmp`, its window's letters.

Start the tool (a double click). Its window shows what the `.nsp` will be made of, each with its state
(found, not found, or what is wrong with it):

- **Console keys**: `prod.keys` beside the tool, or in `.switch` in your user folder;
- **ROM set**: `orunners.zip` beside the tool;
- **HOME menu icon**: a 256 x 256 JPEG of yours, `icon.jpg` beside the tool or dropped on the window; else
  the build's own;
- **The game's Switch build**: the `switch` folder beside the tool;
- **Output folder**: where `OutRunners.nsp` goes, beside the tool unless you choose another.

Drop your files on the window and it takes each by what it is: a `.keys` (or `.txt`) is the keys, a
`.jpg` the icon, anything else the ROM set (`orunners.zip`, or a folder of its files). **Choose the output
folder** opens your system's folder dialog (on Linux zenity's or kdialog's). **Make OutRunners.nsp**, or
Enter, makes it: the window shows the step it is at, then the `.nsp`'s path and size, or what failed.

Files dropped on the tool itself, not on its window, are taken the same way, and it makes the `.nsp` at
once in its console. With options, from a command line, it is a console program and nothing else:

```
OutRunners NSP --keys prod.keys --rom orunners.zip --icon my_icon.jpg --out OutRunners.nsp
```

Install the `.nsp` with an installer (Tinfoil, DBI, Goldleaf...) on a console that runs Atmosphère with
signature patches: the `.nsp` is not signed by Nintendo, as no homebrew `.nsp` is.

## Building

### The Switch game

With [devkitPro](https://devkitpro.org/wiki/Getting_Started): its installer's **Switch Development**,
then, in devkitPro's MSys2 shell (Windows) or a shell with devkitPro's pacman (Linux, macOS):

```bash
pacman -S --needed switch-sdl2 switch-cmake switch-pkg-config
```

Then, from the repository's folder, in that shell:

```bash
/opt/devkitpro/portlibs/switch/bin/aarch64-none-elf-cmake -S . -B build/switch -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DORUNNERS_REFERENCE_TOOLS=OFF
make -C build/switch -j6 OutRunners_switch
```

`aarch64-none-elf-cmake` is CMake with devkitPro's Switch toolchain; `CMakeLists.txt` sees it and builds
the game alone (`switch.cmake`), into `build/bin/switch/`:

| File | What it is |
|---|---|
| `main` | the program (an NSO: the ELF `OutRunners.elf` through `elf2nso`) |
| `main.npdm` | its permissions on the console (`outrunners.json` through `npdmtool`): title `05005E6A4F520000` |
| `control.nacp` | its name, author and version (`nacptool`) |
| `icon.jpg` | its icon (`icon.jpg` here when the project has one, else libnx's) |
| `OutRunners.nro` | only when `build/bin/outrunners.assets` exists (the pack the computer's OutRunners writes on its first run): the game with your assets inside, for the Homebrew Menu. It is yours alone: never share it |

### The tool that makes the .nsp

`OutRunners NSP` is part of the computer build (`ports/switch/nsp/`, the target `orunners_nsp`):

```bash
cmake --build build/cmake --config Release --target orunners_nsp
```

Its window is built where the game's is, with SDL2 (with `font.bmp`, and on Windows `SDL2.dll` unless
SDL2 is built in); without SDL2 the tool is its command line alone. It lands in `build/bin/`, beside
`build/bin/switch/`, so from the repository it makes the `.nsp` at once:

```bash
"build/bin/OutRunners NSP" --keys prod.keys --rom rom/orunners.zip
```

### Checking an .nsp

`tools/check_nsp.py` (SegaSonic's) reads an `.nsp` as the console does, with another implementation of AES
(Python's `cryptography`): it deciphers each NCA's header with the keys, checks every hash and takes the
files out to compare them with the ones they were made from:

```bash
python tools/check_nsp.py build/bin/OutRunners.nsp prod.keys --pieces build/bin/switch --assets build/bin/outrunners.assets
```

It shows nothing of the keys.

## How the .nsp is made

As SegaSonic NSP makes SegaSonic's (its `crypto.c`, `formats.c`, `make_nsp.c`, `window.c` and
`folder_dialog.c`, with OutRunners' names):

1. It takes the assets out of the ROM set with the computer's first run (`runtime/src/extract.c`) into a
   folder beside the `.nsp`, loads them as the game does and writes the pack (`rom_pack.c`): the same
   `outrunners.assets` the computer's OutRunners writes. The folder goes once the pack is made.
2. **The program NCA**: its ExeFS (`main`, `main.npdm`) and its RomFS, with `outrunners.assets`. The game
   reads the pack from `romfs:/`.
3. **The control NCA**: `control.nacp` and the icon, a copy for each of the 16 languages.
4. **The meta NCA**: `Application_05005e6a4f520000.cnmt`, which lists the other two.
5. The NCAs ciphered with the console's keys, each named by its SHA-256's first half; the `.nsp` is a PFS0
   of the three.

| File | |
|---|---|
| `switch.cmake` | the Switch build |
| `outrunners.json` | `main.npdm`'s settings: the title, its range, the kernel's capabilities |
| `nsp/crypto.c` | AES-128 (ECB, CTR, XTS) and SHA-256 |
| `nsp/formats.c` | PFS0, RomFS, the hashed sections, the NCA, the content meta |
| `nsp/rom_pack.c` | the assets pack out of the ROM set, as the first run makes it |
| `nsp/make_nsp.c` | the `.nsp` from its inputs, for the command line and the window |
| `nsp/main.c` | the tool's command line; started with nothing, its window |
| `nsp/window.c` | the window (SDL2, with Kenney Mini's letters: `ports/sdl2/letters.c`) |
| `nsp/folder_dialog.c` | the system's folder dialog: Windows', macOS's (`osascript`), Linux's (`zenity` or `kdialog`) |

The game's own changes for the Switch are in `ports/sdl2/main.c` (`__SWITCH__`: the romfs, the SD card
folder, the screen; `ORUNNERS_HANDHELD`: the fixed picture), `console/menu.c` (OPTIONS without VIDEO and
CONTROLS, `ORUNNERS_TWO_PLAYERS`, `ORUNNERS_SYSTEM_EXIT`) and `runtime/src/tasks.c` (the game's two tasks
on stacks of their own without ucontext, which the Switch's C library has not).

Nothing of the game, the keys or the player's own icon is in this repository: the `.nsp` is made on the
player's computer, from the player's files. OutRunners is a trademark of SEGA; this project is not
affiliated with or endorsed by SEGA, nor with Nintendo.
