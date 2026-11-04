# OutRunners on the New Nintendo 3DS

The console version of the game on a New 3DS or New 2DS, at 60 frames a second, with stereoscopic 3D. An original 3DS
is too slow and has too little memory.

## Playing it

You need homebrew on the console (Luma3DS and the Homebrew Launcher), and your own ROM set: **nothing of the game is in
the .3dsx**.

1. Copy `OutRunners.3dsx` to `sdmc:/3ds/`, and your `orunners.zip` (MAME's set, World) to `sdmc:/3ds/outrunners/`.
2. Start it from the Homebrew Launcher.

The first run takes the game's graphics and sounds out of the zip, once, into `outrunners.assets` in that folder: about
five minutes on a New 3DS, with nothing moving on the screen. After that the game starts at once, and the zip can go.
Holding L and R while the game starts makes the pack again.

Sound asks for the DSP firmware, `sdmc:/3ds/dspfirm.cdc` (dumped with DSP1 from the Homebrew Launcher); without it the
game plays silent.

| Control | |
|---|---|
| Circle pad or d-pad | the wheel, the menus' cursor |
| A, B | gas and accept, brake and back |
| R, L | shift up, shift down |
| X | the radio: music on and off, or the next song, as in the arcade |
| ZL, ZR | the song before, the song after |
| START | start |
| SELECT + START | leave the game |

The bottom screen shows the race's route map, the game's own, with your car on it and a large speedometer in its corner
(ARCADE and TIME ATTACK; it is the cabinet's other monitor, and it stays dark in the menus).

The 3D slider sets the depth: the road goes from the screen at the bottom into it at the horizon, what stands by the
road goes in with it, and your car, the words and the HUD stay at the screen. With the slider down it is the plain picture.

## Building

No devkitPro on the computer: Docker's `devkitpro/devkitarm` image does it, from the repository's folder.

```bash
docker run --rm -v "$PWD":/src -w /src devkitpro/devkitarm:latest ports/3ds/build.sh
```

That makes `build/3ds/OutRunners.3dsx`. Send a build to the Homebrew Launcher (press Y there) with 3dslink. Only ever send it a .3dsx: Luma's loader takes the
whole console down on anything else.

```bash
docker run --rm -v "$PWD":/src -w /src devkitpro/devkitarm:latest /opt/devkitpro/tools/bin/3dslink -a <the console's address> -r 10 build/3ds/OutRunners.3dsx
```

## How it works

There is no SDL2 there: `main.c` is the whole host, and `gpu.c` draws. The board makes each pixel out of palette offsets
and the PICA200 has no palette textures, so `runtime/src/video.c` describes each frame as a scene (`S32_VIDEO_SCENE`:
the mixer's order, each tilemap row's source, the sprites), and `gpu.c` keeps textures of its colours:

- each tilemap page is a 512 x 256 texture, whose tiles are made again when their cell or colours change;
- each row of a layer is a strip of a page, so the road's row scroll costs nothing;
- each sprite is a texture found by a hash of its place and colours, so a fade makes new ones and old colours find theirs;
- the layers are drawn from the back, each at its depth, then the sprites in the list's order, tested against the
  layers but never each other, as the board's sprite buffer has it.

The game runs on core 0, the sound board on core 2 up to four frames behind it, and saves on a thread of their own: the
SD card takes seconds. OutRunners plays no note on the YM3438, so `S32_FM_SILENT` leaves out its synthesis.

`ORUNNERS_SCENE_CHECK` builds `orunners_scene_check` on a computer (`check/`): the real `gpu.c` on a software stand-in for
citro3d, beside the board's own frames. It checks the logic, not the real GPU.

## Where it stands

- The picture is the board's to within a few percent of the pixels in the busiest scenes, mostly single pixels of the
  crowd. Not exact: the other ports' frames are.
- With the 3D slider up the game keeps 60 frames a second and the sound holds, but the GPU misses about every other
  frame: about 30 pictures a second.
- NETWORK and 2 PLAYERS are not there: the 3DS build has no sockets started, and one seat.
- The pause is not there yet: START does nothing in a race.
