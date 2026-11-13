/**
 *    Name:        wheel_extras.c
 *    Description: OPTIONS > STEERING's SET UP WHEEL on more than one device: a racing wheel's pedals and buttons
 *                 on devices of their own (pedals, a shifter), read beside the wheel's game controller
 *    Version:     0.1.0
 *    Created:     13-11-2026
 *    Modified:    13-11-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    A wheel whose pedals and shifter are devices of their own (issue 28: a wheel, pedals and a shifter on USB
 *    each). SDL's game controller is one device, so the wheel's game controller layout (wheel_setup.c) keeps
 *    what is on the wheel, as before, and what SET UP WHEEL took on another device comes here: kept in
 *    wheel_devices.txt, a line a control, those devices opened as joysticks as they come, and read once a
 *    frame beside the wheel's game controller by its player (main.c), as its triggers and buttons are. A wheel
 *    with every control its own writes no line: the game controller alone, as before.
 *
 *    The file: "wheel GUID", then "gas GUID axis half inverted", "brake ...", "shift_up GUID button",
 *    "shift_down ...", "start ..." for those on another device.
 */
#include "wheel_extras.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARTS 4    /* the devices but the wheel's, open at most */

static const char *const names[WHEEL_EXTRAS] = { "gas", "brake", "shift_up", "shift_down", "start" };

static struct
{
	char path[1024];
	bool used;                     /* a wheel with controls on other devices */
	SDL_JoystickGUID wheel;
	wheel_extra extras[WHEEL_EXTRAS];
	SDL_Joystick *parts[PARTS];    /* those devices, open */
} kept;

static bool same(SDL_JoystickGUID one, SDL_JoystickGUID other)
{
	return memcmp(one.data, other.data, sizeof(one.data)) == 0;
}

static bool is_wanted(SDL_JoystickGUID guid)
{
	for (int which = 0; kept.used && which < WHEEL_EXTRAS; which++)
		if (kept.extras[which].used && same(kept.extras[which].device, guid))
			return true;
	return false;
}

/* (every device here that has one of the controls, opened) */
static void open_parts(void)
{
	for (int index = 0; index < SDL_NumJoysticks(); index++)
		wheel_extras_device_added(index);
}

static void close_parts(void)
{
	for (int slot = 0; slot < PARTS; slot++)
		if (kept.parts[slot])
		{
			SDL_JoystickClose(kept.parts[slot]);
			kept.parts[slot] = NULL;
		}
}

void wheel_extras_open(const char *path)
{
	FILE *file;
	char line[256];

	SDL_strlcpy(kept.path, path, sizeof(kept.path));
	memset(kept.extras, 0, sizeof(kept.extras));
	kept.used = false;
	if ((file = fopen(path, "rb")) == NULL)
		return;
	while (fgets(line, sizeof(line), file))
	{
		char name[16], guid[64];
		int index = 0, half = 0, inverted = 0;
		const int fields = sscanf(line, "%15s %63s %d %d %d", name, guid, &index, &half, &inverted);

		if (fields < 2 || name[0] == '#')
			continue;
		if (strcmp(name, "wheel") == 0)
		{
			kept.wheel = SDL_JoystickGetGUIDFromString(guid);
			kept.used = true;
			continue;
		}
		for (int which = 0; which < WHEEL_EXTRAS && fields >= 3; which++)
			if (strcmp(name, names[which]) == 0)
			{
				kept.extras[which].used = true;
				kept.extras[which].device = SDL_JoystickGetGUIDFromString(guid);
				kept.extras[which].index = index;
				kept.extras[which].half = half;
				kept.extras[which].inverted = inverted != 0;
			}
	}
	fclose(file);
}

