/**
 *    Name:        menu.c
 *    Description: the console's menus on the left seat's screen: PRESS START in the attract mode, the title's
 *                 menu, DELUXE and OPTIONS with its pages
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The menus are the game's sprite letters (draw.c) over the title: its menu under the logo, where the
 *    prompt was, with a shadow behind it; OPTIONS over the title's opening (the palm beach and the cars
 *    going by, before the logo comes), held there and darkened. They run on the left seat's frame, at 30 a
 *    second, from the actions the port hands (console_actions).
 */
#include "console.h"
#include "console_internal.h"
#include "console_draw.h"

#include "game.h"

#include <stdio.h>

#define SCREEN_WIDTH        320
#define ITEM_COLOUR         0x0090u    /* the font's colours on the title: white, as the game's credits line */
#define CHOSEN_COLOUR       0x00F0u    /* orange to yellow */
#define TITLE_COLOUR        0x0100u    /* light yellow */
#define PROMPT_Y            186        /* PRESS START, where the game's prompt is */
#define MENU_Y              124        /* the title's menu, under the logo */
#define ITEM_STEP           13         /* a menu line, the font as it is */
#define PAGE_Y              40         /* OPTIONS' pages: their first line */
#define PAGE_STEP           14
#define TITLE_HELD          0x10u      /* the title's opening frames, kept from running out (title_screen frames) */
#define TITLE_TIME          0x1C2u     /* the title's 15 seconds (mode_timer) */
#define DIFFICULTIES        8
#define TIME_STAGES         6
#define TIME_RANGE          5          /* seconds more or less */
#define SOUND_STOP          0x1Fu      /* the sound program's command: every sound off */
#define CONTROLS_Y          38         /* CONTROLS' PLAYER line */
#define CONTROLS_HEADING_Y  14

enum
{
	PAGE_NONE,
	PAGE_MENU,
	PAGE_DELUXE,
	PAGE_OPTIONS,
	PAGE_TIME,
	PAGE_SOUND_TEST,
	PAGE_VIDEO,
	PAGE_CONTROLS
};

static const char *const menu_items[] = { "ARCADE", "2 PLAYERS", "DELUXE", "OPTIONS", "EXIT" };
static const char *const deluxe_items[] = { "TIME ATTACK", "FULL", "REVERSE", "BACK" };
static const char *const options_items[] = { "CONTROLS", "DIFFICULTY", "TIME", "VIDEO", "VOLUME", "SOUND TEST",
	"BACK" };
static const char *const time_stages[TIME_STAGES] = { "PRE-STAGE", "STAGE 1", "STAGE 2", "STAGE 3", "STAGE 4",
	"FINAL STAGE" };
static const char *const pixel_kinds[PIXELS_KINDS] = { "SHARP", "SMOOTH", "BILINEAR", "CRT" };
static const char *const control_names[CONTROLS] = { "STEER LEFT", "STEER RIGHT", "GAS", "BRAKE", "SHIFT UP",
	"SHIFT DOWN", "MUSIC", "SONG <<", "SONG >>", "START" };
/* the test mode's GAME DIFFICULTY, from EASY to HARD on its bar of 8 (the fourth, the game's own) */
static const char *const difficulties[DIFFICULTIES] = { "VERY EASY", "EASIER", "EASY", "NORMAL", "HARD", "HARDER",
	"VERY HARD", "HARDEST" };

#define COUNT(list) ((int)(sizeof(list) / sizeof((list)[0])))

enum { MENU_ARCADE, MENU_TWO_PLAYERS, MENU_DELUXE, MENU_OPTIONS, MENU_EXIT };
enum { OPTIONS_CONTROLS, OPTIONS_DIFFICULTY, OPTIONS_TIME, OPTIONS_VIDEO, OPTIONS_VOLUME, OPTIONS_SOUND_TEST,
	OPTIONS_BACK };
enum { VIDEO_WINDOW, VIDEO_FULL_SCREEN, VIDEO_PIXELS, VIDEO_WIDE, VIDEO_TWO_PLAYERS, VIDEO_BACK, VIDEO_ITEMS };
/* CONTROLS' lines: PLAYER, the controls, DEFAULTS, BACK */
enum { CONTROLS_PLAYER, CONTROLS_FIRST, CONTROLS_DEFAULTS = CONTROLS_FIRST + CONTROLS, CONTROLS_BACK,
	CONTROLS_LINES };

