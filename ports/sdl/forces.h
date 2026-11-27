/**
 *    Name:        forces.h
 *    Description: the forces the controllers give back: a racing wheel's centring spring, and the cabinet's
 *                 wheel motor as a wheel's shake or a game controller's rumble
 *    Version:     0.1.1
 *    Created:     06-11-2026
 *    Modified:    07-11-2026
 *    Author:      Sebastian Toyos
 */
#ifndef ORUNNERS_PORT_FORCES_H
#define ORUNNERS_PORT_FORCES_H

#include "port_sdl.h"

#include <stdbool.h>

/* SDL's haptic part, where the system has one (without it a wheel gets no spring nor shake; game controllers
   still rumble) */
void forces_start(void);

/* Once a frame, for each player: their game controller (or NULL), the spring that centres a racing wheel
   ('centring', percent of the wheel's strongest: 0 none), how hard it shakes or rumbles ('shake', percent:
   0 never) while the cabinet's wheel motor runs ('motor': its seat's output port D bit 4), and how hard while
   the car's tyres slide ('slip', percent: 0 never, nor the spring eased then; 'sliding': console_tyres_sliding) */
void forces_frame(int player, SDL_Gamepad *pad, int centring, int shake, bool motor, int slip, bool sliding);

/* Once a frame: the device's own vibrator (a phone's), for the players with no game controller (the touch
   controls): it vibrates at 'shake' percent while 'motor' (one of those players' seats' wheel motor) runs, and at
   'slip' percent while 'sliding' (one of those players' cars' tyres, console_tyres_sliding) */
void forces_device(int shake, bool motor, int slip, bool sliding);

/* The game held (the pause, a network race waiting): the shakes and rumbles stopped, the springs kept */
void forces_hold(void);

/* Before a game controller is closed: its forces stopped and its haptic closed */
void forces_release(SDL_Gamepad *pad);

/* As the game ends: every force stopped and every haptic closed */
void forces_stop(void);

#endif
