# The board's video on the GPU (branch switch-4-players)

Sebas, 30-10-2026: the Switch with 4 players (two cabinets of two seats in one device, the screen in four)
draws four of the board's screens a frame; the board's video goes to the GPU so that it fits.

## Where the time goes today

`runtime/src/video.c` draws the Multi 32's video in software, as MAME's `segas32_v.cpp` does, pixel for
pixel: every layer writes a palette offset per pixel into a buffer of its own through its clip windows, the
sprites are drawn into frame buffers when the board sends the list, and each screen's mixer takes, pixel by
pixel, the topmost layer that is not transparent, maybe blends it, and looks its colour up in the palette.

Measured on the computer, one thread, a race (`orunners_console_headless`, a frame's video):

| | layers | sprites | mixing |
|---|---|---|---|
| one screen | 1.47 ms (54%) | 0.39 ms (14%) | 0.87 ms (32%) |
| two screens | 2.44 ms | 0.83 ms | 1.67 ms |

The game's own work is 0.18 ms a frame. On the Switch (Eden, the video on three cores) a one-screen frame
takes 8 to 12.7 ms: its video 4.5 to 6.8, showing it 1, the sound board most of the rest. Four screens and two
sound boards would be about 35 ms, twice a frame at 60.

## The plan

The layers and the mixing, 85% of the time, are what a GPU does best: a function of each pixel, from
textures. The sprites stay on the CPU at first (14%): their list is walked in order, and they are drawn into
frame buffers the GPU takes as textures.

1. **The GPU's frame, for a screen**: one pass per tile layer (NBG0-NBG3: zoomed and scrolled, with the line
   tables' row scroll and row select), the text layer and the bitmap layer, each into a texture of palette
   offsets (R16UI, 416 x 224); then the mixer's pass, which reads them with the sprites' buffer, the palette
   and the mixer's registers and writes the colours straight into the texture the window shows. Nothing
   comes back to the CPU.
2. **What goes up each frame**: the video RAM (the tilemap pages, the text cells, the bitmap, the line tables
   and the registers: 128 KB), the palettes (each screen's), the mixer's registers, the sprites' buffers
   (two 416 x 224 x 16-bit a screen at most) and the clip windows' runs (the CPU works them out as today:
   `find_clip_runs`). Once: the tiles' pens and the text's graphics (the assets' tilesets), as integer
   textures.
3. **The shaders**: GLSL for OpenGL 3.3 core and OpenGL ES 3.0 (integer textures, `usampler2D`), the same
   source with a version line each: computers (OpenGL 3.3; macOS 4.1 core), Android (GLES 3.0), the Switch
   (OpenGL through devkitPro's switch-mesa), Anbernic's H700 (Mali-G31, GLES 3.0 where the firmware's SDL
   gives it).
4. **The window**: SDL2's OpenGL context in place of SDL_Renderer where the GPU draws the video: the frame
   texture to the window with the pixels' kinds (sharp, smooth, bilinear, CRT as a shader), and the port's
   own drawings (the touch controls, the letters, the controls' pictures, the pause's picture) as textured
   quads. SDL_Renderer stays where there is no OpenGL.
5. **The reference stays the CPU's**: the headless builds, the scenarios and the hashes keep `video.c`;
   the GPU's frames are checked against it (the same frame drawn both ways, read back once for the check:
   identical, or the pixels that differ and why).

## What the Switch's OpenGL takes (Mesa 20.1 on Eden)

The same shaders draw identical frames on the computer (OpenGL 4.6 and OpenGL ES 3.2) and on the Switch in
Eden, with three things done the Switch's way (`ports/sdl2/gpu_video.c`):

- **No integer multiplies in the shaders.** Mesa makes them, and signed divisions by a constant (even by 2),
  into the GPU's IMAD; Eden does not take IMAD and drops the whole draw, so the picture was black. The
  shaders shift and add where `video.c` multiplies, blend in floats (exact on such small whole numbers), and
  wrap a tile's number with a mask (the tiles are a power of two, or the CPU draws the layers).
- **No array texture to draw into.** Drawing into a layer of an array texture, Eden draws into the first:
  the layers go in one tall texture instead, 256 rows each (`layer << 8 | y`), each drawn through its own
  viewport.
- **A texture is the GPU's or the CPU's, never both.** When the CPU writes part of a texture the GPU drew
  into, Eden loads the whole texture back from memory and what the GPU drew is lost: the sprites (the CPU's)
  have a texture of their own.

Measured on Eden (attract, one screen, 60 frames a second both ways): the CPU's video takes 2.9 to 3.7 ms
of a frame and showing it 0.7; on the GPU, 1.1 to 1.4 and 2.2 to 2.5. The whole frame stays at 5 to 8 ms
either way, but the CPU's share of the video drops by about 2 ms, room for the second cabinet. Eden's times are the emulator's:
the real Switch is still to be measured.

## The steps, each checked

1. The window on OpenGL with the CPU's frame as a texture (nothing else changes): the same picture as
   today on the computer, Android and the Switch (Eden).
2. The mixer on the GPU, the layers still the CPU's (uploaded as textures): the frames identical to the
   CPU's in a race, the title, 2 PLAYERS, OPTIONS, REVERSE's map.
3. The text and bitmap layers on the GPU; then the scrolled layers; then the zoomed ones: identical again
   after each.
4. Measured on the Switch (Eden): one screen, above; a frame of two screens still to measure.
5. Two worlds in one process (two cabinets: the game's libraries twice, their symbols apart), linked in
   memory as NETWORK links devices (`console/network.c`), the screen in four, a controller each.
6. Measured with four players on the Switch; then, if it needs it, the sprites on the GPU too.
