/**
 *    Name:        video.h
 *    Description: Sega System Multi 32 video: draws each frame of the board's two screens from its memories,
 *                 as the hardware does
 *    Version:     0.1.0
 *    Created:     13-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The video chips of the board as a library: the four tilemaps (NBG0-3), the text layer, the bitmap
 *    layer and the backdrop the two screens share, each screen's two sprite frame buffers, mixer and
 *    palette. s32_video_init() hooks it to the board: the sprite list is drawn into the hidden buffers when
 *    the board sends it, and the frames are composed when VBLANK starts, from video RAM, sprite RAM, the
 *    palettes and the mixers as they are then.
 *
 *    A port shows s32_video_frame() of each screen and nothing else of the board's video: 320 or 416 x 224
 *    pixels, 0x00RRGGBB, five bits a channel in the top bits of each byte.
 */
#ifndef S32_VIDEO_H
#define S32_VIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S32_VIDEO_MAX_WIDTH  496    /* (320 and WIDE's most at each side: S32_VIDEO_WIDE_SIDE_MOST) */
#define S32_VIDEO_HEIGHT     224

/* The board's graphics as the video draws them: every tile's pens by its number (16 rows of 16, a byte a pen)
   and the 32-bit word of sprite graphics at a place of the sprite chips (bank << 20 | offset, in words): the
   game's assets give them (game/assets.h: its tilesets and sprite sheets) */
typedef struct
{
	const uint8_t *const *tile_pens;
	uint32_t tile_count;
	uint32_t (*sprite_word)(uint32_t place);
	uint32_t sprite_banks;                  /* of 1M words */
} s32_graphics;

/* Hooks the video to s32_board, drawing with 'graphics' (kept by the caller) */
void s32_video_init_graphics(const s32_graphics *graphics);

/* The same from the chips' images, for the tools that read the ROM set (the reference, the scaffold): the
   tile ROM (16x16 tiles, 128 bytes each) and the sprite ROMs (32-bit words, big endian), as game/data.h
   holds them; the video takes the tiles' pens out of the image and reads the sprites' words in it */
void s32_video_init(const uint8_t *tiles, size_t tiles_size, const uint8_t *sprites, size_t sprites_size);

/* What the board calls (s32_video_init sets them): the sprite commands, and the start of VBLANK */
void s32_video_sprites(bool erase, bool draw);
void s32_video_draw_frame(void);

/* The last frame of screen 0 (left) or 1 (right): S32_VIDEO_MAX_WIDTH pixels a row, S32_VIDEO_HEIGHT rows;
   its width is 320 or 416 */
const uint32_t *s32_video_frame(int screen, int *width);

/* The screens a port shows (bit 0 the left, bit 1 the right; both unless it says otherwise, as the headless
   runs and their hashes have them): the video draws those alone, their sprites, the layers their mixers
   take and their mixing, and a screen left out keeps its last frame */
void s32_video_show_screens(int screens);

/* FNV-1a hash of the last frames' visible pixels (red, green, blue bytes), the left screen's then the
   right one's, as tools/core/system32/reference/s32ref writes it */
uint64_t s32_video_frame_hash(void);

/* WIDE (a port's console build): frames wider than the 320 pixels the board shows, the game's picture in the
   middle and 'side' more of it at each side (S32_VIDEO_WIDE_SIDE_16_9 for 16:9's 416, at most
   S32_VIDEO_WIDE_SIDE_MOST for 20:9's 496): the tilemaps and the bitmap go on, the sprites are drawn where
   they are (the game places them well past its edges), the text layer stays where it is (and is clear at
   the sides), the clip windows that cover the screen cover the wider one. 0, the default, the frames are
   the board's */
#define S32_VIDEO_WIDE_SIDE_16_9 48
#define S32_VIDEO_WIDE_SIDE_20_9 88
#define S32_VIDEO_WIDE_SIDE_MOST S32_VIDEO_WIDE_SIDE_20_9
void s32_video_set_wide_side(int side);