static struct
{
	int page;
	int choice;             /* the line chosen on the page */
	int options_choice;     /* OPTIONS' line, kept while one of its pages is open */
	int blink;              /* frames, for PRESS START */
	int sound;              /* SOUND TEST's number: the test mode's list of sound codes */
	bool settings_changed;  /* DIFFICULTY or TIME: saved into the EEPROM when OPTIONS closes */
	int controls_player;    /* CONTROLS: whose */
	int controls_waiting;   /* CONTROLS: the control waiting for its key or button, or -1 */
	bool controls_defaults; /* CONTROLS: DEFAULTS picked, for the port */
} menu = { .controls_waiting = -1 };

console_window console_window_settings = { 2, false, PIXELS_SHARP, false, false, 5, false };

/* ---- drawing ---- */

static void centred(int y, int scale, uint16_t colour, const char *text)
{
	console_text((SCREEN_WIDTH - console_text_width(text, scale)) / 2, y, scale, colour, text);
}

/* A page's line: its name on the left, its value (if any) on the right, the chosen one in its colour */
static void line(int index, const char *name, const char *value)
{
	const uint16_t colour = index == menu.choice ? CHOSEN_COLOUR : ITEM_COLOUR;
	const int y = PAGE_Y + 16 + index * PAGE_STEP;

	console_text(48, y, 1, colour, name);
	if (value)
		console_text(SCREEN_WIDTH - 48 - console_text_width(value, 1), y, 1, colour, value);
}

/* The page's heading, and the darkened screen its lines are drawn over (drawn last: the list's newest group
   comes first) */
static void page_heading(const char *heading)
{
	centred(PAGE_Y - 8, 2, TITLE_COLOUR, heading);
}

static void darken_page(void)
{
	console_shade(0, 0, SCREEN_WIDTH, 224);
}

/* ---- the attract mode's objects ---- */

uint32_t console_find_object(uint32_t handler, uint32_t from)
{
	const uint32_t pool = screen - 0x7C00u;
	uint32_t object = from ? from + 0x100u : pool;

	for (; object < screen; object += 0x100u)
		if ((OBJECT(object)->flags & 1u) != 0u && OBJECT(object)->handler == handler)
			return object;
	return 0u;
}

/* The title kept on: its 15 seconds from running out, and with OPTIONS its opening from giving way to the
   logo */
static void hold_title(bool opening)
{
	if (SCREEN(screen)->mode_step == 2u)
		SCREEN(screen)->mode_timer = TITLE_TIME;
	if (opening)
	{
		const uint32_t title = console_find_object(title_screen_update_ADDRESS, 0u);

		if (title && OBJECT(title)->state == 1u)
			OBJECT(title)->title_screen.frames = (uint16_t)((OBJECT(title)->title_screen.frames & 0xFF00u) | TITLE_HELD);
	}
}

/* True while the title shows its opening (the title object before its logo) */
static bool title_opening(void)
{
	const uint32_t title = console_find_object(title_screen_update_ADDRESS, 0u);

	return title && OBJECT(title)->state <= 1u;
}

/* ---- the pages ---- */

static int lines_of(int page)
{
	switch (page)
	{
		case PAGE_MENU: return COUNT(menu_items);
		case PAGE_DELUXE: return COUNT(deluxe_items);
		case PAGE_OPTIONS: return COUNT(options_items);
		case PAGE_TIME: return TIME_STAGES + 1;
		case PAGE_SOUND_TEST: return 2;
		case PAGE_VIDEO: return VIDEO_ITEMS;
		case PAGE_CONTROLS: return 0;    /* (its grid moves its own way: controls_move) */
		default: return 0;
	}
}

static void open_page(int page, int choice)
{
	menu.page = page;
	menu.choice = choice;
}

static int step(int value, int by, int low, int high)
{
	value += by;
	return value < low ? high : (value > high ? low : value);
}

/* LEFT or RIGHT on a line with a value: -1, +1 or 0 */
static int change(uint32_t pressed)
{
	return (pressed & ACTION_BIT(ACTION_LEFT)) ? -1 : ((pressed & ACTION_BIT(ACTION_RIGHT)) ? 1 : 0);
}

