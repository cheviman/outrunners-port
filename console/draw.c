/**
 *    Name:        draw.c
 *    Description: the console's own sprites in the screen's sprite list: letters of the game's sprite font,
 *                 at any size
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    As the game's draw_text_sprites writes its letters: a sprite entry a letter (8 x 8 pixels of the sprite
 *    font, at text_sprite_font + 8 x the character), in the screen's sprite list and its halfway copy, linked
 *    into the text's depth layer (19E, drawn after nearly every other); the console's are drawn at a size of
 *    its own (the entry's screen height and width).
 */
#include "console.h"
#include "console_draw.h"

#include "game.h"

#include <string.h>

#define LETTER                  8          /* pixels: the font's letters, as they are */
#define HALFWAY_COPY            0x3000u    /* the list's copy, drawn between the 30 frames a second */
#define SIZE_BITS               0x03FFu    /* an entry's screen height and width */

int console_text_width(const char *text, int scale)
{
	int letters = 0;

	while (text[letters])
		letters++;
	return letters * LETTER * scale;
}

/* Writes one 16-byte entry of the list and its halfway copy */
static void put_entry(uint32_t sprite, const uint16_t words[8])
{
	for (int word = 0; word < 8; word++)
	{
		SPRITE_WORDS(sprite)[word] = words[word];
		SPRITE_WORDS(sprite + HALFWAY_COPY)[word] = words[word];
	}
}

/* Links the group of entries from 'first' to before 'end' into the text's depth list, newest first (as
   draw_text_sprites ends): the entry at 'end' jumps on to the group drawn before it (with none,
   finish_sprite_list chains it on to the next layer) */
static void link_group(uint32_t first, uint32_t end)
{
	const uint16_t head = SCREEN(screen)->text_depth_list_head;

	if (head == 0u)
		SCREEN(screen)->text_depth_list_tail = (uint16_t)end;
	AS32(SPRITE_WORDS(end)[0]) = head;
	AS32(SPRITE_WORDS(end + HALFWAY_COPY)[0]) = (uint16_t)(head + 0x300u);
	SCREEN(screen)->sprite_list_last = end;
	SCREEN(screen)->text_depth_list_head = (uint16_t)((first >> 4) | 0x8000u);
	SCREEN(screen)->sprite_groups++;
}

void console_shade(int x, int y, int width, int height)
{
	/* one pen of graphics stretched over the rectangle, through a table of its own (the two entries after
	   it) whose colours are never see-through: every pixel of it darkens what is under it */
	const uint32_t first = SCREEN(screen)->sprite_list_last + 0x10u;
	const uint32_t size = text_sprite_size | ((uint32_t)SCREEN(screen)->sprite_screen_bit << 16);
	uint16_t words[8] = { 0 };

	if (width <= 0 || height <= 0 || SCREEN(screen)->sprite_list_limit <= first + 0x30u)
		return;
	words[0] = 0x3B0Au;    /* indirect (13), its table inline (12), a shadow (11), opaque (8), by its top left */
	words[1] = 0x0102u;    /* one row of one word */
	words[2] = (uint16_t)(height & SIZE_BITS);
	words[3] = (uint16_t)(((uint16_t)(size >> 16) & ~SIZE_BITS) | (uint16_t)(width & SIZE_BITS));
	words[4] = (uint16_t)y;
	words[5] = (uint16_t)x;
	put_entry(first, words);
	words[0] = words[1] = words[2] = words[3] = words[4] = words[5] = 0u;
	put_entry(first + 0x10u, words);
	put_entry(first + 0x20u, words);
	link_group(first, first + 0x30u);
}

void console_text(int x, int y, int scale, uint16_t colour, const char *text)
{
	const uint32_t first = SCREEN(screen)->sprite_list_last + 0x10u;
	const uint32_t size = text_sprite_size | ((uint32_t)SCREEN(screen)->sprite_screen_bit << 16);
	uint32_t sprite = first;
	uint16_t words[8];

	if (!text[0] || scale < 1)
		return;
	for (const char *letter = text; *letter; letter++, x += LETTER * scale)
	{
		if (SCREEN(screen)->sprite_list_limit <= sprite + 0x10u)    /* (room for the group's jump after it) */
			break;
		words[0] = (uint16_t)text_sprite_words_0_1;
		words[1] = (uint16_t)(text_sprite_words_0_1 >> 16);
		words[2] = (uint16_t)(((uint16_t)size & ~SIZE_BITS) | (uint16_t)(LETTER * scale));
		words[3] = (uint16_t)(((uint16_t)(size >> 16) & ~SIZE_BITS) | (uint16_t)(LETTER * scale));
		words[4] = (uint16_t)y;
		words[5] = (uint16_t)x;
		words[6] = (uint16_t)(((uint32_t)(uint8_t)*letter << 3) + text_sprite_font);
		words[7] = (uint16_t)(text_sprite_palette | colour);
		put_entry(sprite, words);
		sprite += 0x10u;
	}
	if (sprite != first)
		link_group(first, sprite);
}

/* ---- the pause's words, for the port ---- */

#define PAUSE_Y             84
#define PAUSE_LINES_Y       116
#define PAUSE_LINE_STEP     14
#define PAUSE_TEXT          0x0090u    /* the HUD's text colours: white, and the split time's yellow */
#define PAUSE_CHOSEN        0x00B0u
#define SPRITE_END          0xC000u    /* the list's end command */

static const char *const pause_items[CONSOLE_PAUSE_ITEMS] = { "CONTINUE", "QUIT GAME" };

/* One line of text into the list, as console_text puts it into the screen's: centred */
static size_t pause_line(uint8_t *list, size_t at, size_t room, int y, int scale, uint16_t colour, const char *text)
{
	const uint32_t size = text_sprite_size | ((uint32_t)SCREEN(SCREEN_1)->sprite_screen_bit << 16);
	int x = (320 - console_text_width(text, scale)) / 2;

	for (const char *letter = text; *letter; letter++, x += LETTER * scale)
	{
		uint16_t words[8];

		if (at + 32u > room)    /* (room for the end after it) */
			break;
		words[0] = (uint16_t)text_sprite_words_0_1;
		words[1] = (uint16_t)(text_sprite_words_0_1 >> 16);
		words[2] = (uint16_t)(((uint16_t)size & ~SIZE_BITS) | (uint16_t)(LETTER * scale));
		words[3] = (uint16_t)(((uint16_t)(size >> 16) & ~SIZE_BITS) | (uint16_t)(LETTER * scale));
		words[4] = (uint16_t)y;
		words[5] = (uint16_t)x;
		words[6] = (uint16_t)(((uint32_t)(uint8_t)*letter << 3) + text_sprite_font);
		words[7] = (uint16_t)(text_sprite_palette | colour);
		for (int word = 0; word < 8; word++)
		{
			list[at++] = (uint8_t)words[word];
			list[at++] = (uint8_t)(words[word] >> 8);
		}
	}
	return at;
}

size_t console_pause_sprites(uint8_t *list, size_t room, int choice)
{
	size_t at = 0;

	if (room < 16u)
		return 0;
	at = pause_line(list, at, room, PAUSE_Y, 2, PAUSE_CHOSEN, "PAUSE");
	for (int item = 0; item < CONSOLE_PAUSE_ITEMS; item++)
		at = pause_line(list, at, room, PAUSE_LINES_Y + item * PAUSE_LINE_STEP, 1,
			item == choice ? PAUSE_CHOSEN : PAUSE_TEXT, pause_items[item]);
	memset(&list[at], 0, 16);
	list[at + 1] = (uint8_t)(SPRITE_END >> 8);
	return at + 16u;
}
