/**
 *    Name:        presentation.c
 *    Description: the console's SEGA presentation, before the title at power-on: the title's SEGA large on
 *                 black, its letters dark until cars race across them and light them up, the engine
 *                 starting and SEGA's jingle
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The scene is the console's own game mode on the left seat's screen (console.c): its objects are the
 *    game's, with handlers of the console's (console_code), drawn by the game's own draw_object. The SEGA is
 *    the title's (title_pictures' third: the sprite title/title_pictures, row sega_logo) at twice its
 *    size; a shadow (draw.c) darkens it from where the leading car is to the right; the cars are the title's
 *    logo cars (title_car_animations, title_logo_car_palettes), larger, in the depth layer after the
 *    shadow's. The engine is sound 21, the one the game plays as a game starts (the ignition key turned);
 *    the jingle is SegaSonic's two chords (jingle.c).
 */
#include "console.h"
#include "console_internal.h"
#include "console_draw.h"

#include "game.h"

#define SCREEN_WIDTH        320
#define LOGO_PICTURE        2          /* title_pictures' SEGA */
#define LOGO_SCALE          0x200u     /* twice */
#define LOGO_X              160
#define LOGO_Y              128        /* (its bottom: the picture hangs above its place) */
#define LOGO_WIDTH          128        /* its 64 pixels, twice */
#define LOGO_HEIGHT         32
#define LOGO_LAYER          0x180u
#define CAR_LAYER           0x19Fu     /* after the text's (19E), where the shadow is */
#define CAR_SCALE           0x48u
#define CAR_FRAME           9u         /* the logo cars' frame: from the side */
#define CAR_SPEED           9          /* pixels a frame (the screen's 30 a second) */
#define CAR_START_X         (-48)
#define CAR_END_X           380
#define CARS                3
#define ENGINE_SOUND        0x21u      /* the game starting: the ignition key turned */

/* The scene's frames, at the screen's 30 a second */
#define AT_ENGINE           12
#define AT_CARS             30         /* the first car leaves (the others 5 frames apart) */
#define AT_JINGLE           82         /* every letter lit */
#define AT_FADE_OUT         140
#define AT_END              156

static const int car_rows[CARS] = { LOGO_Y - 22, LOGO_Y - 12, LOGO_Y - 2 };

static struct
{
	int frame;
	bool done;
	int lit_to;            /* the logo is lit left of here: the leading car's place */
} scene;

/* ---- the objects' code (console_code) ---- */

/* The SEGA: the title's picture, scaled up and centred; the shadow over its dark part */
void presentation_logo_update(void)
{
	if (OBJECT(current_object)->state == 0u)
	{
		const uint32_t picture = title_pictures_ADDRESS + (LOGO_PICTURE << 4);

		OBJECT(current_object)->draw_mode = 0u;
		animation_start(PICTURE_RECORD(picture)->animation);
		OBJECT(current_object)->palette = PICTURE_RECORD(picture)->palette;
		OBJECT(current_object)->width_scale = LOGO_SCALE;
		OBJECT(current_object)->height_scale = LOGO_SCALE;
		OBJECT(current_object)->x = LOGO_X;
		OBJECT(current_object)->y = LOGO_Y;
		OBJECT(current_object)->depth_layer = LOGO_LAYER;
		OBJECT(current_object)->state++;
	}
	draw_object(0u);
	if (scene.lit_to < SCREEN_WIDTH)
		console_shade(scene.lit_to, LOGO_Y - LOGO_HEIGHT - 8, SCREEN_WIDTH - scene.lit_to, LOGO_HEIGHT + 16);
}

/* A car racing across the SEGA from the left (index: its row, and its start a few frames after the one
   before), a model at random */
void presentation_car_update(void)
{
	const int index = OBJECT(current_object)->title_logo_car.index;
	int x;

	switch (OBJECT(current_object)->state)
	{
		case 0:
			if (scene.frame < AT_CARS + index * 5)
				return;
			{
				const uint32_t model = random_number() & 7u;

				OBJECT(current_object)->draw_mode = 0xAu;
				animation_start(title_car_animations[model]);
				OBJECT(current_object)->palette = title_logo_car_palettes[model];
				OBJECT(current_object)->x = (uint16_t)CAR_START_X;
				OBJECT(current_object)->y = (uint16_t)car_rows[index];
				OBJECT(current_object)->width_scale = CAR_SCALE;
				OBJECT(current_object)->height_scale = CAR_SCALE;
				OBJECT(current_object)->frame = CAR_FRAME;
				OBJECT(current_object)->depth_layer = CAR_LAYER;
				OBJECT(current_object)->state++;
			}
			break;
		case 1:
			x = (int16_t)OBJECT(current_object)->x + CAR_SPEED;
			OBJECT(current_object)->x = (uint16_t)x;
			if (x > scene.lit_to)
				scene.lit_to = x;
			if (x > CAR_END_X)
			{
				OBJECT(current_object)->flags = 0u;
				OBJECT(current_object)->flags_high = 0u;
				return;
			}
			break;
	}
	draw_object(0u);
}

/* ---- the scene ---- */

static void start(uint32_t handler, uint8_t index)
{
	const uint32_t object = start_object();

	OBJECT(object)->handler = handler;
	OBJECT(object)->title_logo_car.index = index;
}

bool console_presentation_frame(uint32_t pressed)
{
	if (scene.done)
		return false;
	if (scene.frame == 0)
	{
		/* as the title clears its screen, with its palettes (the SEGA's and the cars'), from black */
		clear_objects();
		reset_tile_layers();
		clear_sprite_group_count();
		clear_text_layer();
		queue_palette(title_palette);
		SCREEN(screen)->fade_kind = 1u;
		SCREEN(screen)->flags |= 1u;
		scene.lit_to = (SCREEN_WIDTH - LOGO_WIDTH) / 2;
		start(CONSOLE_CODE_PRESENTATION_LOGO, 0u);
		for (uint8_t car = 0; car < CARS; car++)
			start(CONSOLE_CODE_PRESENTATION_CAR, car);
	}
	if (pressed & (ACTION_BIT(ACTION_START) | ACTION_BIT(ACTION_ACCEPT)) && scene.frame < AT_FADE_OUT)
		scene.frame = AT_FADE_OUT;    /* (skipped: the fade out at once) */
	if (scene.frame == AT_ENGINE)
		queue_sound(ENGINE_SOUND);
	if (scene.frame == AT_JINGLE)
	{
		scene.lit_to = SCREEN_WIDTH;
		console_jingle_start();
	}
	if (scene.frame == AT_FADE_OUT)
	{
		SCREEN(screen)->fade_kind = 0u;
		SCREEN(screen)->flags |= 1u;
	}
	console_jingle_frame();
	if (++scene.frame >= AT_END && (SCREEN(screen)->flags & 1u) == 0u)
	{
		scene.done = true;
		console_jingle_stop();
		clear_objects();
		return false;
	}
	return true;
}