/* The settings DIFFICULTY and TIME changed, into the EEPROM as the test mode's exit saves them */
static void save_game_settings(void)
{
	if (!menu.settings_changed)
		return;
	menu.settings_changed = false;
	save_settings();
}

static void play_sound_test(void)
{
	const int number = menu.sound;

	queue_sound(SOUND_STOP);
	if (number >= 0x40)
		queue_sound(number >= 0x6B ? 0xD8u : (number >= 0x66 ? 0xDAu : 0xD6u));    /* (its sound set) */
	queue_sound(sound_test_codes[number]);
}

static int sound_test_count(void)
{
	int count = 0;

	while (sound_test_codes[count] != 0u)
		count++;
	return count;
}

/* OPTIONS' lines: LEFT and RIGHT change those with a value */
static void options_frame(uint32_t pressed)
{
	char value[16];
	const int by = change(pressed);

	if (by && menu.choice == OPTIONS_DIFFICULTY)
	{
		game_difficulty = (uint8_t)step(game_difficulty, by, 0, DIFFICULTIES - 1);
		menu.settings_changed = true;
	}
	if (by && menu.choice == OPTIONS_VOLUME)
	{
		console_window_settings.volume = step(console_window_settings.volume, by, 0, 10);
		console_window_settings.changed = true;
	}
	if (pressed & ACTION_BIT(ACTION_ACCEPT))
		switch (menu.choice)
		{
			case OPTIONS_CONTROLS:
				menu.options_choice = menu.choice;
				open_page(PAGE_CONTROLS, CONTROLS_FIRST);
				return;
			case OPTIONS_TIME:
				menu.options_choice = menu.choice;
				open_page(PAGE_TIME, 0);
				return;
			case OPTIONS_SOUND_TEST:
				menu.options_choice = menu.choice;
				open_page(PAGE_SOUND_TEST, 0);
				return;
			case OPTIONS_VIDEO:
				menu.options_choice = menu.choice;
				open_page(PAGE_VIDEO, 0);
				return;
			case OPTIONS_BACK:
				save_game_settings();
				open_page(PAGE_MENU, MENU_OPTIONS);
				return;
			default:
				break;
		}
	page_heading("OPTIONS");
	for (int index = 0; index < COUNT(options_items); index++)
	{
		const char *shown = NULL;

		if (index == OPTIONS_DIFFICULTY)
		{
			snprintf(value, sizeof(value), "< %s >", difficulties[game_difficulty & (DIFFICULTIES - 1)]);
			shown = value;
		}
		else if (index == OPTIONS_VOLUME)
		{
			snprintf(value, sizeof(value), "< %d >", console_window_settings.volume);
			shown = value;
		}
		line(index, options_items[index], shown);
	}
	darken_page();
}

static void time_frame(uint32_t pressed)
{
	char value[16];
	const int by = change(pressed);

	if (by && menu.choice < TIME_STAGES)
	{
		time_difficulty[menu.choice] = (int8_t)step(time_difficulty[menu.choice], by, -TIME_RANGE, TIME_RANGE);
		menu.settings_changed = true;
	}
	if ((pressed & ACTION_BIT(ACTION_ACCEPT)) && menu.choice == TIME_STAGES)
	{
		open_page(PAGE_OPTIONS, menu.options_choice);
		return;
	}
	page_heading("TIME");
	for (int index = 0; index < TIME_STAGES; index++)
	{
		snprintf(value, sizeof(value), "< %+d >", time_difficulty[index]);
		line(index, time_stages[index], value);
	}
	line(TIME_STAGES, "BACK", NULL);
	darken_page();
}

static void sound_test_frame(uint32_t pressed)
{
	char value[16];
	const int by = change(pressed);

	if (by && menu.choice == 0)
		menu.sound = step(menu.sound, by, 0, sound_test_count() - 1);
	if (pressed & ACTION_BIT(ACTION_ACCEPT))
	{
		if (menu.choice == 0)
			play_sound_test();
		else
		{
			queue_sound(SOUND_STOP);
			open_page(PAGE_OPTIONS, menu.options_choice);
			return;
		}
	}
	page_heading("SOUND TEST");
	snprintf(value, sizeof(value), "< %03d >", menu.sound + 1);
	line(0, "SOUND", value);
	line(1, "BACK", NULL);
	darken_page();
}

