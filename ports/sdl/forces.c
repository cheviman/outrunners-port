/**
 *    Name:        forces.c
 *    Description: the forces the controllers give back: a racing wheel's centring spring, and the cabinet's
 *                 wheel motor as a wheel's shake, a game controller's rumble or a phone's vibration; and the
 *                 tyres sliding as a lighter shake and an eased spring
 *    Version:     0.2.0
 *    Created:     06-11-2026
 *    Modified:    26-11-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The cabinet's wheel came back to the centre on a spring of its own: the game never pushes it there. The
 *    one force the board drives is the wheel's motor, on or off (output port D bit 4 of each seat's I/O chip,
 *    while the test mode's STEERING WHEEL REACTION is on), which shakes the wheel in a crash and off the road
 *    (src/race/player_car.c, player_car_off_road_shake). A racing wheel with force feedback (a Thrustmaster
 *    T300RS, a Logitech G29) gives no force the game does not ask for, so with nothing asked it turns loose:
 *    this asks it for the cabinet's spring, and for the motor's shake while the motor runs.
 *    - the spring: SDL's spring effect (SDL_HAPTIC_SPRING) at 'centring' percent of the wheel's strongest,
 *      round the wheel's centre, for as long as the game runs; where the wheel has none, its autocentre
 *      (SDL_HAPTIC_AUTOCENTER) at that strength.
 *    - the shake: a sine (SDL_HAPTIC_SINE, SHAKE_PERIOD) at 'shake' percent of the wheel's strongest while the
 *      motor runs. A device with no sine (a game controller: an Xbox, a DualSense...) rumbles instead.
 *    A device is looked at once, the first frame it is a player's game controller with a force wanted: its
 *    haptic is kept where it has one of those effects, and closed where it has none (a game controller's,
 *    whose rumble is SDL's game controller rumble). SDL opening a haptic turns the device's own autocentre off,
 *    so with CENTRING at 0 and VIBRATION at 0 the haptic is not opened at all and the wheel is as its driver
 *    leaves it. The effects go along the wheel's one axis (SDL_HAPTIC_CARTESIAN, 1: DirectInput takes no polar
 *    direction for a device of one axis).
 *    An effect that lasts for ever (SDL_HAPTIC_INFINITY) does not on Windows nor on macOS: SDL gives its length
 *    there in microseconds in 32 bits (length * 1000: 71 minutes), and a wheel's driver may keep less of it (a
 *    Logitech G920's spring was gone a couple of bends into a race, issue 46). So the effects are run over and over
 *    (SDL_HAPTIC_INFINITY times), and the spring is looked after: once a second where the device says it is not
 *    playing (SDL_HAPTIC_STATUS), and every SPRING_RENEW frames whatever it says, it is run again, as after the
 *    game was held (the frames here stop then).
 *
 *    The port's own, none of the cabinet's (issue 48, as Cannonball-DX does for OutRun): while the car's tyres
 *    slide past their grip (car skidding: the tyres' noise and smoke) the same shake at 'slip' percent, under the
 *    motor's (which goes first), and the spring eased to SLIP_SPRING percent of it, as a wheel goes light when
 *    the tyres let go. A slide is held SLIP_HOLD frames past its end, so a car at the edge of its grip, sliding
 *    every other frame, does not start and stop the effects each frame.
 *
 *    A player with no game controller plays on the device itself (a phone's touch controls): the device's own
 *    vibrator, which SDL lists on Android as a haptic of its own named VIBRATOR_SERVICE (rumble only), vibrates
 *    while the motor runs. Android starts a vibration again at each call (vibrate), so it is asked for a span
 *    longer than a few frames (VIBRATOR_SPAN) and again before it ends (VIBRATOR_RENEW frames), and stopped
 *    as the motor stops.
 */
#include "forces.h"

#include <stdio.h>
#include <string.h>

#define FORCES_PLAYERS 2
#define SHAKE_PERIOD   40     /* ms: the shake's, 25 a second */
#define SPRING_WATCH   60     /* frames: the spring's status asked for once a second... */
#define SPRING_RENEW   600    /* ...and the spring run again every 10 seconds */
#define RUMBLE_SPAN    100   /* ms: a rumble asked for each frame while the motor runs, lasting past the next */
#define VIBRATOR_NAME  "VIBRATOR_SERVICE"    /* the device's own, as SDL's Android haptics name it */
#define VIBRATOR_SPAN  250    /* ms: a phone's vibration asked for at once... */
#define VIBRATOR_RENEW 10     /* ...and again after this many frames (167 ms), before it ends */
#define SLIP_HOLD      8      /* frames: a slide felt past its end (133 ms) */
#define SLIP_SPRING    67     /* percent: the spring's strength while the tyres slide (Cannonball-DX's) */

