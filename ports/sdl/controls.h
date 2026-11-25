/**
 *    Name:        controls.h
 *    Description: the keys and game controller buttons of each player's controls of the cabinet (console
 *                 OPTIONS > CONTROLS), and the file that keeps them (controls.ini)
 *    Version:     0.1.1
 *    Created:     29-10-2026
 *    Modified:    24-11-2026
 *    Author:      Sebastian Toyos
 */
#ifndef ORUNNERS_PORT_CONTROLS_H
#define ORUNNERS_PORT_CONTROLS_H

#include "console.h"

#include "port_sdl.h"
#include <stdbool.h>

#define CONTROL_KEYS     2
#define CONTROL_BUTTONS  3

/* The triggers and the sticks' ways as buttons of the bindings, past SDL's own (its game controller has them
   as axes): the axes' buttons. The left stick's left and right are the wheel's already (issue 42: the right
   stick's up and down as the pedals) */
#define CONTROL_LEFT_TRIGGER       ((SDL_GamepadButton)SDL_GAMEPAD_BUTTON_COUNT)
#define CONTROL_RIGHT_TRIGGER      ((SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_COUNT + 1))
#define CONTROL_LEFT_STICK_UP      ((SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_COUNT + 2))
#define CONTROL_LEFT_STICK_DOWN    ((SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_COUNT + 3))
#define CONTROL_RIGHT_STICK_UP     ((SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_COUNT + 4))
#define CONTROL_RIGHT_STICK_DOWN   ((SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_COUNT + 5))
#define CONTROL_RIGHT_STICK_LEFT   ((SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_COUNT + 6))
#define CONTROL_RIGHT_STICK_RIGHT  ((SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_COUNT + 7))

/* An axis's button holds its control from any travel past its rest's wobble (of 32767) */
#define AXIS_PRESSED  1024

typedef struct
{
	const char *name;                                   /* as the settings file says it */
	SDL_Scancode keys[CONTROL_KEYS];                    /* SDL_SCANCODE_UNKNOWN: none */
	SDL_GamepadButton buttons[CONTROL_BUTTONS];  /* SDL_GAMEPAD_BUTTON_INVALID: none; or a trigger */
} control_binding;

/* Each seat's: their keys, and the buttons of their own game controller (the first one plugged in is
   player 1's, the second player 2's) */
extern control_binding control_bindings[CONTROL_PLAYERS][CONTROLS];

/* controls.ini at 'path': read (the defaults where it says nothing), or written when there is none */
void controls_open(const char *path);
void controls_save(void);
void controls_defaults(int player);    /* (and saved) */

/* The key or button pressed for a player's control on the CONTROLS page: its only key, or its only button,
   now (the ones it had before let go of it: issue 56), and no longer any other control's of the player's
   (and saved) */
void controls_bind_key(int player, int control, SDL_Scancode key);
void controls_bind_button(int player, int control, SDL_GamepadButton button);

/* The axis's button an axis moved to 'value' takes CONTROLS' control with, from halfway: a trigger, or a
   stick's way; SDL_GAMEPAD_BUTTON_INVALID short of that, or for the left stick's left and right */
SDL_GamepadButton controls_axis_button(SDL_GamepadAxis axis, int value);

/* True while one of the player's keys or their game controller's buttons for 'control' is held, its axes'
   apart */
bool controls_holds(int player, SDL_Gamepad *pad, int control);

/* How far the player's game controller's axes' buttons for 'control' are pressed, the furthest: 0 to 32767
   (a stick's from past its rest's wobble) */
int controls_axis_travel(int player, SDL_Gamepad *pad, int control);

#endif