static void video_frame(uint32_t pressed)
{
	console_window *window = &console_window_settings;
	char value[16];
	const int by = change(pressed);
	const bool accept = (pressed & ACTION_BIT(ACTION_ACCEPT)) != 0;

	if (by || accept)
		switch (menu.choice)
		{
			case VIDEO_WINDOW:
				window->window_scale = step(window->window_scale, by ? by : 1, 1, 6);
				window->changed = true;
				break;
			case VIDEO_FULL_SCREEN:
				window->full_screen = !window->full_screen;
				window->changed = true;
				break;
			case VIDEO_PIXELS:
				window->pixels = step(window->pixels, by ? by : 1, 0, PIXELS_KINDS - 1);
				window->changed = true;
				break;
			case VIDEO_WIDE:
				window->wide = !window->wide;
				window->changed = true;
				break;
			case VIDEO_TWO_PLAYERS:
				window->vertical = !window->vertical;
				window->changed = true;
				break;
			case VIDEO_BACK:
				if (accept)
				{
					open_page(PAGE_OPTIONS, menu.options_choice);
					return;
				}
				break;
		}
	page_heading("VIDEO");
	snprintf(value, sizeof(value), "< %dX >", window->window_scale);
	line(VIDEO_WINDOW, "WINDOW", value);
	line(VIDEO_FULL_SCREEN, "FULL SCREEN", window->full_screen ? "< ON >" : "< OFF >");
	snprintf(value, sizeof(value), "< %s >", pixel_kinds[window->pixels]);
	line(VIDEO_PIXELS, "PIXELS", value);
	line(VIDEO_WIDE, "WIDE", window->wide ? "< ON >" : "< OFF >");
	line(VIDEO_TWO_PLAYERS, "2 PLAYERS", window->vertical ? "< VERTICAL >" : "< HORIZONTAL >");
	line(VIDEO_BACK, "BACK", NULL);
	darken_page();
}

/* CONTROLS: PLAYER on top, the controls in two columns of five (each its name, and under it the pictures of
   its keys and button, the port's), DEFAULTS and BACK side by side at the bottom. The arrows move about it */
#define CONTROLS_COLUMN_ROWS  5
#define CONTROLS_CELL_Y       58     /* the first row of controls: their names */
#define CONTROLS_CELL_STEP    30     /* a name, its pictures under it, a gap */
#define CONTROLS_PICTURES_Y   10     /* under the name */
#define CONTROLS_BOTTOM_Y     210    /* DEFAULTS and BACK */

static const int controls_column_x[2] = { 28, 172 };

static void control_place(int control, int *x, int *y)
{
	*x = controls_column_x[control / CONTROLS_COLUMN_ROWS];
	*y = CONTROLS_CELL_Y + (control % CONTROLS_COLUMN_ROWS) * CONTROLS_CELL_STEP;
}

/* The arrows on the grid: up and down within a column (to PLAYER above it, to DEFAULTS or BACK under it),
   left and right across the columns (on PLAYER: the player) */
static void controls_move(uint32_t pressed)
{
	const int choice = menu.choice;
	const int control = choice - CONTROLS_FIRST;
	const bool on_control = choice >= CONTROLS_FIRST && choice < CONTROLS_DEFAULTS;
	const int column = on_control ? control / CONTROLS_COLUMN_ROWS : (choice == CONTROLS_BACK ? 1 : 0);

	if (pressed & ACTION_BIT(ACTION_UP))
	{
		if (choice == CONTROLS_DEFAULTS || choice == CONTROLS_BACK)
			menu.choice = CONTROLS_FIRST + column * CONTROLS_COLUMN_ROWS + CONTROLS_COLUMN_ROWS - 1;
		else if (on_control && control % CONTROLS_COLUMN_ROWS != 0)
			menu.choice = choice - 1;
		else if (on_control)
			menu.choice = CONTROLS_PLAYER;
		else
			menu.choice = CONTROLS_DEFAULTS;    /* (from PLAYER round to the bottom) */
	}
	else if (pressed & ACTION_BIT(ACTION_DOWN))
	{
		if (choice == CONTROLS_PLAYER)
			menu.choice = CONTROLS_FIRST;
		else if (on_control && control % CONTROLS_COLUMN_ROWS != CONTROLS_COLUMN_ROWS - 1)
			menu.choice = choice + 1;
		else if (on_control)
			menu.choice = column ? CONTROLS_BACK : CONTROLS_DEFAULTS;
		else
			menu.choice = CONTROLS_PLAYER;
	}
	else if (change(pressed))
	{
		if (choice == CONTROLS_PLAYER)
			menu.controls_player = step(menu.controls_player, change(pressed), 0, CONTROL_PLAYERS - 1);
		else if (on_control)
			menu.choice = CONTROLS_FIRST + (control + CONTROLS_COLUMN_ROWS) % CONTROLS;
		else
			menu.choice = choice == CONTROLS_DEFAULTS ? CONTROLS_BACK : CONTROLS_DEFAULTS;
	}
}

