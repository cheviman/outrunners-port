/**
 *    Name:        gpu.c
 *    Description: the frames on the PICA200, from the scene runtime/src/video.c describes
 *    Version:     0.1.0
 *    Created:     03-11-2026
 *    Modified:    03-11-2026
 *    Author:      yoshisuga
 *
 *    license:BSD-3-Clause
 *    The board makes a pixel at a time out of palette offsets; the PICA200 has no palette textures. So each tilemap
 *    page is a texture of its colours, whose tiles are made again when their cell or their colours change (little
 *    changes: a few pages of 128 and a few of the 16384 colours a frame), and each row of a layer is a strip of a
 *    page's texture. Each sprite is a texture of its colours, found again by a hash of its place and its colours, so
 *    a fade makes new ones and the colours coming back find the old ones. The layers are drawn from the back in the
 *    mixer's order, each at a depth of its own, then the sprites in their list's order, as the board's buffer has them:
 *    each tested against the layers by its group's place among them, and never against another sprite. RGBA5551 holds
 *    the board's colours as they are.
 */
#include "gpu.h"

#include "game/assets.h"
#include "s32/machine.h"

#include "outrunners_shader_shbin.h"

#include <3ds.h>
#include <citro3d.h>

#include <math.h>
#include <string.h>

#define SCREEN_WIDTH     400
#define SCREEN_HEIGHT    240
#define TOP              ((SCREEN_HEIGHT - S32_VIDEO_HEIGHT) / 2)
#define PAGE_WIDTH       512
#define PAGE_HEIGHT      256
#define BITMAP_HEIGHT    512         /* of 4 bits a pixel; 256 of 8 */
#define PAGE_CELLS       512         /* 32 x 16 */
#define PAGE_SLOTS       20
#define TEXT_CELLS       2048        /* 64 x 32 */
#define TEXT_CHARACTERS  512
#define PALETTE_BLOCKS   (S32_PALETTE_ENTRIES / 16)
#define QUADS_MOST       8192
#define EYE_MOST         6.0f        /* pixels each eye's picture moves, at the slider's top and the far end */
#define EDGE             8           /* columns drawn past the screen's sides, for the eyes' pictures to move into */
#define ROAD_ROWS_LEAST  20          /* rows that do not follow on from the last: a road */
#define FAR              1.0f        /* the depths: into the screen, from the screen's own 0 */
#define BEHIND_MENU      0.5f        /* a tilemap's where no road is */
#define VERTICES_QUAD    6
#define SPRITE_SLOTS     2048
#ifndef SPRITE_BYTES
#define SPRITE_BYTES     (10 * 1024 * 1024)
#endif
#define SPRITE_KEEP      3           /* frames a sprite's texture is kept past its last use, at the least */
#define CMDBUF_SIZE      (C3D_DEFAULT_CMDBUF_SIZE * 4)
#define OPAQUE           0xFFFFFFFFu
#define CHANNEL_MOST     31

#define TRANSFER_FLAGS (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct
{
	float x, y, z;
	float u, v;
	uint32_t colour;
} vertex;

/* A strip of a texture on the screen */
typedef struct
{
	int16_t slot;
	int16_t x, y, width, height;
	int16_t column, line;
} strip;

/* A tilemap page as a layer's colours show it */
typedef struct
{
	C3D_Tex texture;
	bool made;
	int page;
	int bank;
	uint16_t palette_base;
	uint8_t shift;
	uint32_t used;
	uint16_t cells[PAGE_CELLS];
	uint32_t versions[PAGE_CELLS];
} page_slot;

typedef struct
{
	C3D_Tex texture;
	uint32_t key;
	uint32_t used;
	bool made;
} sprite_slot;

static C3D_RenderTarget *target, *target_right;
static float eye;                   /* pixels this eye's picture moves at FAR: 0 with the slider down */
static int road_horizon;            /* the row where the road's rows begin, or -1 where there is no road */
static DVLB_s *shader_data;
static shaderProgram_s shader;
static int projection_at;
static C3D_Mtx projection;
static float depth;                 /* of the quads being added */
static vertex *vertices[2];
static int vertex_list, vertex_count;
static uint8_t morton[8][8];
static page_slot pages[PAGE_SLOTS];
static strip strips[QUADS_MOST];
static int strip_count;
static C3D_Tex text_texture, bitmap_texture;
static uint16_t text_cells[TEXT_CELLS];
static uint32_t text_versions[TEXT_CELLS];
static uint8_t text_characters[TEXT_CHARACTERS * 32];
static uint32_t character_versions[TEXT_CHARACTERS];
static uint8_t bitmap_rows[0x20000];
static uint16_t palette_copy[S32_PALETTE_ENTRIES];
static uint32_t palette_versions[PALETTE_BLOCKS];
static uint32_t palette_version;
static sprite_slot sprites[SPRITE_SLOTS];
static uint32_t sprite_bytes;
static uint32_t frame_number;
static uint32_t text_palette_key, bitmap_key, bitmap_palette_seen;

