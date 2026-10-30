/**
 *    Name:        touch.h
 *    Description: the touch screen's controls (Android): the wheel, GAS, BRAKE, the gears, the radio and START
 *                 drawn over the picture, and the controls and actions the fingers on them hold
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 */
#ifndef ORUNNERS_PORT_TOUCH_H
#define ORUNNERS_PORT_TOUCH_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

#define TOUCH_WHEEL_FULL 32767    /* the wheel turned all the way, as a game controller's left stick */
#define TOUCH_PLAYERS    2        /* the players with controls of their own: both with the screens facing */

/* The controls' pictures for the renderer, their letters from font.bmp in the folder 'base' (with its
   separator); shown from the start where there is a touch screen */
void touch_open(SDL_Renderer *renderer, const char *base);

/* An event: a finger on the controls; a game controller's button or stick, or a key, hides them until a
   finger touches the screen again */
void touch_event(const SDL_Event *event);

/* The menus' actions the fingers hold (ACTION_BIT): the wheel's directions, GAS's accept, START's start */
uint32_t touch_actions(void);

/* True while a finger holds the cabinet's control 'control' (CONTROL_*) of player 'player''s controls */
bool touch_holds(int player, int control);

/* How far the player's wheel turns, -TOUCH_WHEEL_FULL (left) to TOUCH_WHEEL_FULL */
int touch_wheel(int player);

/* Where the controls go on a screen of 'width' x 'height' with the picture at 'picture': over it, on the whole
   screen; or with room enough under it (a screen taller than the picture), the picture at the top
   ('picture' moved) and the controls in the room under it */
void touch_layout(int width, int height, SDL_Rect *picture);

/* 2 PLAYERS on a tablet: the screens facing each other on a screen of 'width' x 'height', the first player's
   on its bottom half and the second's on the top one, turned half round for the player across, each with its
   controls over it; 'pictures' the places of their pictures of 'picture_width' x 'picture_height' (the
   second's to be shown turned) */
void touch_layout_facing(int width, int height, int picture_width, int picture_height,
	SDL_Rect pictures[TOUCH_PLAYERS]);

/* The room under the picture, for the game's route map behind the controls: false on a phone's screen (the
   controls over the picture) */
bool touch_map_room(SDL_Rect *room);

/* The controls over the picture, before it is presented, each player's gears' buttons with its 'gears' (a race
   with the manual transmission) alone: nothing while they are hidden */
void touch_draw(SDL_Renderer *renderer, const bool gears[TOUCH_PLAYERS]);

#endif
