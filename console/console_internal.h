/**
 *    Name:        console_internal.h
 *    Description: what the console build's files share among themselves
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 */
#ifndef ORUNNERS_CONSOLE_INTERNAL_H
#define ORUNNERS_CONSOLE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#define LEFT_SEAT                   0u    /* screen index: the left seat's screen (screen 1); the right one's is 1 */

/* The game modes (screen game_mode) */
#define GAME_MODE_POWER_ON          0u
#define GAME_MODE_RESTART           1u    /* the title from its start */
#define GAME_MODE_TITLE             2u
#define GAME_MODE_CAR_PRESENTATION  3u
#define GAME_MODE_DEMO_RACE         4u
#define GAME_MODE_RANKING           5u
#define GAME_MODE_PLAY              6u
#define GAME_MODE_ENTRY_INVITATION  7u
#define GAME_MODE_CONSOLE_SCENE     9u    /* none of the game's: its code runs nothing (the console's scene) */

/* What the menus ask of the left seat's frame (console_menu_frame) */
enum
{
	MENU_NOTHING,
	MENU_START_ONE,     /* a game of one player (ARCADE) */
	MENU_START_TWO,     /* a game of both seats (2 PLAYERS) */
	MENU_TIME_ATTACK,   /* a game of one player without the time limit (& KNUCKLES > TIME ATTACK) */
	MENU_FULL,          /* a game of one player through the 31 stages (& KNUCKLES > FULL) */
	MENU_REVERSE,       /* a game of one player from a goal back to the start (& KNUCKLES > REVERSE) */
	MENU_CLASSIC,       /* the arcade's game as it is, the right seat free to join (& KNUCKLES > CLASSIC) */
	MENU_TITLE          /* the title from its start (game mode 1) */
};

typedef struct
{
	uint32_t pressed;       /* the actions pressed since the left seat's last frame */
	bool quit;
	bool two_players;       /* 2 PLAYERS: both screens shown, the right seat in the race */
	bool join_right;        /* the right seat's START, pressed once it is in the entry (game mode 7) */
	bool time_attack;       /* DELUXE > TIME ATTACK: the left seat's race has no time limit */
	bool classic;           /* DELUXE > CLASSIC: the game's entries as the arcade's (WAITING FOR CHALLENGERS) */
	uint8_t quit_seats;     /* the pause's QUIT GAME: the seats (bit 0 the left) still to go back to the title */
} console_state;

extern console_state console;

/* The console's own objects' code, at addresses the program has none at (console_code) */
#define CONSOLE_CODE_PRESENTATION_LOGO  0xFE0000u
#define CONSOLE_CODE_PRESENTATION_CAR   0xFE0001u
#define CONSOLE_CODE_START_LINE         0xFE0002u    /* REVERSE's start line (routes.c) */
#define CONSOLE_CODE_GHOST              0xFE0003u    /* TIME ATTACK's ghost's objects (ghost.c) */
#define CONSOLE_CODE_ROUTE_CAR          0xFE0004u    /* the car on REVERSE's route map (route_select.c) */

/* The menus' sound, a line changed or picked: the test mode's sound 57 */
#define MENU_SOUND          0x4Eu

void presentation_logo_update(void);
void presentation_car_update(void);
void presentation_start_line_update(void);
void ghost_object_update(void);
void route_select_car_update(void);

/* TIME ATTACK's ghost (ghost.c): a game starts; the left seat's frame during it */
void console_ghost_start(void);
void console_ghost_frame(void);

/* The SEGA presentation (presentation.c) on the left seat's frame at power-on, with the actions pressed:
   true while it lasts (the game mode's code waits), false once it is over */
bool console_presentation_frame(uint32_t pressed);


/* & KNUCKLES' routes (routes.c): REVERSE started (its goal picked on the route map after the car select),
   FULL, and their part of the left seat's frame before the game mode's code, with the actions pressed */
void console_reverse_start(void);
void console_full_start(void);
bool console_routes_frame(uint32_t mode, uint32_t pressed);    /* (true: the left seat's game waits) */

/* REVERSE's goal on the route map (route_select.c), once the car select is over: started with the car
   model chosen, on while the goal is picked, the left seat's frame meanwhile (its game waits) and the goal
   picked (0-9, the last one picked when it starts again) */
enum
{
	ROUTE_SELECT_NOTHING,
	ROUTE_SELECT_START,     /* the goal picked, the screen dark again: REVERSE starts there */
	ROUTE_SELECT_BACK       /* the game given up: back to the title */
};

void console_route_select_start(int model);
bool console_route_select_running(void);
int console_route_select_frame(uint32_t pressed);
int console_route_select_goal(void);

/* DIFFICULTY and TIME, as OPTIONS leaves them, into the race records (console.c) */
void console_settings_to_race(void);

/* The menus on the left seat's frame, before its game mode's code ('mode'): what it asks for (MENU_*) */
int console_menu_frame(uint32_t mode);

/* The objects of the screen running: the first live one at or after 'from' whose handler is 'handler' (0:
   none); from 0 the first */
uint32_t console_find_object(uint32_t handler, uint32_t from);

#endif
