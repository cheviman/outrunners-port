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

void gpu_open(void);
void gpu_close(void);

/* The scene on the top screen */
void gpu_draw(const s32_scene *scene);

#endif
