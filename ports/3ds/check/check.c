/**
 *    Name:        check.c
 *    Description: the frames ports/3ds/gpu.c draws beside the board's own, pixel by pixel, on a computer
 *    Author:      yoshisuga
 *
 *    license:BSD-3-Clause
 *    Usage (ORUNNERS_SCENE_CHECK): orunners_scene_check --wide --video-hashes /dev/null ... with CHECK_DIR set: the
 *    pixels that differ by more than a step of the board's 5 bits, a line a second, and the worst frames as BMPs,
 *    GPU above, board below.
 */
#include "gpu.h"

#include "s32/machine.h"
#include "s32/video.h"

#include <citro3d.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH   400
#define HEIGHT  224
#define TOP     8
#define REPORT  60
#define DUMPS_MOST 200
#define DUMP_AT 1500    /* pixels differing: worth a picture */

void s32_video_check_pixel(int x, int y);

static s32_scene scene;
static unsigned long total_wrong, total_frames, window_wrong, window_worst;
static int dumps;

static void dump(int frame, const uint32_t *board, int wrong)
{
	const char *folder = getenv("CHECK_DIR");
	char path[512];
	FILE *file;
	static uint8_t row[WIDTH * 3 + 4];
	uint32_t size = 54 + (WIDTH * 3) * (HEIGHT * 2);
	uint8_t header[54] = { 'B', 'M' };

	if (!folder || dumps >= DUMPS_MOST)
		return;
	dumps++;
	snprintf(path, sizeof(path), "%s/frame_%06d_%d.bmp", folder, frame, wrong);
	if (!(file = fopen(path, "wb")))
		return;
	memcpy(header + 2, &size, 4);
	header[10] = 54;
	header[14] = 40;
	{
		uint32_t width = WIDTH, height = HEIGHT * 2;
		uint16_t planes = 1, bits = 24;

		memcpy(header + 18, &width, 4);
		memcpy(header + 22, &height, 4);
		memcpy(header + 26, &planes, 2);
		memcpy(header + 28, &bits, 2);
	}
	fwrite(header, 1, 54, file);
	for (int y = HEIGHT - 1; y >= 0; y--)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			const uint32_t pixel = board[y * S32_VIDEO_MAX_WIDTH + x];

			row[x * 3] = (uint8_t)pixel;
			row[x * 3 + 1] = (uint8_t)(pixel >> 8);
			row[x * 3 + 2] = (uint8_t)(pixel >> 16);
		}
		fwrite(row, 1, WIDTH * 3, file);
	}
	for (int y = HEIGHT - 1; y >= 0; y--)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			row[x * 3] = check_screen[y + TOP][x][2];
			row[x * 3 + 1] = check_screen[y + TOP][x][1];
			row[x * 3 + 2] = check_screen[y + TOP][x][0];
		}
		fwrite(row, 1, WIDTH * 3, file);
	}
	fclose(file);
}

