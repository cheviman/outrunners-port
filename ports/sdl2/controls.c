/**
 *    Name:        controls.c
 *    Description: the keys and game controller buttons of each player's controls of the cabinet, and the
 *                 file that keeps them (controls.ini)
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    01-11-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The defaults below; controls.ini, beside the executable, keeps what the console's CONTROLS page
 *    changes: a section a player, "[player 1]", and a line a control, "gas = Up | righttrigger, a" (SDL's names
 *    of the keys, then of the buttons and the triggers), as SegaSonic's. A player's game controller is the one
 *    plugged in in their turn; its left stick turns the wheel besides (the window port). A trigger is one more
 *    button, the pedals' by default, which it presses as far as it goes.
 */
#include "controls.h"

#include <stdio.h>
#include <string.h>

#define NO_KEY     SDL_SCANCODE_UNKNOWN
#define NO_BUTTON  SDL_CONTROLLER_BUTTON_INVALID
#define LINE_SIZE  256

/* controls.ini's version: 2 has the triggers among the buttons (1, before, had none, and the triggers
   pressed the pedals besides) */
#define FILE_VERSION  2

/* The controls' names in controls.ini */
static const char *const control_names[CONTROLS] = {
	[CONTROL_STEER_LEFT] = "steer_left", [CONTROL_STEER_RIGHT] = "steer_right", [CONTROL_GAS] = "gas",
	[CONTROL_BRAKE] = "brake", [CONTROL_SHIFT_UP] = "shift_up", [CONTROL_SHIFT_DOWN] = "shift_down",
	[CONTROL_MUSIC] = "music", [CONTROL_SONG_BACK] = "song_back", [CONTROL_SONG_NEXT] = "song_next",
	[CONTROL_START] = "start",
};

/* Player 1 the arrows, Q and A, Space, [ and ], Enter or 1; player 2 IJKL, O and U, H, N and M, 2 */
static const SDL_Scancode default_keys[CONTROL_PLAYERS][CONTROLS][CONTROL_KEYS] = {
	{
		[CONTROL_STEER_LEFT] = { SDL_SCANCODE_LEFT }, [CONTROL_STEER_RIGHT] = { SDL_SCANCODE_RIGHT },
		[CONTROL_GAS] = { SDL_SCANCODE_UP }, [CONTROL_BRAKE] = { SDL_SCANCODE_DOWN },
		[CONTROL_SHIFT_UP] = { SDL_SCANCODE_Q }, [CONTROL_SHIFT_DOWN] = { SDL_SCANCODE_A },
		[CONTROL_MUSIC] = { SDL_SCANCODE_SPACE }, [CONTROL_SONG_BACK] = { SDL_SCANCODE_LEFTBRACKET },
		[CONTROL_SONG_NEXT] = { SDL_SCANCODE_RIGHTBRACKET }, [CONTROL_START] = { SDL_SCANCODE_RETURN, SDL_SCANCODE_1 },
	},
	{
		[CONTROL_STEER_LEFT] = { SDL_SCANCODE_J }, [CONTROL_STEER_RIGHT] = { SDL_SCANCODE_L },
		[CONTROL_GAS] = { SDL_SCANCODE_I }, [CONTROL_BRAKE] = { SDL_SCANCODE_K },
		[CONTROL_SHIFT_UP] = { SDL_SCANCODE_O }, [CONTROL_SHIFT_DOWN] = { SDL_SCANCODE_U },
		[CONTROL_MUSIC] = { SDL_SCANCODE_H }, [CONTROL_SONG_BACK] = { SDL_SCANCODE_N },
		[CONTROL_SONG_NEXT] = { SDL_SCANCODE_M }, [CONTROL_START] = { SDL_SCANCODE_2 },
	},
};

/* (a control with one button: none is SDL_CONTROLLER_BUTTON_INVALID, not 0, which is A) */
#define ONE_BUTTON(first)  { first, NO_BUTTON, NO_BUTTON }

