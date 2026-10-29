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

/* What the menus ask of the left seat's frame (console_menu_frame) */
enum
{
	MENU_NOTHING,
	MENU_START_ONE,     /* a game of one player (ARCADE) */
	MENU_START_TWO,     /* a game of both seats (2 PLAYERS) */
	MENU_TITLE          /* the title from its start (game mode 1) */
};

typedef struct
{
	uint32_t pressed;       /* the actions pressed since the left seat's last frame */
	bool quit;
	bool two_players;       /* 2 PLAYERS: both screens shown, the right seat in the race */
	bool join_right;        /* the right seat's START, pressed once it is in the entry (game mode 7) */
} console_state;

extern console_state console;

/* The menus on the left seat's frame, before its game mode's code ('mode'): what it asks for (MENU_*) */
int console_menu_frame(uint32_t mode);

/* The objects of the screen running: the first live one at or after 'from' whose handler is 'handler' (0:
   none); from 0 the first */
uint32_t console_find_object(uint32_t handler, uint32_t from);

#endif
