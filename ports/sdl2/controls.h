/**
 *    Name:        controls.h
 *    Description: the keys and game controller buttons of each player's controls of the cabinet (console
 *                 OPTIONS > CONTROLS), and the file that keeps them (controls.ini)
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 */
#ifndef ORUNNERS_PORT_CONTROLS_H
#define ORUNNERS_PORT_CONTROLS_H

#include "console.h"

#include <SDL.h>
#include <stdbool.h>

#define CONTROL_KEYS     2
#define CONTROL_BUTTONS  2

typedef struct
{
	const char *name;                                   /* as the settings file says it */
	SDL_Scancode keys[CONTROL_KEYS];                    /* SDL_SCANCODE_UNKNOWN: none */
	SDL_GameControllerButton buttons[CONTROL_BUTTONS];  /* SDL_CONTROLLER_BUTTON_INVALID: none */
} control_binding;

/* Each seat's: their keys, and the buttons of their own game controller (the first one plugged in is
   player 1's, the second player 2's) */
extern control_binding control_bindings[CONTROL_PLAYERS][CONTROLS];

/* controls.ini at 'path': read (the defaults where it says nothing), or written when there is none */
void controls_open(const char *path);
void controls_save(void);
void controls_defaults(int player);    /* (and saved) */

/* The key or button pressed for a player's control on the CONTROLS page: its first one now (and saved) */
void controls_bind_key(int player, int control, SDL_Scancode key);
void controls_bind_button(int player, int control, SDL_GameControllerButton button);

/* True while one of the player's keys or their game controller's buttons for 'control' is held */
bool controls_holds(int player, SDL_GameController *pad, int control);

#endif
