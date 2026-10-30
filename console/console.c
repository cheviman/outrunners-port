/**
 *    Name:        console.c
 *    Description: the console build's own versions of the game functions console/replaced.txt lists, and
 *                 what it tells the port
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The console build runs the arcade game's program with these in place of the game's own (whose
 *    definitions tools/decompile.py writes as CONSOLE_ORIGINAL(name): original_name here, for these to call):
 *    one player on the left seat's screen, free play, one sound for both speakers, and the title's menu
 *    (menu.c) in place of the start button.
 */
#include "console.h"
#include "console_internal.h"

#include "game.h"
#include "s32/video.h"

#define COIN_SETTINGS       0x1Au    /* the test mode's COIN/CREDIT SETTING: 26 */
#define START_BUTTON        0x40u    /* screen buttons_pressed: START */
#define TIME_ATTACK_TIME_LEFT (99u * 30u)    /* race time_left, in 30ths of a second */
#define SCREEN_CAR          0x1000u  /* the screen's car, beside its block (the register player_car) */
#define TRANSMISSION_MANUAL 1u       /* car transmission (car_select_wheel_transmissions): 0 automatic */

console_state console;

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

/* The prompt's message (INSERT COIN(S), TURN THE IGNITION KEY) and the ignition key's picture, which the
   prompt starts every 30 frames: ended at once, unseen; PRESS START is the console's (menu.c). The little
   car that crosses the prompt still does */
void coin_prompt_message_update(void)
{
	OBJECT(current_object)->flags = 0u;
	OBJECT(current_object)->flags_high = 0u;
}

void ignition_key_prompt_update(void)
{
	OBJECT(current_object)->flags = 0u;
	OBJECT(current_object)->flags_high = 0u;
}

/* ---- the machine ---- */

/* The settings OPTIONS changes, into the first race record's master fields as network_check_result puts
   them at boot (the races read the copies the link makes of them) */
void console_settings_to_race(void)
{
	const uint32_t race = RACE_1;

	RACE(race)->master_game_difficulty = game_difficulty;
	RACE(race)->master_time_difficulty_0 = time_difficulty[0];
	RACE(race)->master_time_difficulty_1 = (uint8_t)time_difficulty[1];
	RACE(race)->master_time_difficulty_2_5 = (uint8_t)time_difficulty[2] | ((uint32_t)(uint8_t)time_difficulty[3] << 8u)
		| ((uint32_t)(uint8_t)time_difficulty[4] << 16u) | ((uint32_t)(uint8_t)time_difficulty[5] << 24u);
}

/* The machine alone (stand alone, or the link failed), as the game's network_check_stand_alone and
   network_check_result set it up as machine 1, without NETWORK CHECK and its result on the screens for
   128 frames: the SEGA presentation is the first thing seen */