void wheel_extras_save(SDL_JoystickGUID wheel, const wheel_extra extras[WHEEL_EXTRAS])
{
	FILE *file;
	char guid[64];
	bool any = false;

	for (int which = 0; which < WHEEL_EXTRAS; which++)
		any = any || extras[which].used;
	close_parts();
	kept.used = any;
	kept.wheel = wheel;
	memcpy(kept.extras, extras, sizeof(kept.extras));
	if ((file = fopen(kept.path, "wb")) == NULL)
	{
		fprintf(stderr, "%s cannot be written\n", kept.path);
		return;
	}
	fputs("# OPTIONS > STEERING > SET UP WHEEL: the wheel's controls on other devices (pedals, a shifter), a line\n"
		"# a control: the device's GUID, then a pedal's axis, half (0 the whole axis) and inversion, or a button.\n",
		file);
	if (any)
	{
		SDL_JoystickGetGUIDString(wheel, guid, sizeof(guid));
		fprintf(file, "wheel %s\n", guid);
	}
	for (int which = 0; any && which < WHEEL_EXTRAS; which++)
	{
		if (!extras[which].used)
			continue;
		SDL_JoystickGetGUIDString(extras[which].device, guid, sizeof(guid));
		if (which <= WHEEL_EXTRA_BRAKE)
			fprintf(file, "%s %s %d %d %d\n", names[which], guid, extras[which].index, extras[which].half,
				extras[which].inverted ? 1 : 0);
		else
			fprintf(file, "%s %s %d\n", names[which], guid, extras[which].index);
	}
	fclose(file);
	open_parts();
}

void wheel_extras_device_added(int device_index)
{
	const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device_index);
	int free_slot = -1;

	if (!is_wanted(SDL_JoystickGetDeviceGUID(device_index)))
		return;
	for (int slot = 0; slot < PARTS; slot++)
	{
		if (kept.parts[slot] && SDL_JoystickInstanceID(kept.parts[slot]) == id)
			return;    /* (open already) */
		if (!kept.parts[slot] && free_slot < 0)
			free_slot = slot;
	}
	if (free_slot >= 0)
		kept.parts[free_slot] = SDL_JoystickOpen(device_index);
}

void wheel_extras_device_removed(SDL_JoystickID which)
{
	for (int slot = 0; slot < PARTS; slot++)
		if (kept.parts[slot] && SDL_JoystickInstanceID(kept.parts[slot]) == which)
		{
			SDL_JoystickClose(kept.parts[slot]);
			kept.parts[slot] = NULL;
		}
}

bool wheel_extras_is_wheel(SDL_JoystickGUID guid)
{
	return kept.used && same(kept.wheel, guid);
}

bool wheel_extras_is_part(SDL_JoystickGUID guid)
{
	return is_wanted(guid) && !wheel_extras_is_wheel(guid);
}

/* The open device of a control */
static SDL_Joystick *part_of(int which)
{
	if (!kept.used || which < 0 || which >= WHEEL_EXTRAS || !kept.extras[which].used)
		return NULL;
	for (int slot = 0; slot < PARTS; slot++)
		if (kept.parts[slot] && same(SDL_JoystickGetGUID(kept.parts[slot]), kept.extras[which].device))
			return kept.parts[slot];
	return NULL;
}

int wheel_extras_pedal(int which)
{
	SDL_Joystick *part = which <= WHEEL_EXTRA_BRAKE ? part_of(which) : NULL;
	const wheel_extra *extra = &kept.extras[which];
	int value, travel;

	if (!part)
		return 0;
	value = SDL_JoystickGetAxis(part, extra->index);
	if (extra->half != 0)
		travel = extra->half > 0 ? value : -value;    /* (the half that way, as SDL's +a / -a) */
	else
		travel = ((extra->inverted ? -value - 1 : value) + 32768) / 2;    /* (the whole axis, as SDL's a / a~) */
	return travel < 0 ? 0 : (travel > 32767 ? 32767 : travel);
}

bool wheel_extras_button(int which)
{
	SDL_Joystick *part = which > WHEEL_EXTRA_BRAKE ? part_of(which) : NULL;

	return part && SDL_JoystickGetButton(part, kept.extras[which].index) != 0;
}