void scene_check_frame(void)
{
	static bool open;
	static int quiet;
	const int frame = (int)s32_board.frame;
	int width, wrong = 0;
	const uint32_t *board = s32_video_frame(0, &width);

	if (!open)
	{
		open = true;
		s32_video_use_scene();
		gpu_open();
		return;
	}
	s32_video_scene(&scene);
	gpu_draw(&scene);
	if (getenv("CHECK_AT") && atoi(getenv("CHECK_AT")) == frame)
	{
		int px = atoi(getenv("CHECK_X")), py = atoi(getenv("CHECK_Y"));

		for (int step = 0; step < 6; step++, py += 2)
		{
			const uint8_t *gpu = check_screen[py + TOP][px];

			printf("probe gpu %02X%02X%02X", gpu[0], gpu[1], gpu[2]);
			s32_video_check_pixel(px + (width - WIDTH) / 2, py);
		}
		for (int index = 0; index < scene.sprite_count; index++)
		{
			const s32_scene_sprite *it = &scene.sprites[index];

			if (it->x + (WIDTH - width) / 2 < px + 30 && it->x + it->width + (WIDTH - width) / 2 > px - 30 && it->y < py + 10 && it->y + it->height > py - 20)
				printf("   list %d at %d,%d %dx%d clip %d-%d,%d-%d src %dw %dr shadow %d ind %d col %04X end %d\n", index, it->x, it->y, it->width, it->height, it->clip_left, it->clip_right, it->clip_top, it->clip_bottom, it->words, it->rows, it->shadow, it->indirect, it->color, it->end_pen);
		}
	}
	{
		/* (a frame narrower than the screen stands in its middle, as show_frame had it: bars at the sides) */
		static uint32_t placed[S32_VIDEO_HEIGHT][S32_VIDEO_MAX_WIDTH];
		const int shown = width < WIDTH ? width : WIDTH;
		const int into = (WIDTH - shown) / 2, from = (width - shown) / 2;

		memset(placed, 0, sizeof(placed));
		for (int y = 0; y < HEIGHT; y++)
			memcpy(&placed[y][into], &board[y * S32_VIDEO_MAX_WIDTH + from], (size_t)shown * sizeof(uint32_t));
		board = &placed[0][0];
	}
	if (getenv("CHECK_WIDTHS") && total_frames % 30 == 0)
		printf("frame %d width %d side %d on %d\n", frame, width, scene.side, scene.on);
	for (int y = 0; y < HEIGHT; y++)
		for (int x = 0; x < WIDTH; x++)
		{
			const uint32_t pixel = board[y * S32_VIDEO_MAX_WIDTH + x];
			const uint8_t *gpu = check_screen[y + TOP][x];
			const int red = abs(((int)((pixel >> 16) & 0xff) >> 3) - (gpu[0] >> 3));
			const int green = abs(((int)((pixel >> 8) & 0xff) >> 3) - (gpu[1] >> 3));
			const int blue = abs(((int)(pixel & 0xff) >> 3) - (gpu[2] >> 3));

			wrong += red > 1 || green > 1 || blue > 1;
		}
	total_wrong += (unsigned long)wrong;
	total_frames++;
	window_wrong += (unsigned long)wrong;
	if ((unsigned long)wrong > window_worst)
		window_worst = (unsigned long)wrong;
	if (wrong >= DUMP_AT && quiet <= 0)
	{
		int shown = 0;

		printf("frame %d, wrong pixels:\n", frame);
		for (int y = 3; y < HEIGHT && shown < 4; y += 11)
			for (int x = 5; x < WIDTH && shown < 4; x += 37)
			{
				const uint32_t pixel = board[y * S32_VIDEO_MAX_WIDTH + x];
				const uint8_t *gpu = check_screen[y + TOP][x];

				if (abs((int)((pixel >> 16) & 0xff) - gpu[0]) + abs((int)((pixel >> 8) & 0xff) - gpu[1]) + abs((int)(pixel & 0xff) - gpu[2]) > 40)
				{
					printf("  gpu %02X%02X%02X", gpu[0], gpu[1], gpu[2]);
					s32_video_check_pixel(x + (width - WIDTH) / 2 >= 0 ? x + (width - WIDTH) / 2 : 0, y);
					shown++;
					for (int index = scene.sprite_count - 1, found = 0; index >= 0 && found < 3; index--)
					{
						const s32_scene_sprite *it = &scene.sprites[index];
						const int fx = x + (width - WIDTH) / 2;

						if (fx >= it->x && fx < it->x + it->width && y >= it->y && y < it->y + it->height)
						{
							const int pixel = it->indirect ? it->table[1] : it->color;

							printf("    sprite %d at %d,%d %dx%d clip %d-%d,%d-%d src %dw %dr ram %d b8 %d ind %d col %04X tbl %04X %04X end %d grp %d above %d\n", index, it->x, it->y,
								it->width, it->height, it->clip_left, it->clip_right, it->clip_top, it->clip_bottom, it->words, it->rows, it->from_ram, it->bits8, it->indirect, it->color, it->table[0], it->table[1],
								it->end_pen, (pixel >> scene.group_shift) & scene.group_mask, scene.sprites_above[(pixel >> scene.group_shift) & scene.group_mask]);
							found++;
							printf("      words %08X %08X addr %X bank %d tr %04X table", s32_video_scene_sprite_word(it, it->address), s32_video_scene_sprite_word(it, it->address + 1),
								(unsigned)it->address, it->bank, it->transparent_bits);
							for (int entry = 0; entry < 16; entry++)
								printf(" %04X", it->table[entry]);
							printf(" pixcol %04X bits %04X shpen %04X\n", s32_video_sprite_pixel_color(0, 0x8000), scene.sprite_bits, scene.shadow_pen);
						}
					}
				}
			}
		dump(frame, board, wrong);
		quiet = frame < 400 ? 30 : 300;
	}
	quiet--;
	if (total_frames % REPORT == 0)
	{
		printf("frame %d: %lu wrong a frame, worst %lu, sprites %d, layers", frame, window_wrong / REPORT, window_worst, scene.sprite_count);
		for (int index = 0; index < scene.layer_count; index++)
			printf(" %c", "T0123BSD"[scene.layers[index].index]);
		printf(" spr_above %d %d %d %d\n", scene.sprites_above[0], scene.sprites_above[1], scene.sprites_above[2], scene.sprites_above[3]);
		window_wrong = window_worst = 0;
	}
}