static void controls_frame(uint32_t pressed)
{
	char value[24];
	int x, y;

	centred(CONTROLS_HEADING_Y, 2, TITLE_COLOUR, "CONTROLS");
	if (menu.controls_waiting >= 0)
	{
		/* the port takes the next key or button (console_controls_bound) */
		control_place(menu.controls_waiting, &x, &y);
		console_text(x, y, 1, CHOSEN_COLOUR, control_names[menu.controls_waiting]);
		centred(CONTROLS_BOTTOM_Y, 1, TITLE_COLOUR, "PRESS A KEY OR BUTTON");
		darken_page();
		return;
	}
	controls_move(pressed);
	if (pressed & ACTION_BIT(ACTION_ACCEPT))
	{
		if (menu.choice >= CONTROLS_FIRST && menu.choice < CONTROLS_DEFAULTS)
		{
			menu.controls_waiting = menu.choice - CONTROLS_FIRST;
			return;
		}
		if (menu.choice == CONTROLS_DEFAULTS)
			menu.controls_defaults = true;
		if (menu.choice == CONTROLS_BACK)
		{
			open_page(PAGE_OPTIONS, menu.options_choice);
			return;
		}
	}
	snprintf(value, sizeof(value), "< PLAYER %d >", menu.controls_player + 1);
	centred(CONTROLS_Y, 1, menu.choice == CONTROLS_PLAYER ? CHOSEN_COLOUR : ITEM_COLOUR, value);
	for (int control = 0; control < CONTROLS; control++)
	{
		control_place(control, &x, &y);
		console_text(x, y, 1, menu.choice == CONTROLS_FIRST + control ? CHOSEN_COLOUR : ITEM_COLOUR,
			control_names[control]);
	}
	console_text(controls_column_x[0], CONTROLS_BOTTOM_Y, 1,
		menu.choice == CONTROLS_DEFAULTS ? CHOSEN_COLOUR : ITEM_COLOUR, "DEFAULTS");
	console_text(controls_column_x[1], CONTROLS_BOTTOM_Y, 1,
		menu.choice == CONTROLS_BACK ? CHOSEN_COLOUR : ITEM_COLOUR, "BACK");
	darken_page();
}

bool console_controls_place(int control, int *x, int *y)
{
	if (menu.page != PAGE_CONTROLS || menu.controls_waiting >= 0 || control < 0 || control >= CONTROLS)
		return false;
	control_place(control, x, y);
	*y += CONTROLS_PICTURES_Y;
	return true;
}

int console_controls_player(void)
{
	return menu.controls_player;
}

int console_controls_waiting(void)
{
	return menu.page == PAGE_CONTROLS ? menu.controls_waiting : -1;
}

void console_controls_bound(void)
{
	menu.controls_waiting = -1;
}

bool console_controls_defaults(void)
{
	const bool picked = menu.controls_defaults;

	menu.controls_defaults = false;
	return picked;
}

/* The title's menu and DELUXE: their lines centred under the logo, over a shadow */
static void list_frame(const char *const *items, int count)
{
	const int height = count * ITEM_STEP;
	int widest = 0;

	for (int index = 0; index < count; index++)
	{
		const int width = console_text_width(items[index], 1);

		widest = width > widest ? width : widest;
		centred(MENU_Y + index * ITEM_STEP, 1, index == menu.choice ? CHOSEN_COLOUR : ITEM_COLOUR, items[index]);
	}
	console_shade((SCREEN_WIDTH - widest) / 2 - 24, MENU_Y - 8, widest + 48, height + 11);
}

/* ---- the frame ---- */