typedef struct
{
	SDL_JoystickID pad;      /* the player's game controller looked at, or 0 (SDL 3's none) */
	bool looked;             /* its haptic opened (or found to have nothing) */
	SDL_Haptic *haptic;      /* its haptic, where it has a spring, an autocentre or a sine */
	unsigned int kinds;      /* the haptic's effects (SDL_HAPTIC_*) */
	int spring;              /* the spring effect, or -1 */
	int centring;            /* the spring's strength (percent), as asked of the haptic */
	int spring_frames;       /* since the spring was last run */
	bool spring_ended;       /* the spring found not playing (said once in the log) */
	int sine;                /* the shake's effect, or -1 */
	int shake;               /* its strength (percent), as asked of the haptic */
	bool shaking;            /* the shake running */
	bool rumbling;           /* a rumble asked for, not yet stopped */
	int slip_frames;         /* the slide still felt for this many frames (SLIP_HOLD from its last) */
} player_forces;

static player_forces players[FORCES_PLAYERS];
static bool haptics;    /* SDL's haptic part started */

/* The device's own vibrator (forces_device) */
static struct
{
	bool looked;
	SDL_Haptic *haptic;
	bool vibrating;
	int frames;              /* since it was last asked to vibrate */
} vibrator;

static void forget(player_forces *forces)
{
	forces->pad = 0;
	forces->looked = false;
	forces->haptic = NULL;
	forces->kinds = 0;
	forces->spring = -1;
	forces->centring = 0;
	forces->spring_frames = 0;
	forces->spring_ended = false;
	forces->sine = -1;
	forces->shake = 0;
	forces->shaking = false;
	forces->rumbling = false;
	forces->slip_frames = 0;
}

void forces_start(void)
{
	for (int player = 0; player < FORCES_PLAYERS; player++)
		forget(&players[player]);
	haptics = SDL_InitSubSystem(SDL_INIT_HAPTIC);
}

/* The effects' direction: along the wheel's axis */
static void wheel_direction(SDL_HapticDirection *direction)
{
	direction->type = SDL_HAPTIC_CARTESIAN;
	direction->dir[0] = 1;
}

/* The spring round the wheel's centre at 'centring' percent: as hard each way, with no dead band */
static void make_spring(SDL_HapticEffect *effect, int centring)
{
	const Sint16 coefficient = (Sint16)(0x7fff * centring / 100);

	SDL_zerop(effect);
	effect->type = SDL_HAPTIC_SPRING;
	wheel_direction(&effect->condition.direction);
	effect->condition.length = SDL_HAPTIC_INFINITY;
	for (int axis = 0; axis < 3; axis++)
	{
		effect->condition.right_sat[axis] = 0xffff;
		effect->condition.left_sat[axis] = 0xffff;
		effect->condition.right_coeff[axis] = coefficient;
		effect->condition.left_coeff[axis] = coefficient;
	}
}

/* The motor's shake at 'shake' percent */
static void make_sine(SDL_HapticEffect *effect, int shake)
{
	SDL_zerop(effect);
	effect->type = SDL_HAPTIC_SINE;
	wheel_direction(&effect->periodic.direction);
	effect->periodic.length = SDL_HAPTIC_INFINITY;
	effect->periodic.period = SHAKE_PERIOD;
	effect->periodic.magnitude = (Sint16)(0x7fff * shake / 100);
}

/* The device's haptic, kept where it has an effect of ours */
static void look(player_forces *forces, SDL_Joystick *joystick)
{
	SDL_Haptic *haptic;
	unsigned int kinds;

	forces->looked = true;
	if (!haptics || !SDL_IsJoystickHaptic(joystick))
		return;
	haptic = SDL_OpenHapticFromJoystick(joystick);
	if (!haptic)
	{
		fprintf(stderr, "forces: %s: %s\n", SDL_GetJoystickName(joystick), SDL_GetError());
		return;
	}
	kinds = SDL_GetHapticFeatures(haptic);
	if (!(kinds & (SDL_HAPTIC_SPRING | SDL_HAPTIC_AUTOCENTER | SDL_HAPTIC_SINE)))
	{
		SDL_CloseHaptic(haptic);
		return;
	}
	forces->haptic = haptic;
	forces->kinds = kinds;
	fprintf(stderr, "forces: %s:%s%s%s\n", SDL_GetJoystickName(joystick),
		(kinds & SDL_HAPTIC_SPRING) ? " spring" : ((kinds & SDL_HAPTIC_AUTOCENTER) ? " autocentre" : ""),
		(kinds & SDL_HAPTIC_SINE) ? " sine" : "", (kinds & SDL_HAPTIC_GAIN) ? " gain" : "");
}