static inline uint16_t texture_colour(uint16_t colour)
{
	/* xBBBBBGGGGGRRRRR to RRRRRGGGGGBBBBBA */
	return (uint16_t)((colour & 0x1f) << 11 | (colour & 0x3e0) << 1 | (colour & 0x7c00) >> 9 | 1);
}

static inline uint32_t vertex_colour(uint16_t colour)
{
	const uint32_t red = (colour & 0x1f) * 255u / CHANNEL_MOST, green = ((colour >> 5) & 0x1f) * 255u / CHANNEL_MOST;
	const uint32_t blue = ((colour >> 10) & 0x1f) * 255u / CHANNEL_MOST;

	return 0xFF000000u | blue << 16 | green << 8 | red;
}

/* The texel at (x, y) from the top left of a texture 'width' x 'height', as the PICA200 keeps it: 8 x 8 blocks from the
   bottom row up, in Morton order */
static inline uint32_t texel_at(int x, int y, int width, int height)
{
	const int flipped = height - 1 - y;

	return (uint32_t)((flipped & ~7) * width + (x & ~7) * 8 + morton[flipped & 7][x & 7]);
}

static int power_of_two_at_least(int value)
{
	int size = 8;

	while (size < value)
		size *= 2;
	return size;
}

static void make_texture(C3D_Tex *texture, int width, int height, GPU_TEXTURE_WRAP_PARAM wrap)
{
	C3D_TexInit(texture, (u16)width, (u16)height, GPU_RGBA5551);
	C3D_TexSetFilter(texture, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(texture, wrap, wrap);
	memset(texture->data, 0, texture->size);
}

/* ---- the palette ---- */

/* Which blocks of 16 colours changed since the last frame drawn */
static void watch_palette(void)
{
	const uint16_t *palette = s32_board.palette[0];

	for (int block = 0; block < PALETTE_BLOCKS; block++)
		if (memcmp(&palette_copy[block * 16], &palette[block * 16], 16 * sizeof(uint16_t)) != 0)
		{
			memcpy(&palette_copy[block * 16], &palette[block * 16], 16 * sizeof(uint16_t));
			palette_versions[block] = ++palette_version;
		}
}

static inline int palette_index(const s32_scene_layer *layer, int pixel)
{
	return (layer->palette_base + ((pixel >> layer->shift) & 0xfff0) + (pixel & 0x0f)) & 0x3fff;
}

/* ---- the tilemap pages ---- */

static void draw_tile(page_slot *slot, const s32_scene_layer *layer, int cell_number, uint16_t cell)
{
	const s32_graphics *graphics = assets_graphics();
	const uint32_t tile = ((uint32_t)slot->bank << 13 | (cell & 0x1fff)) % graphics->tile_count;
	const uint8_t *pens = graphics->tile_pens[tile];
	const int left = (cell_number & 31) * 16, top = (cell_number >> 5) * 16;
	const int index = palette_index(layer, ((cell >> 4) & 0x1ff) << 4);
	const bool flip_x = (cell & 0x4000) != 0, flip_y = (cell & 0x8000) != 0;
	uint16_t *texels = (uint16_t *)slot->texture.data;
	uint16_t colours[16];

	colours[0] = 0;
	for (int pen = 1; pen < 16; pen++)
		colours[pen] = texture_colour(palette_copy[(index & 0x3ff0) + pen]);
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
			texels[texel_at(left + x, top + y, PAGE_WIDTH, PAGE_HEIGHT)] = colours[pens[(flip_y ? 15 - y : y) * 16 + (flip_x ? 15 - x : x)]];
}

static int page_slot_of(int page, int bank, const s32_scene_layer *layer)
{
	int oldest = 0;

	for (int index = 0; index < PAGE_SLOTS; index++)
	{
		const page_slot *slot = &pages[index];

		if (slot->made && slot->page == page && slot->bank == bank && slot->palette_base == layer->palette_base
			&& slot->shift == layer->shift)
			return index;
		if (slot->used < pages[oldest].used)
			oldest = index;
	}
	pages[oldest].page = page;
	pages[oldest].bank = bank;
	pages[oldest].palette_base = layer->palette_base;
	pages[oldest].shift = layer->shift;
	pages[oldest].made = true;
	pages[oldest].used = 0;
	memset(pages[oldest].versions, 0xFF, sizeof(pages[oldest].versions));
	return oldest;
}

/* The page's tiles whose cell or colours changed, once a frame */
static void update_page(int index, const s32_scene_layer *layer)
{
	page_slot *slot = &pages[index];
	const uint16_t *cells = (const uint16_t *)&s32_board.video_ram[(uint32_t)slot->page * 0x400];
	bool changed = false;

	if (slot->used == frame_number)
		return;
	slot->used = frame_number;
	for (int cell_number = 0; cell_number < PAGE_CELLS; cell_number++)
	{
		const uint16_t cell = cells[cell_number];
		const uint32_t version = palette_versions[palette_index(layer, ((cell >> 4) & 0x1ff) << 4) >> 4];

		if (cell == slot->cells[cell_number] && version == slot->versions[cell_number])
			continue;
		slot->cells[cell_number] = cell;
		slot->versions[cell_number] = version;
		draw_tile(slot, layer, cell_number, cell);
		changed = true;
	}
	if (changed)
		C3D_TexFlush(&slot->texture);
}

/* ---- the layers' geometry ---- */

static void add_quad(float x, float y, float width, float height, float u0, float v0, float u1, float v1, uint32_t colour)
{
	vertex *out = vertices[vertex_list] + vertex_count;

	if (vertex_count + VERTICES_QUAD > QUADS_MOST * VERTICES_QUAD)
		return;
	out[0] = (vertex){ x, y, depth, u0, v0, colour };
	out[1] = (vertex){ x, y + height, depth, u0, v1, colour };
	out[2] = (vertex){ x + width, y + height, depth, u1, v1, colour };
	out[3] = out[2];
	out[4] = (vertex){ x + width, y, depth, u1, v0, colour };
	out[5] = out[0];
	vertex_count += VERTICES_QUAD;
}

static void draw_added(int first)
{
	if (vertex_count > first)
		C3D_DrawArrays(GPU_TRIANGLES, first, vertex_count - first);
}

/* The colours a layer adds to or takes from its own, and its texture or none */
static void set_shading(const int8_t *offsets, bool textured)
{
	C3D_TexEnv *stage = C3D_GetTexEnv(0);
	uint32_t added = 0, taken = 0;

	C3D_TexEnvInit(stage);
	if (textured)
	{
		C3D_TexEnvSrc(stage, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
		C3D_TexEnvFunc(stage, C3D_Both, GPU_MODULATE);
	}
	else
	{
		C3D_TexEnvSrc(stage, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
		C3D_TexEnvFunc(stage, C3D_Both, GPU_REPLACE);
	}
	for (int channel = 0; channel < 3; channel++)
	{
		const int amount = (offsets ? offsets[channel] : 0) * 255 / CHANNEL_MOST;

		if (amount > 0)
			added |= (uint32_t)(amount > 255 ? 255 : amount) << (8 * channel);
		else
			taken |= (uint32_t)(amount < -255 ? 255 : -amount) << (8 * channel);
	}
	stage = C3D_GetTexEnv(1);
	C3D_TexEnvInit(stage);
	if (added)
	{
		C3D_TexEnvSrc(stage, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, 0);
		C3D_TexEnvFunc(stage, C3D_RGB, GPU_ADD);
		C3D_TexEnvColor(stage, added | 0xFF000000u);
	}
	stage = C3D_GetTexEnv(2);
	C3D_TexEnvInit(stage);
	if (taken)
	{
		C3D_TexEnvSrc(stage, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, 0);
		C3D_TexEnvFunc(stage, C3D_RGB, GPU_SUBTRACT);
		C3D_TexEnvColor(stage, taken | 0xFF000000u);
	}
}

static void set_blending(const s32_scene *scene, bool blended)
{
	if (blended)
	{
		const uint32_t part = (uint32_t)(7 - scene->blend_factor) * 255u / 8u;

		C3D_BlendingColor(part << 24);
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_CONSTANT_ALPHA, GPU_ONE_MINUS_CONSTANT_ALPHA, GPU_ONE, GPU_ZERO);
	}
	else
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
}

static void add_strip(int slot, int x, int y, int width, int height, int column, int line)
{
	if (strip_count < QUADS_MOST)
		strips[strip_count++] = (strip){ (int16_t)slot, (int16_t)x, (int16_t)y, (int16_t)width, (int16_t)height, (int16_t)column, (int16_t)line };
}

/* ---- the depths ---- */

/* The road: the tilemap whose rows each pick their source, its rows from the horizon down */
static void find_road(const s32_scene *scene)
{
	road_horizon = -1;
	for (int number = 0; number < 4; number++)
	{
		const s32_scene_tilemap *map = &scene->tilemaps[number];
		int picked = 0, first = -1;

		if (!scene->drawn[S32_SCENE_NBG0 + number])
			continue;
		for (int y = 1; y < S32_VIDEO_HEIGHT; y++)
			if (map->rows[y].line != map->rows[y - 1].line + 1 && map->rows[y].line != 0)
			{
				picked++;
				if (first < 0)
					first = y;
			}
		if (picked >= ROAD_ROWS_LEAST)
			road_horizon = first;
	}
}

/* How deep the road is at a row: far at the horizon, at the screen at the bottom; perspective's 1 / distance */
static float road_depth(int y)
{
	const float below = (float)(y - road_horizon) / (float)(S32_VIDEO_HEIGHT - road_horizon);

	if (y <= road_horizon)
		return FAR;
	return below >= 1.0f ? 0.0f : FAR * (1.0f - below) * (1.0f - below);
}

/* A row of a tilemap's depth, as this eye moves it */
static float tilemap_shift(int y)
{
	if (eye == 0.0f)
		return 0.0f;
	return eye * (road_horizon >= 0 ? road_depth(y) : BEHIND_MENU);
}

/* The tilemap's rows as strips, the rows that go on from each other as one; then each page's strips together */
static void draw_tilemap(const s32_scene *scene, const s32_scene_layer *layer)
{
	const s32_scene_tilemap *map = &scene->tilemaps[layer->index - S32_SCENE_NBG0];
	const s32_scene_clip *clip = &scene->clips[layer->index];
	const int left = (scene->width - SCREEN_WIDTH) / 2;
	bool page_seen[PAGE_SLOTS];

	memset(page_seen, 0, sizeof(page_seen));
	strip_count = 0;
	for (int y = 0; y < S32_VIDEO_HEIGHT;)
	{
		const s32_scene_row *row = &map->rows[y];
		const s32_scene_clip_row *windows = &clip->rows[y];
		int height = 1;

		while (y + height < S32_VIDEO_HEIGHT)
		{
			const s32_scene_row *next = &map->rows[y + height];

			if (next->column != row->column || next->line != row->line + height || next->pages[0] != row->pages[0]
				|| next->pages[1] != row->pages[1] || row->line + height >= PAGE_HEIGHT
				|| memcmp(&clip->rows[y + height], windows, sizeof(*windows)) != 0
				|| tilemap_shift(y + height) != tilemap_shift(y))
				break;
			height++;
		}
		for (int span = 0; span < windows->count; span++)
		{
			const int edge = eye != 0.0f ? EDGE : 0;
			const int reach = SCREEN_WIDTH + edge;
			int x = windows->spans[span][0] - left;
			const int end = windows->spans[span][1] - left < reach ? windows->spans[span][1] - left : reach;
			int column;

			if (x < -edge)
				x = -edge;
			column = row->column + left + x;
			while (x < end)
			{
				const int at = column & 0x1ff;
				const int page = row->pages[(column >> 9) & 1];
				const int width = PAGE_WIDTH - at < end - x ? PAGE_WIDTH - at : end - x;
				const int slot = page_slot_of(page, map->bank, layer);

				update_page(slot, layer);
				page_seen[slot] = true;
				add_strip(slot, x, y + TOP, width, height, at, row->line);
				x += width;
				column += width;
			}
		}
		y += height;
	}
	for (int slot = 0; slot < PAGE_SLOTS; slot++)
	{
		const int first = vertex_count;

		if (!page_seen[slot])
			continue;
		for (int index = 0; index < strip_count; index++)
		{
			const strip *it = &strips[index];

			if (it->slot == slot)
				add_quad((float)it->x + tilemap_shift(it->y - TOP), it->y, it->width, it->height, (float)it->column / PAGE_WIDTH,
					(float)it->line / PAGE_HEIGHT, (float)(it->column + it->width) / PAGE_WIDTH,
					(float)(it->line + it->height) / PAGE_HEIGHT, OPAQUE);
		}
		C3D_TexBind(0, &pages[slot].texture);
		draw_added(first);
	}
}

/* ---- the text layer ---- */

static void draw_text_cell(const s32_scene *scene, const s32_scene_layer *layer, int cell_number, uint16_t cell)
{
	const uint32_t character = (uint32_t)(scene->text_control & 7) << 9 | (cell & 0x1ff);
	const uint8_t *rows = &s32_board.video_ram[(character * 32) & (S32_VIDEO_RAM_SIZE - 1)];
	const int index = palette_index(layer, ((cell >> 9) & 0x7f) << 4);
	const int left = (cell_number & 63) * 8, top = (cell_number >> 6) * 8;
	uint16_t *texels = (uint16_t *)text_texture.data;

	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
		{
			const uint8_t value = rows[y * 4 + x / 2];
			const int pen = (x & 1) ? (value & 0x0f) : (value >> 4);

			texels[texel_at(left + x, top + y, PAGE_WIDTH, PAGE_HEIGHT)] = pen ? texture_colour(palette_copy[(index & 0x3ff0) + pen]) : 0;
		}
}

static void draw_text(const s32_scene *scene, const s32_scene_layer *layer)
{
	const uint16_t *cells = (const uint16_t *)&s32_board.video_ram[((uint32_t)(scene->text_control >> 4) & 0x1f) << 12];
	const uint8_t *characters = &s32_board.video_ram[(uint32_t)(scene->text_control & 7) << 14];
	const uint32_t key = (uint32_t)layer->palette_base << 8 | layer->shift << 4 | (scene->text_control & 7);
	const int left = (scene->width - SCREEN_WIDTH) / 2, shown = scene->width - 2 * scene->side;
	const int first = vertex_count;
	bool changed = false;

	if (key != text_palette_key)
	{
		text_palette_key = key;
		memset(text_versions, 0xFF, sizeof(text_versions));
	}
	for (int character = 0; character < TEXT_CHARACTERS; character++)
		if (memcmp(&text_characters[character * 32], &characters[character * 32], 32) != 0)
		{
			memcpy(&text_characters[character * 32], &characters[character * 32], 32);
			character_versions[character] = ++palette_version;
		}
	for (int cell_number = 0; cell_number < TEXT_CELLS; cell_number++)
	{
		const uint16_t cell = cells[cell_number];
		const uint32_t version = palette_versions[palette_index(layer, ((cell >> 9) & 0x7f) << 4) >> 4]
			+ character_versions[cell & 0x1ff];

		if (cell == text_cells[cell_number] && version == text_versions[cell_number])
			continue;
		text_cells[cell_number] = cell;
		text_versions[cell_number] = version;
		draw_text_cell(scene, layer, cell_number, cell);
		changed = true;
	}
	if (changed)
		C3D_TexFlush(&text_texture);
	add_quad((float)(scene->side - left), TOP, (float)shown, S32_VIDEO_HEIGHT, 0.0f, 0.0f, (float)shown / PAGE_WIDTH,
		(float)S32_VIDEO_HEIGHT / PAGE_HEIGHT, OPAQUE);
	C3D_TexBind(0, &text_texture);
	draw_added(first);
}

/* ---- the bitmap layer ---- */

static void draw_bitmap(const s32_scene *scene, const s32_scene_layer *layer)
{
	const uint32_t key = (uint32_t)layer->palette_base << 8 | layer->shift << 4 | (uint32_t)scene->bitmap_bits8;
	const s32_scene_clip *clip = &scene->clips[S32_SCENE_BITMAP];
	const int row_bytes = scene->bitmap_bits8 ? 512 : 256;
	const int rows = scene->bitmap_bits8 ? PAGE_HEIGHT : BITMAP_HEIGHT;
	const int left = (scene->width - SCREEN_WIDTH) / 2;
	const int first = vertex_count;
	const bool all = key != bitmap_key || bitmap_palette_seen != palette_version;
	uint16_t *texels = (uint16_t *)bitmap_texture.data;
	bool any_shown = false;
	bool changed = false;

	for (int y = 0; y < S32_VIDEO_HEIGHT && !any_shown; y++)
		any_shown = clip->rows[y].count != 0;
	if (!any_shown)
		return;    /* (its clip window shuts it, and video RAM's start is the tilemaps': nothing to make) */
	bitmap_key = key;
	bitmap_palette_seen = palette_version;
	for (int y = 0; y < rows; y++)
	{
		const uint8_t *row = &s32_board.video_ram[(uint32_t)y * (uint32_t)row_bytes];

		if (!all && memcmp(&bitmap_rows[y * row_bytes], row, (size_t)row_bytes) == 0)
			continue;
		memcpy(&bitmap_rows[y * row_bytes], row, (size_t)row_bytes);
		changed = true;
		for (int x = 0; x < PAGE_WIDTH; x++)
		{
			uint32_t pixel;
			bool shown;

			if (scene->bitmap_bits8)
			{
				pixel = row[x] + scene->bitmap_color;
				shown = (pixel & 0xff) != 0;
			}
			else
			{
				pixel = (uint32_t)((row[x / 2] >> (4 * (x & 1))) & 0x0f) + scene->bitmap_color;
				shown = (pixel & 0x0f) != 0;
			}
			texels[texel_at(x, y, PAGE_WIDTH, BITMAP_HEIGHT)] = shown ? texture_colour(palette_copy[palette_index(layer, (int)pixel)]) : 0;
		}
	}
	if (changed)
		C3D_TexFlush(&bitmap_texture);
	for (int y = 0; y < S32_VIDEO_HEIGHT; y++)
	{
		const int line = (y + scene->bitmap_y) & (rows - 1);

		for (int span = 0; span < clip->rows[y].count; span++)
		{
			const int x = clip->rows[y].spans[span][0] - left < 0 ? 0 : clip->rows[y].spans[span][0] - left;
			const int end = clip->rows[y].spans[span][1] - left < SCREEN_WIDTH ? clip->rows[y].spans[span][1] - left : SCREEN_WIDTH;
			const float column = (float)(x + left - scene->side + scene->bitmap_x);

			if (x < end)
				add_quad((float)x, (float)(y + TOP), (float)(end - x), 1.0f, column / PAGE_WIDTH, (float)line / BITMAP_HEIGHT,
					(column + (float)(end - x)) / PAGE_WIDTH, (float)(line + 1) / BITMAP_HEIGHT, OPAQUE);
		}
	}
	C3D_TexBind(0, &bitmap_texture);
	draw_added(first);
}

/* ---- the sprites ---- */

/* The colour each pen of the sprite gives, 0 where it is transparent, and their hash. The pens that are the mixer's
   shadow have SHADOW_PEN instead: a sprite's texture has none of them, and its shadow's has only them */
#define SAMPLE_BACK 0.49f
#define SHADOW_PEN 0xFFFEu
#define SHADOW_KEY 0x9E3779B9u

static uint32_t sprite_colours(const s32_scene *scene, const s32_scene_sprite *sprite, uint16_t *colours, bool *shadowed, int *group)
{
	const int count = sprite->bits8 ? 256 : 16;
	uint32_t hash = sprite->address * 2654435761u ^ (uint32_t)sprite->bank << 24 ^ (uint32_t)sprite->words << 16
		^ (uint32_t)sprite->rows << 8 ^ sprite->end_pen ^ (sprite->bits8 ? 0x80000000u : 0u) ^ (sprite->shadow ? 0x40000000u : 0u);

	if (sprite->from_ram)
		for (int word = 0; word < sprite->words * sprite->rows; word++)
			hash = (hash ^ s32_video_scene_sprite_word(sprite, sprite->address + (uint32_t)word)) * 16777619u;
	for (int pen = 0; pen < count; pen++)
	{
		int pixel;
		bool shown;

		if (!sprite->indirect)
		{
			pixel = sprite->color | pen;
			shown = pen != 0;
		}
		else
		{
			pixel = sprite->bits8 ? sprite->table[pen >> 4] | (pen & 0x0f) : sprite->table[pen];
			shown = (pixel & sprite->transparent_bits) != sprite->transparent_bits;
		}
		shown = shown && (sprite->shadow || (pixel & 0x7fff) != 0x7fff);
		if (shown && *group < 0)
			*group = (pixel >> scene->group_shift) & scene->group_mask;    /* (a pen that shows: all of a sprite's are one's) */
		if (shown && (sprite->shadow || ((pixel & scene->sprite_bits) & 0x7ffe) == scene->shadow_pen))
		{
			colours[pen] = SHADOW_PEN;
			*shadowed = true;
		}
		else
			colours[pen] = shown ? texture_colour(s32_video_scene_sprite_color(sprite, (uint16_t)pixel)) : 0;
		hash = (hash ^ colours[pen]) * 16777619u;
	}
	return hash;
}

/* The rows of the sprite's source, as the board reads them: words of pens, the first and last of a word left out where
   they are the end code, and a word that ends with it ends the row */
static void decode_sprite(const s32_scene_sprite *sprite, const uint16_t *colours, C3D_Tex *texture, bool shadow)
{
	const int bits = sprite->bits8 ? 8 : 4;
	const int pens = 32 / bits;
	const int pen_mask = (1 << bits) - 1;
	uint16_t *texels = (uint16_t *)texture->data;

	for (int row = 0; row < sprite->rows; row++)
	{
		uint32_t address = sprite->address + (uint32_t)row * sprite->words;
		int x = 0;

		for (int word_number = 0; word_number < sprite->words; word_number++)
		{
			const uint32_t word = s32_video_scene_sprite_word(sprite, address++);
			int pen = 0;

			for (int index = 0; index < pens; index++, x++)
			{
				const int skipped_pen = (index == 0 || index == pens - 1) ? sprite->end_pen : 0;

				pen = (int)(word >> (32 - bits * (index + 1))) & pen_mask;
				if (pen != skipped_pen)
				{
					const uint16_t colour = colours[pen];

					if (shadow ? colour == SHADOW_PEN : colour != SHADOW_PEN)
						texels[texel_at(x, row, texture->width, texture->height)] = shadow ? 1 : colour;    /* (opaque) */
				}
			}
			if (sprite->end_pen != 0 && pen == pen_mask)
				break;
		}
	}
	C3D_TexFlush(texture);
}

/* Room for 'bytes' more: the textures unused longest go */
static void free_sprites(uint32_t bytes)
{
	while (sprite_bytes + bytes > SPRITE_BYTES)
	{
		int oldest = -1;

		for (int index = 0; index < SPRITE_SLOTS; index++)
			if (sprites[index].made && (oldest < 0 || sprites[index].used < sprites[oldest].used))
				oldest = index;
		if (oldest < 0 || sprites[oldest].used + SPRITE_KEEP > frame_number)
			return;
		sprite_bytes -= sprites[oldest].texture.size;
		C3D_TexDelete(&sprites[oldest].texture);
		sprites[oldest].made = false;
	}
}

/* The texture of the sprite's colours, or of its shadow */
static sprite_slot *sprite_texture(const s32_scene_sprite *sprite, uint32_t key, const uint16_t *colours, bool shadow)
{
	const int source_width = sprite->words * (sprite->bits8 ? 4 : 8);
	const int width = power_of_two_at_least(source_width), height = power_of_two_at_least(sprite->rows);
	sprite_slot *slot = NULL;

	for (int probe = 0; probe < 32; probe++)
	{
		sprite_slot *here = &sprites[(key + (uint32_t)probe) % SPRITE_SLOTS];

		if (here->made && here->key == key)
		{
			here->used = frame_number;
			return here;
		}
		if (!here->made && !slot)
			slot = here;
	}
	if (!slot)
		return NULL;
	free_sprites((uint32_t)width * (uint32_t)height * 2u);
	if (sprite_bytes + (uint32_t)width * (uint32_t)height * 2u > SPRITE_BYTES || !C3D_TexInit(&slot->texture, (u16)width, (u16)height, GPU_RGBA5551))
		return NULL;
	C3D_TexSetFilter(&slot->texture, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&slot->texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	memset(slot->texture.data, 0, slot->texture.size);
	decode_sprite(sprite, colours, &slot->texture, shadow);
	sprite_bytes += slot->texture.size;
	slot->key = key;
	slot->used = frame_number;
	slot->made = true;
	return slot;
}

/* A sprite the board scales stands on the road at its feet; one at its own size (the car, the HUD, the words) is at the
   screen */
static float sprite_shift(const s32_scene_sprite *sprite)
{
	const bool scaled = sprite->width != sprite->words * (sprite->bits8 ? 4 : 8) || sprite->height != sprite->rows;
	const int feet = sprite->y + sprite->height;

	if (eye == 0.0f || road_horizon < 0 || !scaled)
		return 0.0f;
	return eye * road_depth(feet >= S32_VIDEO_HEIGHT ? S32_VIDEO_HEIGHT - 1 : feet);
}

/* The sprite's texture on the screen, inside its clip */
static void add_sprite(const s32_scene *scene, const s32_scene_sprite *sprite, const sprite_slot *slot)
{
	const int left = (scene->width - SCREEN_WIDTH) / 2;
	const float texture_right = (float)(sprite->words * (sprite->bits8 ? 4 : 8)) / slot->texture.width;
	const float texture_bottom = (float)sprite->rows / slot->texture.height;
	const float across = texture_right / sprite->width, down = texture_bottom / sprite->height;
	const int x0 = sprite->x > sprite->clip_left ? sprite->x : sprite->clip_left;
	const int y0 = sprite->y > sprite->clip_top ? sprite->y : sprite->clip_top;
	const int x1 = sprite->x + sprite->width - 1 < sprite->clip_right ? sprite->x + sprite->width - 1 : sprite->clip_right;
	const int y1 = sprite->y + sprite->height - 1 < sprite->clip_bottom ? sprite->y + sprite->height - 1 : sprite->clip_bottom;
	/* (the board takes a screen pixel's source at its left and top edges, the GPU at its middle) */
	float u0 = ((float)(x0 - sprite->x) - SAMPLE_BACK) * across, u1 = ((float)(x1 + 1 - sprite->x) - SAMPLE_BACK) * across;
	float v0 = ((float)(y0 - sprite->y) - SAMPLE_BACK) * down, v1 = ((float)(y1 + 1 - sprite->y) - SAMPLE_BACK) * down;

	if (x1 < x0 || y1 < y0)
		return;
	if (sprite->flip_x)
	{
		const float was = u0;

		u0 = texture_right - u1;
		u1 = texture_right - was;
		{
			const float swap = u0;

			u0 = u1;
			u1 = swap;
		}
	}
	if (sprite->flip_y)
	{
		const float was = v0;

		v0 = texture_bottom - v1;
		v1 = texture_bottom - was;
		{
			const float swap = v0;

			v0 = v1;
			v1 = swap;
		}
	}
	add_quad((float)(x0 - left) + sprite_shift(sprite), (float)(y0 + TOP), (float)(x1 + 1 - x0), (float)(y1 + 1 - y0), u0, v0, u1, v1, OPAQUE);
}

/* The depth of the layer 'above' layers from the top: over the backdrop, under nothing; a sprite with that many over
   it goes half a step nearer */
static float layer_depth(const s32_scene *scene, float above)
{
	return ((float)scene->layer_count - above) / ((float)scene->layer_count + 2.0f);
}

/* The sprites in the list's order */
static void draw_sprites(const s32_scene *scene)
{
	for (int index = 0; index < scene->sprite_count; index++)
	{
		const s32_scene_sprite *sprite = &scene->sprites[index];
		const int first = vertex_count;
		static uint16_t colours[256];
		const sprite_slot *slot;
		bool shadowed = false;
		int group = -1;
		uint32_t key;

		key = sprite_colours(scene, sprite, colours, &shadowed, &group);
		/* (a group the mixer puts behind the backdrop never shows: the game hides sprites so) */
		if (group < 0 || scene->sprites_above[group] >= scene->layer_count)
			continue;
		depth = layer_depth(scene, (float)scene->sprites_above[group] - 0.5f);
		if ((slot = sprite_texture(sprite, key, colours, false)) != NULL)
		{
			if (sprite->ghost)
			{
				C3D_BlendingColor(0x7F000000u);
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_CONSTANT_ALPHA, GPU_ONE_MINUS_CONSTANT_ALPHA, GPU_ONE, GPU_ZERO);
			}
			add_sprite(scene, sprite, slot);
			C3D_TexBind(0, &slot->texture);
			draw_added(first);
			if (sprite->ghost)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
		}
		if (shadowed && (slot = sprite_texture(sprite, key ^ SHADOW_KEY, colours, true)) != NULL)
		{
			const int shadow_first = vertex_count;

			add_sprite(scene, sprite, slot);
			C3D_TexBind(0, &slot->texture);
			C3D_BlendingColor(0x7F000000u);
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ZERO, GPU_CONSTANT_ALPHA, GPU_ONE, GPU_ZERO);
			draw_added(shadow_first);
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
		}
	}
}

/* ---- the frame ---- */

static void draw_layer(const s32_scene *scene, const s32_scene_layer *layer)
{
	set_shading(layer->offsets, true);
	set_blending(scene, layer->blended);
	if (layer->index >= S32_SCENE_NBG0 && layer->index < S32_SCENE_BITMAP)
		draw_tilemap(scene, layer);
	else if (layer->index == S32_SCENE_BITMAP)
		draw_bitmap(scene, layer);
	else
		draw_text(scene, layer);
}

static void draw_backdrop(const s32_scene *scene)
{
	const s32_scene_layer *back = &scene->layers[scene->layer_count - 1];
	const int first = vertex_count;

	depth = layer_depth(scene, (float)(scene->layer_count - 1));
	set_shading(back->offsets, false);
	set_blending(scene, false);
	for (int y = 0; y < S32_VIDEO_HEIGHT;)
	{
		int height = 1;

		while (y + height < S32_VIDEO_HEIGHT && scene->backdrop[y + height] == scene->backdrop[y])
			height++;
		add_quad(0.0f, (float)(y + TOP), SCREEN_WIDTH, (float)height, 0.0f, 0.0f, 0.0f, 0.0f, vertex_colour(scene->backdrop[y]));
		y += height;
	}
	draw_added(first);
}

/* The scene for one eye */
static void draw_eye(const s32_scene *scene, C3D_RenderTarget *on)
{
	C3D_RenderTargetClear(on, C3D_CLEAR_ALL, 0x000000FF, 0);
	C3D_FrameDrawOn(on);
	if (!scene->on)
		return;
	draw_backdrop(scene);
	for (int index = scene->layer_count - 2; index >= 0; index--)
	{
		depth = layer_depth(scene, (float)index);
		draw_layer(scene, &scene->layers[index]);
	}
	set_shading(scene->sprite_offsets, true);
	set_blending(scene, false);
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR);
	draw_sprites(scene);
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
}