int console_menu_frame(uint32_t mode)
{
	const uint32_t pressed = console.pressed;
	const int lines = lines_of(menu.page);

	console.pressed = 0;
	if (mode == GAME_MODE_PLAY || mode == GAME_MODE_POWER_ON || mode == GAME_MODE_RESTART)
	{
		if (mode == GAME_MODE_PLAY)
			menu.page = PAGE_NONE;
		return MENU_NOTHING;
	}

	/* the attract mode: PRESS START, the menu on the title */
	if (menu.page == PAGE_NONE)
	{
		if ((++menu.blink & 16) == 0)
			centred(PROMPT_Y, 2, CHOSEN_COLOUR, "PRESS START");
		if (pressed & (ACTION_BIT(ACTION_START) | ACTION_BIT(ACTION_ACCEPT)))
		{
			open_page(PAGE_MENU, MENU_ARCADE);
			if (mode != GAME_MODE_TITLE)
				return MENU_TITLE;
		}
		return MENU_NOTHING;
	}
	if (mode != GAME_MODE_TITLE)
		return MENU_NOTHING;    /* (the title coming back from another mode: the menu stays open) */

	if (menu.page == PAGE_CONTROLS && menu.controls_waiting >= 0)
	{
		/* (the port takes the keys and buttons meanwhile) */
		hold_title(true);
		controls_frame(0);
		return MENU_NOTHING;
	}
	if (lines)
	{
		if (pressed & ACTION_BIT(ACTION_UP))
			menu.choice = step(menu.choice, -1, 0, lines - 1);
		if (pressed & ACTION_BIT(ACTION_DOWN))
			menu.choice = step(menu.choice, 1, 0, lines - 1);
	}
	switch (menu.page)
	{
		case PAGE_MENU:
			hold_title(false);
			if (pressed & ACTION_BIT(ACTION_BACK))
			{
				open_page(PAGE_NONE, 0);
				return MENU_NOTHING;
			}
			if (pressed & (ACTION_BIT(ACTION_ACCEPT) | ACTION_BIT(ACTION_START)))
				switch (menu.choice)
				{
					case MENU_ARCADE:
						open_page(PAGE_NONE, 0);
						return MENU_START_ONE;
					case MENU_TWO_PLAYERS:
						open_page(PAGE_NONE, 0);
						return MENU_START_TWO;
					case MENU_DELUXE:
						open_page(PAGE_DELUXE, 0);
						return MENU_NOTHING;
					case MENU_OPTIONS:
						open_page(PAGE_OPTIONS, OPTIONS_CONTROLS);
						return title_opening() ? MENU_NOTHING : MENU_TITLE;
					case MENU_EXIT:
						console.quit = true;
						return MENU_NOTHING;
				}
			list_frame(menu_items, COUNT(menu_items));
			return MENU_NOTHING;
		case PAGE_DELUXE:
			hold_title(false);
			if ((pressed & ACTION_BIT(ACTION_BACK))
				|| ((pressed & ACTION_BIT(ACTION_ACCEPT)) && menu.choice == COUNT(deluxe_items) - 1))
			{
				open_page(PAGE_MENU, MENU_DELUXE);
				return MENU_NOTHING;
			}
			list_frame(deluxe_items, COUNT(deluxe_items));
			return MENU_NOTHING;
		default:
			break;
	}

	/* OPTIONS and its pages, over the title's opening */
	hold_title(true);
	if (pressed & ACTION_BIT(ACTION_BACK))
	{
		if (menu.page == PAGE_OPTIONS)
		{
			save_game_settings();
			open_page(PAGE_MENU, MENU_OPTIONS);
		}
		else
		{
			if (menu.page == PAGE_SOUND_TEST)
				queue_sound(SOUND_STOP);
			open_page(PAGE_OPTIONS, menu.options_choice);
		}
		return MENU_NOTHING;
	}
	switch (menu.page)
	{
		case PAGE_OPTIONS: options_frame(pressed); break;
		case PAGE_TIME: time_frame(pressed); break;
		case PAGE_SOUND_TEST: sound_test_frame(pressed); break;
		case PAGE_VIDEO: video_frame(pressed); break;
		case PAGE_CONTROLS: controls_frame(pressed); break;
		default: break;
	}
	return MENU_NOTHING;
}