#ifdef ORUNNERS_ANBERNIC_BUTTONS
/* Anbernic's handhelds (Sebas): the pad's left and right turn the wheel, L1 the gas (R2 too) and L2 the
   brake, Y and B shift up and down, SELECT the music, the pad's down and up the songs */
static const SDL_GameControllerButton default_buttons[CONTROLS][CONTROL_BUTTONS] = {
	[CONTROL_STEER_LEFT] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_LEFT),
	[CONTROL_STEER_RIGHT] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_RIGHT),
	[CONTROL_GAS] = { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, CONTROL_RIGHT_TRIGGER, NO_BUTTON },
	[CONTROL_BRAKE] = ONE_BUTTON(CONTROL_LEFT_TRIGGER),
	[CONTROL_SHIFT_UP] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_Y), [CONTROL_SHIFT_DOWN] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_B),
	[CONTROL_MUSIC] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_BACK),
	[CONTROL_SONG_BACK] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_DOWN),
	[CONTROL_SONG_NEXT] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_UP), [CONTROL_START] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_START),
};
#else
/* Every player's game controller: the pad's left and right turn the wheel (as the stick does), the right
   trigger or A the gas and the left trigger or B the brake, the shoulders shift, Y the music, the pad's down
   and up the songs */
static const SDL_GameControllerButton default_buttons[CONTROLS][CONTROL_BUTTONS] = {
	[CONTROL_STEER_LEFT] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_LEFT),
	[CONTROL_STEER_RIGHT] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_RIGHT),
	[CONTROL_GAS] = { CONTROL_RIGHT_TRIGGER, SDL_CONTROLLER_BUTTON_A, NO_BUTTON },
	[CONTROL_BRAKE] = { CONTROL_LEFT_TRIGGER, SDL_CONTROLLER_BUTTON_B, NO_BUTTON },
	[CONTROL_SHIFT_UP] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER),
	[CONTROL_SHIFT_DOWN] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
	[CONTROL_MUSIC] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_Y), [CONTROL_SONG_BACK] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_DOWN),
	[CONTROL_SONG_NEXT] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_UP), [CONTROL_START] = ONE_BUTTON(SDL_CONTROLLER_BUTTON_START),
};
#endif

control_binding control_bindings[CONTROL_PLAYERS][CONTROLS];

static char file_path[1024];

static void set_defaults(int player)
{
	for (int control = 0; control < CONTROLS; control++)
	{
		control_binding *binding = &control_bindings[player][control];

		binding->name = control_names[control];
		for (int key = 0; key < CONTROL_KEYS; key++)
			binding->keys[key] = default_keys[player][control][key];    /* (0 is SDL_SCANCODE_UNKNOWN: none) */
		for (int button = 0; button < CONTROL_BUTTONS; button++)
			binding->buttons[button] = default_buttons[control][button];
	}
}

static bool is_trigger(SDL_GameControllerButton button)
{
	return button == CONTROL_LEFT_TRIGGER || button == CONTROL_RIGHT_TRIGGER;
}

bool controls_holds(int player, SDL_GameController *pad, int control)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	const control_binding *binding = &control_bindings[player][control];

	for (int key = 0; key < CONTROL_KEYS; key++)
		if (binding->keys[key] != NO_KEY && keys[binding->keys[key]])
			return true;
	for (int button = 0; pad && button < CONTROL_BUTTONS; button++)
		if (binding->buttons[button] != NO_BUTTON && !is_trigger(binding->buttons[button])
			&& SDL_GameControllerGetButton(pad, binding->buttons[button]))
			return true;
	return false;
}

