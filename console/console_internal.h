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
	MENU_REVERSE,       /* a game of one player from a goal back to the start (& KNUCKLES > REVERSE) */
	MENU_TITLE          /* the title from its start (game mode 1) */
};

typedef struct
{
	uint32_t pressed;       /* the actions pressed since the left seat's last frame */
	bool quit;
	bool two_players;       /* 2 PLAYERS: both screens shown, the right seat in the race */
	bool join_right;        /* the right seat's START, pressed once it is in the entry (game mode 7) */
	bool time_attack;       /* DELUXE > TIME ATTACK: the left seat's race has no time limit */
} console_state;

extern console_state console;

/* The console's own objects' code, at addresses the program has none at (console_code) */
#define CONSOLE_CODE_PRESENTATION_LOGO  0xFE0000u
#define CONSOLE_CODE_PRESENTATION_CAR   0xFE0001u
#define CONSOLE_CODE_START_LINE         0xFE0002u    /* REVERSE's start line (reverse.c) */
#define CONSOLE_CODE_GHOST              0xFE0003u    /* TIME ATTACK's ghost's objects (ghost.c) */

void presentation_logo_update(void);
void presentation_car_update(void);
void presentation_start_line_update(void);
void ghost_object_update(void);

/* TIME ATTACK's ghost (ghost.c): a game starts; the left seat's frame during it */
void console_ghost_start(void);
void console_ghost_frame(void);

/* The SEGA presentation (presentation.c) on the left seat's frame at power-on, with the actions pressed:
   true while it lasts (the game mode's code waits), false once it is over */
bool console_presentation_frame(uint32_t pressed);

/* SEGA's jingle (jingle.c): started, then stepped once a frame of the left seat's */
void console_jingle_start(void);
void console_jingle_frame(void);
void console_jingle_stop(void);    /* (its notes off at once: the scene skipped or over) */

/* REVERSE (reverse.c): started from the goal picked (0-9: Australia to Kenya), running, and its part of the
   left seat's frame before the game mode's code */
void console_reverse_start(int goal);
bool console_reverse_running(void);
void console_reverse_frame(uint32_t mode);

/* The goal REVERSE starts from (menu.c's pick, 0-9) */
int console_menu_reverse_goal(void);

/* DIFFICULTY and TIME, as OPTIONS leaves them, into the race records (console.c) */
void console_settings_to_race(void);

/* The menus on the left seat's frame, before its game mode's code ('mode'): what it asks for (MENU_*) */
int console_menu_frame(uint32_t mode);

/* The objects of the screen running: the first live one at or after 'from' whose handler is 'handler' (0:
   none); from 0 the first */
uint32_t console_find_object(uint32_t handler, uint32_t from);

#endif
