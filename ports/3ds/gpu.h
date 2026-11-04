/**
 *    Name:        gpu.h
 *    Description: the frames on the PICA200: what the board's video would draw, as the scene describes it
 *    Version:     0.1.0
 *    Created:     03-11-2026
 *    Modified:    03-11-2026
 *    Author:      yoshisuga
 *
 *    license:BSD-3-Clause
 */
#ifndef ORUNNERS_PORT_GPU_H
#define ORUNNERS_PORT_GPU_H

#include "s32/video.h"

typedef struct
{
	unsigned pages;                 /* tilemap pages touched */
	unsigned tiles;                 /* tiles redrawn */
	unsigned sprites;
	unsigned decoded;               /* sprites not found in the textures made before */
	unsigned quads;
	unsigned sprite_kb;
	unsigned frames_missed;         /* not drawn: the GPU was not done with the last one */
} gpu_stats;

void gpu_open(void);
void gpu_close(void);

/* The scene on the top screen */
void gpu_draw(const s32_scene *scene);

/* Until the GPU has done with what it was given */
void gpu_wait(void);

/* What the frames since the last call took */
void gpu_take_stats(gpu_stats *stats);

#endif