/* This frame's: WIDE's side, else 0 */
int s32_video_side(void);

/* The extra palette (a port's console build): colours of each screen's beside the board's palette, by the
   same indexes and in its format (xBBBBBGGGGGRRRRR), for sprites whose second word has
   S32_SPRITE_EXTRA_PALETTE set (a bit the board does not read): their pixels take the colours set here, and
   the board's where none is. Off, the default, the bit is let alone as the board does */
#define S32_SPRITE_EXTRA_PALETTE 0x0080
void s32_video_use_extra_palette(bool used);
void s32_video_extra_colour(int screen, uint32_t index, uint16_t colour);

/* Ghosts (a port's console build, with the extra palette on): sprites whose colour word (the eighth) has
   S32_SPRITE_GHOST set, a bit the board does not read, are drawn see-through: each pixel half its own colour
   and half the colour of what is under it */
#define S32_SPRITE_GHOST 0x8000

/* A port's own sprites over the frame shown (the console's pause, in the game's letters): 'list' is entries
   as sprite RAM holds them (16 bytes each, 16-bit words in its order, an end command last), drawn over the
   game's in the sprites shown, for s32_video_draw_frame to mix; s32_video_overlay_end puts the game's back
   as they were */
void s32_video_overlay_sprites(const uint8_t *list, size_t size);
void s32_video_overlay_end(void);

#ifdef S32_VIDEO_SCENE
/* A frame as a GPU can draw it (ports/3ds): what the mixer and the layers would draw from, instead of the frame's
   pixels. The sprites' list is kept, not drawn, when the board sends it. Only what OutRunners uses of the board is
   here: no clip windows and no shadows, a group for each sprite and one blend for each layer */
#define S32_SCENE_LAYERS   8
#define S32_SCENE_SPRITES  1024
#define S32_SCENE_RAM_WORDS 8
#define S32_SCENE_TEXT     0        /* a layer's index, as the mixer's order */
#define S32_SCENE_NBG0     1
#define S32_SCENE_BITMAP   5
#define S32_SCENE_SPRITE   6
#define S32_SCENE_BACKDROP 7

typedef struct
{
	uint8_t index;                  /* S32_SCENE_ */
	bool blended;                   /* half seen through to what is under it */
	uint8_t shift;                  /* of a pixel, before the palette base is added */
	uint16_t palette_base;
	int8_t offsets[3];              /* added to its red, green, blue: 5 bits a channel */
} s32_scene_layer;

/* A row of a tilemap: the pages of its source, the source row and the column at the screen's left edge */
typedef struct
{
	int16_t pages[2];
	int16_t line;
	uint16_t column;
} s32_scene_row;

/* The columns of each row that a layer's clip windows let through, from the frame's left */
#define S32_SCENE_SPANS 4
typedef struct
{
	uint8_t count;
	uint16_t spans[S32_SCENE_SPANS][2];      /* first column, one past the last */
} s32_scene_clip_row;

typedef struct
{
	s32_scene_clip_row rows[S32_VIDEO_HEIGHT];
} s32_scene_clip;

typedef struct
{
	int bank;
	s32_scene_row rows[S32_VIDEO_HEIGHT];
} s32_scene_tilemap;

typedef struct
{
	int16_t x, y;                   /* its top left corner on the screen */
	int16_t width, height;          /* there */
	int16_t clip_left, clip_right, clip_top, clip_bottom;    /* the frame's area it shows in, edges included */
	uint32_t address;
	uint8_t bank;
	uint8_t words;                  /* a row's, in the source */
	uint8_t rows;
	uint8_t end_pen;
	bool bits8, flip_x, flip_y, indirect, from_ram;
	uint8_t screen;                 /* the monitor it goes to */
	bool shadow;                    /* it darkens what is under it instead */
	bool extra_colours;             /* its pixels take the extra palette's colours, where it has them */
	bool ghost;                     /* half seen through */
	uint16_t color;
	uint16_t transparent_bits;
	uint16_t table[16];
	uint32_t ram_words[S32_SCENE_RAM_WORDS];    /* from_ram's graphics, as they were when the list went out */
} s32_scene_sprite;