/* The spring at 'centring' percent (0: none), where it changed */
static void centre(player_forces *forces, int centring)
{
	SDL_HapticEffect effect;

	if (centring == forces->centring)
		return;
	forces->centring = centring;
	if (forces->kinds & SDL_HAPTIC_SPRING)
	{
		if (centring == 0)
		{
			if (forces->spring >= 0)
				SDL_DestroyHapticEffect(forces->haptic, forces->spring);
			forces->spring = -1;
			return;
		}
		make_spring(&effect, centring);
		if (forces->spring >= 0)
		{
			SDL_UpdateHapticEffect(forces->haptic, forces->spring, &effect);
			return;
		}
		forces->spring = SDL_CreateHapticEffect(forces->haptic, &effect);
		forces->spring_frames = 0;
		if (forces->spring < 0 || !SDL_RunHapticEffect(forces->haptic, forces->spring, SDL_HAPTIC_INFINITY))
			fprintf(stderr, "forces: the spring: %s\n", SDL_GetError());
	}
	else if (forces->kinds & SDL_HAPTIC_AUTOCENTER)
		SDL_SetHapticAutocenter(forces->haptic, centring);
}

/* The spring kept running (an effect's length ends on Windows and on macOS, above): run again where the device
   says it is not playing, and every SPRING_RENEW frames whatever it says */
static void keep_spring(player_forces *forces)
{
	bool ended = false;

	if (forces->spring < 0)
		return;
	forces->spring_frames++;
	if (forces->spring_frames < SPRING_RENEW)
	{
		if (forces->spring_frames % SPRING_WATCH != 0 || !(forces->kinds & SDL_HAPTIC_STATUS))
			return;
		if (SDL_GetHapticEffectStatus(forces->haptic, forces->spring))
			return;    /* (playing; SDL 3's false is either not playing or the device not saying: run again) */
		ended = true;
	}
	if (ended && !forces->spring_ended)
	{
		fprintf(stderr, "forces: the spring had ended: run again\n");
		forces->spring_ended = true;
	}
	forces->spring_frames = 0;
	if (!SDL_RunHapticEffect(forces->haptic, forces->spring, SDL_HAPTIC_INFINITY) && ended)
		fprintf(stderr, "forces: the spring: %s\n", SDL_GetError());
}

/* The shake at 'shake' percent while 'on', stopped otherwise */
static void shake_wheel(player_forces *forces, int shake, bool on)
{
	SDL_HapticEffect effect;

	if (!on)
	{
		if (forces->shaking)
			SDL_StopHapticEffect(forces->haptic, forces->sine);
		forces->shaking = false;
		return;
	}
	if (forces->sine < 0 || shake != forces->shake)
	{
		make_sine(&effect, shake);
		if (forces->sine < 0)
			forces->sine = SDL_CreateHapticEffect(forces->haptic, &effect);
		else
			SDL_UpdateHapticEffect(forces->haptic, forces->sine, &effect);
		forces->shake = shake;
		if (forces->sine < 0)
			fprintf(stderr, "forces: the shake: %s\n", SDL_GetError());
	}
	if (forces->sine >= 0 && !forces->shaking)
		forces->shaking = SDL_RunHapticEffect(forces->haptic, forces->sine, SDL_HAPTIC_INFINITY);
}

/* A game controller's rumble at 'shake' percent while 'on' (each frame, to last past the next), stopped
   otherwise */
static void rumble(player_forces *forces, SDL_Gamepad *pad, int shake, bool on)
{
	const Uint16 low = (Uint16)(0xffff * shake / 100);

	if (on)
		forces->rumbling = SDL_RumbleGamepad(pad, low, low / 2, RUMBLE_SPAN);
	else if (forces->rumbling)
	{
		SDL_RumbleGamepad(pad, 0, 0, 0);
		forces->rumbling = false;
	}
}

/* The player's haptic closed (its effects with it), their rumble stopped */
static void let_go(player_forces *forces, SDL_Gamepad *pad)
{
	if (forces->haptic)
		SDL_CloseHaptic(forces->haptic);
	if (forces->rumbling && pad)
		SDL_RumbleGamepad(pad, 0, 0, 0);
	forget(forces);
}

