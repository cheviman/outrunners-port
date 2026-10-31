/**
 *    Name:        gpu_video.h
 *    Description: the board's video mixed on the GPU (docs/gpu_video.md): each screen's mixer as a shader
 *                 drawing straight into the window's texture, in SDL's own OpenGL context
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 */
#ifndef ORUNNERS_PORT_GPU_VIDEO_H
#define ORUNNERS_PORT_GPU_VIDEO_H

#include <SDL.h>
#include <stdbool.h>

#include "s32/video.h"

/* Sets the GPU's mixing up in the renderer's OpenGL context (an "opengl" or "opengles2" renderer): false, and
   the CPU mixes as before, where it cannot (another renderer, no OpenGL 3 or OpenGL ES 3, a shader refused) */
bool gpu_video_open(SDL_Renderer *renderer);
void gpu_video_close(void);

/* The frame's layers drawn on the GPU, from what each draws with (s32_video_layers_for_gpu), for every screen's
   mixing after it: false, and the mixing takes the CPU's layers, where it cannot (the tiles' pens larger than
   a texture the GPU takes) */
bool gpu_video_draw_layers(const s32_gpu_layers *layers);

/* A screen's frame mixed on the GPU into 'texture' (the renderer's, ARGB8888) at 'place', its columns from
   'first' (the middle of a wider frame), from what the screen's mixer reads */
void gpu_video_mix(SDL_Texture *texture, const SDL_Rect *place, int first, const s32_video_mixing *mixing);

/* The pixels the GPU mixed at 'place' of 'texture', read back as the CPU's frame has them (0x00RRGGBB, 'pitch'
   pixels a row), for the checks: false where they cannot be read */
bool gpu_video_read(SDL_Texture *texture, const SDL_Rect *place, uint32_t *pixels, int pitch);

#endif