typedef struct
{
	int screen;
	bool on;                        /* the screen's on */
	int width;                      /* of the frame: 320, 416 or WIDE's */
	int side;                       /* WIDE's columns at each side */
	int blend_factor;               /* under a blended layer: of 8, the part of it that shows, plus one */
	int layer_count;
	s32_scene_layer layers[S32_SCENE_LAYERS];    /* from the top down, the backdrop last, without the sprites */
	uint8_t sprites_above[S32_SCENE_LAYERS];     /* by sprite group: how many layers are over it */
	uint8_t group_shift, group_mask;
	int8_t sprite_offsets[3];       /* as a layer's */
	uint16_t sprite_bits;           /* of a sprite's pixel, under its group */
	uint16_t shadow_pen;            /* such a pixel darkens the layers under it, at half */
	bool drawn[S32_SCENE_BACKDROP];              /* by layer */
	s32_scene_tilemap tilemaps[4];
	s32_scene_clip clips[S32_SCENE_BACKDROP];    /* by layer */
	uint16_t text_control;
	uint32_t bitmap_color;
	int bitmap_x, bitmap_y;
	bool bitmap_bits8;
	uint16_t backdrop[S32_VIDEO_HEIGHT];         /* the colour of each row, as the palette has it */
	const s32_scene_sprite *sprites;
	int sprite_count;
} s32_scene;

/* From now on the sprites' lists are kept for s32_video_scene */
void s32_video_use_scene(void);

/* While true the sprites are drawn into the buffers too, for s32_video_draw_frame to show beside the scene: checks */
extern bool s32_video_scene_raster;

/* This frame of a monitor: 'screen', 0 or 1. Its sprites are the list's whose 'screen' it is */
void s32_video_scene(s32_scene *scene, int screen);

/* The colour of a pixel of a kept sprite, as the mixer would look it up */
uint16_t s32_video_scene_sprite_color(const s32_scene_sprite *sprite, uint16_t pixel);

/* The word of sprite graphics a kept sprite's address reaches */
uint32_t s32_video_scene_sprite_word(const s32_scene_sprite *sprite, uint32_t address);
#endif

/* For tools that look at what the sprites are drawn with (the asset extractor's census): called for every
   sprite the list draws, before it is drawn, with the screen it goes to, its graphics' place in the sprite
   ROM (32-bit words, bank << 20 | offset), its size in words and rows, whether it is 8 bits a pen, whether it
   is opaque (it draws the end code, all ones at a word's edges, and goes on past it), and the pixel each pen
   puts in the sprite buffer (16 or 256 of them). NULL, the default, calls nothing. */
typedef void (*s32_sprite_observer)(int screen, uint32_t address, int words, int rows, bool bits8, bool opaque,
	const uint16_t *pixels);
extern s32_sprite_observer s32_video_sprite_observer;

/* The color (RGB555) a pixel of a screen's sprite buffer shows with its mixer's registers as they are now:
   its group's palette, without blending, shadow or color offsets */
uint16_t s32_video_sprite_pixel_color(int screen, uint16_t pixel);

/* Likewise for the tilemaps: called, when a frame is drawn, for every cell of the pages each tilemap
   (NBG0-3: 0-3) that is on shows, with its tile (bank << 13 | number) and color (the cell's 9 bits). NULL,
   the default, calls nothing. */
typedef void (*s32_tile_observer)(int tilemap, uint32_t tile, uint16_t color);
extern s32_tile_observer s32_video_tile_observer;

/* The color (RGB555) pen 'pen' of a tile drawn with color 'color' shows on tilemap 'tilemap' (0-3) of a
   screen, with its mixer's registers as they are now, without blending or color offsets */
uint16_t s32_video_tile_pen_color(int screen, int tilemap, uint16_t color, int pen);

#ifdef __cplusplus
}
#endif

#endif
