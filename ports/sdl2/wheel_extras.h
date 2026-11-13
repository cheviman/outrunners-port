/**
 *    Name:        wheel_extras.h
 *    Description: OPTIONS > STEERING's SET UP WHEEL on more than one device: a racing wheel's pedals and buttons
 *                 on devices of their own (pedals, a shifter), read beside the wheel's game controller
 *    Version:     0.1.1
 *    Created:     13-11-2026
 *    Modified:    13-11-2026
 *    Author:      Sebastian Toyos
 */
#ifndef ORUNNERS_PORT_WHEEL_EXTRAS_H
#define ORUNNERS_PORT_WHEEL_EXTRAS_H

#include <SDL.h>
#include <stdbool.h>

/* The controls SET UP WHEEL asks for after the wheel, as they may be on another device */
enum
{
	WHEEL_EXTRA_GAS,
	WHEEL_EXTRA_BRAKE,
	WHEEL_EXTRA_SHIFT_UP,
	WHEEL_EXTRA_SHIFT_DOWN,
	WHEEL_EXTRA_START,
	WHEEL_EXTRAS
};

/* One of them on another device than the wheel's: the device (its GUID), its axis (a pedal: the whole axis
   or a half of it, as a game controller's trigger takes it) or its button (a button, or an axis held past
   the middle of its travel: a handbrake) */
typedef struct
{
	bool used;
	SDL_JoystickGUID device;
	int index;
	bool axis;       /* a button's: 'index' is an axis */
	int half;        /* an axis': 0 the whole axis, 1 or -1 its half that way */
	bool inverted;   /* an axis' whole axis, pressed toward its negative end */
} wheel_extra;

/* The file of the wheel's controls on other devices (wheel_devices.txt, beside gamecontrollerdb.txt), read */
void wheel_extras_open(const char *path);

/* SET UP WHEEL done: the wheel's (its GUID) controls on other devices, kept in the file and used from now
   on; none, where every control is the wheel's own (the file then empty: the game controller alone, as
   before) */
void wheel_extras_save(SDL_JoystickGUID wheel, const wheel_extra extras[WHEEL_EXTRAS]);

/* SDL_JOYDEVICEADDED, SDL_JOYDEVICEREMOVED: the extras' devices opened as they come */
void wheel_extras_device_added(int device_index);
void wheel_extras_device_removed(SDL_JoystickID which);

/* True for the wheel set up with controls on other devices; for one of those devices (not the wheel's: no
   player's game controller, its pedals and buttons the wheel's player's) */
bool wheel_extras_is_wheel(SDL_JoystickGUID guid);
bool wheel_extras_is_part(SDL_JoystickGUID guid);

/* A pedal on another device: how far it is pressed, 0 to 32767 (a game controller's trigger's); 0 where
   there is none or its device is not here */
int wheel_extras_pedal(int which);

/* A button on another device: held (an axis: past the middle of its travel) */
bool wheel_extras_button(int which);

#endif