void forces_frame(int player, SDL_Gamepad *pad, int centring, int shake, bool motor, int slip, bool sliding)
{
	player_forces *forces;
	SDL_Joystick *joystick = pad ? SDL_GetGamepadJoystick(pad) : NULL;
	const SDL_JoystickID id = joystick ? SDL_GetJoystickID(joystick) : 0;
	int felt;

	if (player < 0 || player >= FORCES_PLAYERS)
		return;
	forces = &players[player];
	if (id != forces->pad)
	{
		/* (another controller: the one before closed already, forces_release) */
		let_go(forces, NULL);
		forces->pad = id;
	}
	if (id == 0)
		return;
	if (sliding && slip > 0)
		forces->slip_frames = SLIP_HOLD;
	else if (forces->slip_frames > 0)
		forces->slip_frames--;
	if (forces->slip_frames > 0)
		centring = centring * SLIP_SPRING / 100;    /* (the tyres let go: the wheel lighter) */
	/* (the shake felt: the motor's, else the slide's) */
	felt = motor && shake > 0 ? shake : (forces->slip_frames > 0 ? slip : 0);
	if (centring == 0 && shake == 0 && slip == 0)
	{
		/* (nothing wanted: the device as its driver has it) */
		if (forces->looked)
		{
			let_go(forces, pad);
			forces->pad = id;
		}
		return;
	}
	if (!forces->looked)
		look(forces, joystick);
	if (forces->haptic)
	{
		centre(forces, centring);
		keep_spring(forces);
	}
	if (forces->haptic && (forces->kinds & SDL_HAPTIC_SINE))
		shake_wheel(forces, felt, felt > 0);
	else
		rumble(forces, pad, felt, felt > 0);
}

/* The device's own vibrator, looked for once: a haptic of SDL's named VIBRATOR_NAME, able to rumble */
static void look_for_vibrator(void)
{
	int count = 0;
	SDL_HapticID *ids = haptics ? SDL_GetHaptics(&count) : NULL;    /* (SDL 3's haptics by their ids) */

	vibrator.looked = true;
	for (int index = 0; index < count; index++)
	{
		const char *name = SDL_GetHapticNameForID(ids[index]);

		if (!name || strcmp(name, VIBRATOR_NAME) != 0)
			continue;
		vibrator.haptic = SDL_OpenHaptic(ids[index]);
		if (vibrator.haptic && !SDL_InitHapticRumble(vibrator.haptic))
		{
			SDL_CloseHaptic(vibrator.haptic);
			vibrator.haptic = NULL;
		}
		if (vibrator.haptic)
			fprintf(stderr, "forces: the device's vibrator\n");
		break;
	}
	SDL_free(ids);
}

static void stop_vibrator(void)
{
	if (vibrator.vibrating)
		SDL_StopHapticRumble(vibrator.haptic);
	vibrator.vibrating = false;
}

void forces_device(int shake, bool motor)
{
	if (!vibrator.looked)
		look_for_vibrator();
	if (!vibrator.haptic)
		return;
	if (!motor || shake <= 0)
	{
		stop_vibrator();
		return;
	}
	if (!vibrator.vibrating || ++vibrator.frames >= VIBRATOR_RENEW)
	{
		vibrator.vibrating = SDL_PlayHapticRumble(vibrator.haptic, (float)shake / 100.0f, VIBRATOR_SPAN);
		vibrator.frames = 0;
	}
}

void forces_hold(void)
{
	for (int player = 0; player < FORCES_PLAYERS; player++)
	{
		player_forces *forces = &players[player];

		if (forces->shaking)
			shake_wheel(forces, forces->shake, false);
		forces->rumbling = false;    /* (a rumble ends by itself, RUMBLE_SPAN after the last frame's) */
		if (forces->spring >= 0)
			forces->spring_frames = SPRING_RENEW - 1;    /* (run again as the game goes on: keep_spring) */
	}
	if (vibrator.haptic)
		stop_vibrator();
}

void forces_release(SDL_Gamepad *pad)
{
	SDL_Joystick *joystick = pad ? SDL_GetGamepadJoystick(pad) : NULL;
	const SDL_JoystickID id = joystick ? SDL_GetJoystickID(joystick) : 0;

	for (int player = 0; player < FORCES_PLAYERS; player++)
		if (id != 0 && players[player].pad == id)
			let_go(&players[player], pad);
}

void forces_stop(void)
{
	for (int player = 0; player < FORCES_PLAYERS; player++)
		let_go(&players[player], NULL);
	if (vibrator.haptic)
	{
		stop_vibrator();
		SDL_CloseHaptic(vibrator.haptic);
		vibrator.haptic = NULL;
	}
}