void network_check_stand_alone(void)
{
	const uint32_t race = RACE_1;

	link_off = 1u;
	player_count = 2u;
	machine_count = 1u;
	SCREEN(SCREEN_1)->car_number = 0u;
	RACE(RACE_1)->car_number = 0u;
	SCREEN(SCREEN_2)->car_number = 1u;
	RACE(RACE_2)->car_number = 1u;
	RACE(RACE_1)->machine = 1u;
	RACE(RACE_2)->machine = 1u;
	RACE(RACE_1)->player = 0u;
	RACE(RACE_2)->player = 1u;
	SCREEN(SCREEN_1)->player = 0u;
	SCREEN(SCREEN_2)->player = 1u;
	SCREEN(SCREEN_1)->race_copy = race_copy_offsets[0] + GAME_VARIABLES;
	SCREEN(SCREEN_2)->race_copy = race_copy_offsets[1] + GAME_VARIABLES;
	SCREEN(SCREEN_1)->race = RACE_1;
	SCREEN(SCREEN_2)->race = RACE_2;
	RACE(race)->master_advertise_sound = advertise_sound;
	RACE(race)->master_steering_wheel_reaction = steering_wheel_reaction;
	RACE(race)->master_coins_per_credit = coins_per_credit;
	RACE(race)->master_first_chute_multiplier = first_chute_multiplier;
	RACE(race)->master_bonus_adder = bonus_adder;
	RACE(race)->master_coin_setting = coin_setting;
	RACE(race)->master_free_play = free_play;
	console_settings_to_race();
	move_halfwords(race + 0x30u, BOOKKEEPING_COURSE_RECORDS_ADDRESS, 0x1Eu);
	clear_text_layer();
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

/* ---- starting games ---- */

/* The game's own start (check_game_start) on the screen running, as if its START had just been pressed:
   with free play it starts a game there (game mode 6), or joins the race being entered from game mode 7 */
static uint32_t press_start(uint32_t mode, uint32_t seat_credits)
{
	SCREEN(screen)->buttons_pressed |= START_BUTTON;
	return original_check_game_start(mode, seat_credits);
}

/* The steps of game mode 6 (and of the demo's race, game mode 4) from the race's set-up to its fade out
   (game_mode_play): the race on the screen */
static bool racing(uint32_t screen_block)
{
	const uint8_t mode = SCREEN(screen_block)->game_mode;
	const uint8_t step = SCREEN(screen_block)->game_step;

	if (mode != GAME_MODE_PLAY && mode != GAME_MODE_DEMO_RACE)
		return false;
	return step == 5u || step == 6u || step == 7u || step == 8u || step == 11u || (step >= 23u && step <= 27u);
}

/* The route map beside the race on the right seat's screen (side_map.c), its game mode 'mode': where the port
   has room for it, while the left seat races alone on the game's route and the right seat is idle */
static bool side_map_due(uint32_t mode)
{
	return console.side_map_wanted && !console.two_players && !console.classic && !console_routes_running()
		&& SCREEN(SCREEN_1)->game_mode == GAME_MODE_PLAY && racing(SCREEN_1)
		&& mode != GAME_MODE_PLAY && mode != GAME_MODE_ENTRY_INVITATION;
}

/* Each screen's frame before its game mode's code. The right seat starts only to join a 2 PLAYERS race, or a
   CLASSIC one with its own START, which shows its screen (its START is let alone otherwise), and shows the
   route map beside a race of the left one where the port has room for it; the left one's START opens the
   title's menu, whose ARCADE and 2 PLAYERS start the game as the arcade's START does (menu.c) */
uint32_t check_game_start(uint32_t mode, uint32_t seat_credits)
{
	if (console.quit_seats & (1u << SCREEN(screen)->index))
	{
		/* (the pause's QUIT GAME: a game being played ends, back to the title) */
		console.quit_seats &= (uint8_t)~(1u << SCREEN(screen)->index);
		if (mode == GAME_MODE_PLAY)
		{
			AS32(SCREEN(screen)->game_mode) = GAME_MODE_RESTART;
			return GAME_MODE_RESTART;
		}
	}
	if (SCREEN(screen)->index != LEFT_SEAT)
	{
		switch (console_side_map_frame(side_map_due(mode)))
		{
			case SIDE_MAP_ON:
				return GAME_MODE_CONSOLE_SCENE;
			case SIDE_MAP_OVER:
				AS32(SCREEN(screen)->game_mode) = GAME_MODE_RESTART;
				return GAME_MODE_RESTART;
			default:
				break;
		}
		if (console.join_right && mode == GAME_MODE_ENTRY_INVITATION)
		{
			console.join_right = false;
			return press_start(mode, seat_credits);
		}
		if (console.classic && mode == GAME_MODE_ENTRY_INVITATION)
		{
			mode = original_check_game_start(mode, seat_credits);
			if (mode == GAME_MODE_PLAY)
				console.two_players = true;    /* (a challenger) */
		}
		return mode;
	}
	if (mode == GAME_MODE_POWER_ON && console_presentation_frame(console.pressed))
	{
		console.pressed = 0;
		return GAME_MODE_CONSOLE_SCENE;    /* (the SEGA presentation first) */
	}
	if (console.time_attack)
	{
		/* TIME ATTACK: the race's time left kept at 99 seconds (the race's time is the HUD's lap and its
		   end's); over with the game */
		if (mode == GAME_MODE_PLAY)
		{
			RACE(SCREEN(screen)->race)->time_left = TIME_ATTACK_TIME_LEFT;
			console_ghost_frame();
		}
		else
			console.time_attack = false;
	}
	if (console_routes_frame(mode, console.pressed))
	{
		console.pressed = 0;
		return GAME_MODE_CONSOLE_SCENE;    /* (REVERSE's goal on the route map: the game waits) */
	}
	if (console.two_players && mode != GAME_MODE_PLAY && SCREEN(SCREEN_2)->game_mode != GAME_MODE_PLAY
		&& SCREEN(SCREEN_2)->game_mode != GAME_MODE_ENTRY_INVITATION && !console.join_right)
		console.two_players = false;    /* (the race of both is over) */
	if (console.classic && mode != GAME_MODE_PLAY && SCREEN(SCREEN_2)->game_mode != GAME_MODE_PLAY)
		console.classic = false;
	switch (console_menu_frame(mode))
	{
		case MENU_START_ONE:
			return press_start(mode, seat_credits);
		case MENU_START_TWO:
			console.two_players = true;
			console.join_right = true;
			return press_start(mode, seat_credits);
		case MENU_TIME_ATTACK:
			console.time_attack = true;
			console_ghost_start();
			return press_start(mode, seat_credits);
		case MENU_FULL:
			console_full_start();
			return press_start(mode, seat_credits);
		case MENU_CLASSIC:
			console.classic = true;
			return press_start(mode, seat_credits);
		case MENU_REVERSE:
			console_reverse_start();
			return press_start(mode, seat_credits);
		case MENU_TITLE:
			AS32(SCREEN(screen)->game_mode) = GAME_MODE_RESTART;
			return GAME_MODE_RESTART;
		default:
			return mode;
	}
}

/* The car select's WAITING FOR CHALLENGERS! (then JUST A MOMENT PLEASE) once the player's choices are made,
   while the race takes entries: with one player (no 2 PLAYERS, no CLASSIC) none, and the entries closed at
   once (player 0's record's entry_frames), as no one else comes */
void waiting_banner_show(void)
{
	if (console.two_players || console.classic)
	{
		original_waiting_banner_show();
		return;
	}
	if (RACE(RACE_1)->entry_frames > 1u)
		RACE(RACE_1)->entry_frames = 1u;
	OBJECT(current_object)->flags = 0u;
	OBJECT(current_object)->flags_high = 0u;
}

/* ---- the port ---- */

/* The console's own objects' code, at addresses the program has none at */
static bool console_code(uint32_t address)
{
	switch (address)
	{
		case CONSOLE_CODE_PRESENTATION_LOGO:
			presentation_logo_update();
			return true;
		case CONSOLE_CODE_PRESENTATION_CAR:
			presentation_car_update();
			return true;
		case CONSOLE_CODE_START_LINE:
			presentation_start_line_update();
			return true;
		case CONSOLE_CODE_GHOST:
			ghost_object_update();
			return true;
		case CONSOLE_CODE_ROUTE_CAR:
			route_select_car_update();
			return true;
		case CONSOLE_CODE_SIDE_MAP:
			side_map_update();
			return true;
		case CONSOLE_CODE_SIDE_MAP_CAR:
			side_map_car_update();
			return true;
		case CONSOLE_CODE_SIDE_MAP_SIGN:
			side_map_sign_update();
			return true;
		default:
			return false;
	}
}

void console_attach(void)
{
	call_code_elsewhere = console_code;
	s32_video_use_extra_palette(true);    /* (REVERSE's start line) */
}

int console_wide_side(void)
{
	if (console.two_players)
		return 0;    /* (2 PLAYERS: both screens alike) */
	switch (console_window_settings.wide)
	{
		case WIDE_16_9: return S32_VIDEO_WIDE_SIDE_16_9;
		case WIDE_20_9: return S32_VIDEO_WIDE_SIDE_20_9;
		default: return 0;
	}
}

bool console_both_screens(void)
{
	return console.two_players;
}

int console_test_autopilot(void)
{
	/* the wheel back toward the middle of the road, by the left seat's place across it (race road_x) */
	const int across = (int16_t)RACE(RACE_1)->road_x;
	const int wheel = 0x80 - across / 6;

	return wheel < 0x10 ? 0x10 : (wheel > 0xF0 ? 0xF0 : wheel);
}

void console_actions(uint32_t pressed)
{
	console.pressed |= pressed;
}

bool console_can_pause(void)
{
	/* (not over REVERSE's map, in the race's step 5: its START picks the goal) */
	return SCREEN(SCREEN_1)->game_mode == GAME_MODE_PLAY && racing(SCREEN_1) && !console_route_select_running();
}

bool console_manual_gears(void)
{
	return console_can_pause() && CAR(SCREEN_1 + SCREEN_CAR)->transmission == TRANSMISSION_MANUAL;
}

void console_quit_game(void)
{
	console.quit_seats = 3u;
}

void console_side_map_wanted(bool wanted)
{
	console.side_map_wanted = wanted;
}

bool console_side_map_shown(void)
{
	return console_side_map_running();
}

bool console_right_seat_joins(void)
{
	return console.classic && SCREEN(SCREEN_2)->game_mode == GAME_MODE_ENTRY_INVITATION;
}

bool console_quit_requested(void)
{
	return console.quit;
}
