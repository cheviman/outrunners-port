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

/* The controls' pictures for the renderer, their letters from font.bmp in the folder 'base' (with its
   separator); shown from the start where there is a touch screen */
void touch_open(SDL_Renderer *renderer, const char *base);

/* An event: a finger on the controls; a game controller's button or stick, or a key, hides them until a
   finger touches the screen again */
void touch_event(const SDL_Event *event);

/* The menus' actions the fingers hold (ACTION_BIT): the wheel's directions, GAS's accept, START's start */
uint32_t touch_actions(void);

/* True while a finger holds the cabinet's control 'control' (CONTROL_*) */
bool touch_holds(int control);

/* How far the wheel turns, -TOUCH_WHEEL_FULL (left) to TOUCH_WHEEL_FULL */
int touch_wheel(void);

/* The controls over the picture, before it is presented, the gears' buttons with 'gears' (a race with the
   manual transmission) alone: nothing while they are hidden */
void touch_draw(SDL_Renderer *renderer, bool gears);

#endif
