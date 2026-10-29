/**
 *    Name:        console.c
 *    Description: the console build's own versions of the game functions console/replaced.txt lists, and
 *                 what it tells the port
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The console build runs the arcade game's program with these in place of the game's own (whose
 *    definitions tools/decompile.py writes as CONSOLE_ORIGINAL(name): original_name here, for these to call):
 *    one player on the left seat's screen, free play, one sound for both speakers.
 */
#include "console.h"

#include "game.h"

#define COIN_SETTINGS       0x1Au    /* the test mode's COIN/CREDIT SETTING: 26 */
#define GAME_MODE_PLAY      6u
#define LEFT_SEAT           0u       /* screen index: the left seat's screen (screen 1), the right one's 1 */

/* ---- the coins ---- */

/* The settings as the EEPROM has them (the game's own load, with its repairs), then the coins set to free
   play as the test mode's COIN/CREDIT SETTING leaves them on FREE PLAY (the setting whose record has no
   coins), and written back as the game's own repairs write them */
void load_settings(uint32_t checksum)
{
	uint32_t setting;

	original_load_settings(checksum);
	if (free_play != 0u)
		return;
	for (setting = 0; setting < COIN_SETTINGS - 1u; setting++)
		if ((int8_t)COIN_SETTING_RECORD(coin_setting_records_ADDRESS + setting * 4u)->coins < 0)
			break;
	coin_setting = (uint8_t)setting;
	coins_per_credit = 1u;
	bonus_adder = 0xFFu;
	first_chute_multiplier = 1u;
	second_chute_multiplier = 1u;
	free_play = 0xFFu;
	eeprom_write_settings();
}

/* The credits line under the prompt: none (with free play the game's says FREE PLAY) */
void credit_display_update(void)
{
	OBJECT(current_object)->flags = 0u;
	OBJECT(current_object)->flags_high = 0u;
}

/* ---- the sound ---- */

/* A screen's sound requests, in the vertical blank: the left seat's always; the right one's only while it
   plays a game (game mode 6), the rest of the time dropped, so that what its screen would play unseen (the
   entry countdown's DJ, its attract mode) is not heard and takes none of the 24 voices the two seats share */
void send_sound_requests(void)
{
	if (SCREEN(screen)->index != LEFT_SEAT && SCREEN(screen)->game_mode != GAME_MODE_PLAY)
	{
		SCREEN(screen)->sound_queue_read = SCREEN(screen)->sound_queue_write;
		return;
	}
	original_send_sound_requests();
}

/* ---- the port ---- */

/* The console's own objects' code, at addresses the program has none at: none yet */
static bool console_code(uint32_t address)
{
	(void)address;
	return false;
}

void console_attach(void)
{
	call_code_elsewhere = console_code;
}

bool console_both_screens(void)
{
	return false;
}