int controls_trigger_travel(int player, SDL_GameController *pad, int control)
{
	const control_binding *binding = &control_bindings[player][control];
	int travel = 0;

	for (int button = 0; pad && button < CONTROL_BUTTONS; button++)
		if (is_trigger(binding->buttons[button]))
		{
			const int axis = SDL_GameControllerGetAxis(pad, binding->buttons[button] == CONTROL_LEFT_TRIGGER
				? SDL_CONTROLLER_AXIS_TRIGGERLEFT : SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

			travel = axis > travel ? axis : travel;
		}
	return travel;
}

/* ---- controls.ini ---- */

static char *trimmed(char *text)
{
	char *end;

	while (*text == ' ' || *text == '\t')
		text++;
	end = text + strlen(text);
	while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
		*--end = 0;
	return text;
}

/* A button's name in controls.ini: SDL's, the triggers' its axes' ("lefttrigger", "righttrigger") */
static const char *button_name(SDL_GameControllerButton button)
{
	if (is_trigger(button))
		return SDL_GameControllerGetStringForAxis(button == CONTROL_LEFT_TRIGGER
			? SDL_CONTROLLER_AXIS_TRIGGERLEFT : SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
	return SDL_GameControllerGetStringForButton(button);
}

static SDL_GameControllerButton named_button(const char *name)
{
	const SDL_GameControllerAxis axis = SDL_GameControllerGetAxisFromString(name);

	if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
		return CONTROL_LEFT_TRIGGER;
	if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
		return CONTROL_RIGHT_TRIGGER;
	return SDL_GameControllerGetButtonFromString(name);
}

/* One line "name = Key, Key | button, button": the player's control it names takes those, in place of its
   own */
static void read_line(int player, char *line)
{
	char *equals = strchr(line, '='), *bar, *part, *next;
	const char *name;
	control_binding *binding;
	int control, slot;

	if (line[0] == ';' || line[0] == '#' || !equals)
		return;
	*equals = 0;
	name = trimmed(line);
	for (control = 0; control < CONTROLS && strcmp(control_names[control], name) != 0; control++)
		;
	if (control == CONTROLS)
		return;
	binding = &control_bindings[player][control];
	bar = strchr(equals + 1, '|');
	if (bar)
		*bar = 0;
	for (slot = 0; slot < CONTROL_KEYS; slot++)
		binding->keys[slot] = NO_KEY;
	for (slot = 0; slot < CONTROL_BUTTONS; slot++)
		binding->buttons[slot] = NO_BUTTON;
	for (part = equals + 1, slot = 0; part && slot < CONTROL_KEYS; part = next)
	{
		next = strchr(part, ',');
		if (next)
			*next++ = 0;
		if (*trimmed(part))
			binding->keys[slot++] = SDL_GetScancodeFromName(trimmed(part));
	}
	for (part = bar ? bar + 1 : NULL, slot = 0; part && slot < CONTROL_BUTTONS; part = next)
	{
		next = strchr(part, ',');
		if (next)
			*next++ = 0;
		if (*trimmed(part))
			binding->buttons[slot++] = named_button(trimmed(part));
	}
}

/* A file of version 1 (no triggers among its buttons, which pressed the pedals besides): the right trigger
   one more button of the gas, the left one of the brake, after the file's own */
static void add_pedal_trigger(int player, int control, SDL_GameControllerButton trigger)
{
	control_binding *binding = &control_bindings[player][control];
	int slot;

	for (slot = 0; slot < CONTROL_BUTTONS && binding->buttons[slot] != trigger; slot++)
		if (binding->buttons[slot] == NO_BUTTON)
		{
			binding->buttons[slot] = trigger;
			return;
		}
}

void controls_open(const char *path)
{
	FILE *file;
	char line[LINE_SIZE];
	int player = 0, number = 0, version = 1;

	for (int each = 0; each < CONTROL_PLAYERS; each++)
		set_defaults(each);
	SDL_strlcpy(file_path, path, sizeof(file_path));
	file = fopen(file_path, "r");
	if (!file)
	{
		controls_save();
		return;
	}
	while (fgets(line, sizeof(line), file))
	{
		/* "version = N" before the players; "[player N]" starts that player's lines (the lines before any are
		   player 1's) */
		if (sscanf(trimmed(line), "version = %d", &number) == 1)
			version = number;
		else if (sscanf(trimmed(line), "[player %d]", &number) == 1)
			player = number >= 1 && number <= CONTROL_PLAYERS ? number - 1 : -1;
		else if (player >= 0)
			read_line(player, line);
	}
	fclose(file);
	if (version < FILE_VERSION)
	{
		for (int each = 0; each < CONTROL_PLAYERS; each++)
		{
			add_pedal_trigger(each, CONTROL_GAS, CONTROL_RIGHT_TRIGGER);
			add_pedal_trigger(each, CONTROL_BRAKE, CONTROL_LEFT_TRIGGER);
		}
		controls_save();
	}
}

void controls_save(void)
{
	FILE *file = fopen(file_path, "w");

	if (!file)
		return;
	fputs("; The console's controls, a section a player: a control a line, its keys (SDL's names), then after |\n"
		"; its game controller buttons and triggers. OPTIONS > CONTROLS writes this file; it can be changed by\n"
		"; hand too.\n", file);
	fprintf(file, "version = %d\n", FILE_VERSION);
	for (int player = 0; player < CONTROL_PLAYERS; player++)
	{
		fprintf(file, "\n[player %d]\n", player + 1);
		for (int control = 0; control < CONTROLS; control++)
		{
			const control_binding *binding = &control_bindings[player][control];
			bool first = true;

			fprintf(file, "%s =", binding->name);
			for (int key = 0; key < CONTROL_KEYS; key++)
				if (binding->keys[key] != NO_KEY)
				{
					fprintf(file, "%s %s", first ? "" : ",", SDL_GetScancodeName(binding->keys[key]));
					first = false;
				}
			fputs(" |", file);
			first = true;
			for (int button = 0; button < CONTROL_BUTTONS; button++)
				if (binding->buttons[button] != NO_BUTTON)
				{
					fprintf(file, "%s %s", first ? "" : ",", button_name(binding->buttons[button]));
					first = false;
				}
			fputs("\n", file);
		}
	}
	fclose(file);
}

void controls_defaults(int player)
{
	set_defaults(player);
	controls_save();
}

/* The key or button the player pressed for 'control': its first one, the ones it had before moving on (one
   that was already the control's moves to the front, the last falls off); the player's other controls let go
   of it */
void controls_bind_key(int player, int control, SDL_Scancode key)
{
	control_binding *binding = &control_bindings[player][control];
	int slot;

	for (int other = 0; other < CONTROLS; other++)
	{
		SDL_Scancode *keys = control_bindings[player][other].keys;

		for (slot = CONTROL_KEYS - 1; other != control && slot >= 0; slot--)
			if (keys[slot] == key)
			{
				memmove(&keys[slot], &keys[slot + 1], (size_t)(CONTROL_KEYS - 1 - slot) * sizeof(keys[0]));
				keys[CONTROL_KEYS - 1] = NO_KEY;
			}
	}
	for (slot = 0; slot < CONTROL_KEYS - 1 && binding->keys[slot] != key; slot++)
		;
	memmove(&binding->keys[1], &binding->keys[0], (size_t)slot * sizeof(binding->keys[0]));
	binding->keys[0] = key;
	controls_save();
}

void controls_bind_button(int player, int control, SDL_GameControllerButton button)
{
	control_binding *binding = &control_bindings[player][control];
	int slot;

	for (int other = 0; other < CONTROLS; other++)
	{
		SDL_GameControllerButton *buttons = control_bindings[player][other].buttons;

		for (slot = CONTROL_BUTTONS - 1; other != control && slot >= 0; slot--)
			if (buttons[slot] == button)
			{
				memmove(&buttons[slot], &buttons[slot + 1], (size_t)(CONTROL_BUTTONS - 1 - slot) * sizeof(buttons[0]));
				buttons[CONTROL_BUTTONS - 1] = NO_BUTTON;
			}
	}
	for (slot = 0; slot < CONTROL_BUTTONS - 1 && binding->buttons[slot] != button; slot++)
		;
	memmove(&binding->buttons[1], &binding->buttons[0], (size_t)slot * sizeof(binding->buttons[0]));
	binding->buttons[0] = button;
	controls_save();
}