void gpu_draw(const s32_scene *scene)
{
	const float slider = osGet3DSliderState();
	C3D_BufInfo *buffers;

	if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK))
		return;
	frame_number++;
	vertex_list ^= 1;
	vertex_count = 0;
	buffers = C3D_GetBufInfo();
	BufInfo_Init(buffers);
	BufInfo_Add(buffers, vertices[vertex_list], sizeof(vertex), 3, 0x210);
	gfxSet3D(slider > 0.0f);
	if (scene->on)
	{
		watch_palette();
		find_road(scene);
	}
	/* (into the screen: what is far moves left for the left eye) */
	eye = -slider * EYE_MOST;
	draw_eye(scene, target);
	if (slider > 0.0f)
	{
		eye = slider * EYE_MOST;
		draw_eye(scene, target_right);
	}
	C3D_FrameEnd(0);
}

void gpu_open(void)
{
	C3D_AttrInfo *attributes;

	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
			morton[y][x] = (uint8_t)((x & 1) | (y & 1) << 1 | (x & 2) << 1 | (y & 2) << 2 | (x & 4) << 2 | (y & 4) << 3);
	C3D_Init(CMDBUF_SIZE);
	target = C3D_RenderTargetCreate(SCREEN_HEIGHT, SCREEN_WIDTH, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, TRANSFER_FLAGS);
	target_right = C3D_RenderTargetCreate(SCREEN_HEIGHT, SCREEN_WIDTH, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(target_right, GFX_TOP, GFX_RIGHT, TRANSFER_FLAGS);
	shader_data = DVLB_ParseFile((u32 *)outrunners_shader_shbin, outrunners_shader_shbin_size);
	shaderProgramInit(&shader);
	shaderProgramSetVsh(&shader, &shader_data->DVLE[0]);
	C3D_BindProgram(&shader);
	projection_at = shaderInstanceGetUniformLocation(shader.vertexShader, "projection");
	Mtx_OrthoTilt(&projection, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, -1.0f, true);    /* (citro2d's: nearer, more) */
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_at, &projection);
	attributes = C3D_GetAttrInfo();
	AttrInfo_Init(attributes);
	AttrInfo_AddLoader(attributes, 0, GPU_FLOAT, 3);
	AttrInfo_AddLoader(attributes, 1, GPU_FLOAT, 2);
	AttrInfo_AddLoader(attributes, 2, GPU_UNSIGNED_BYTE, 4);
	vertices[0] = linearAlloc(QUADS_MOST * VERTICES_QUAD * sizeof(vertex));
	vertices[1] = linearAlloc(QUADS_MOST * VERTICES_QUAD * sizeof(vertex));
	for (int index = 0; index < PAGE_SLOTS; index++)
		make_texture(&pages[index].texture, PAGE_WIDTH, PAGE_HEIGHT, GPU_CLAMP_TO_EDGE);
	make_texture(&text_texture, PAGE_WIDTH, PAGE_HEIGHT, GPU_CLAMP_TO_EDGE);
	make_texture(&bitmap_texture, PAGE_WIDTH, BITMAP_HEIGHT, GPU_REPEAT);
	memset(text_versions, 0xFF, sizeof(text_versions));
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_AlphaTest(true, GPU_GREATER, 0);
	C3D_SetScissor(GPU_SCISSOR_NORMAL, TOP, 0, SCREEN_HEIGHT - TOP, SCREEN_WIDTH);
}

void gpu_close(void)
{
	C3D_Fini();
}
