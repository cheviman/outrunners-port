/**
 *    Name:        first_run.c
 *    Description: the first run: the game's assets out of the player's ROM set, found beside the game (on
 *                 macOS beside its .app) or where it keeps its files, or dropped on a window that asks for it
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    As SegaSonic's first run. The window has none of the game's letters (they are in the ROM): it writes
 *    with Kenney Mini (letters.c; font.bmp beside the executable). The ROM set is read once
 *    (runtime/src/extract.c): the assets come out of it into their folder, and the game never opens it again.
 */
#include "first_run.h"
#include "letters.h"

#include "game/extract.h"

#include <SDL.h>

#include <stdio.h>
#include <string.h>

#define ROM_SET_FILE    "orunners.zip"
#define ROM_SET_FOLDER  "rom"
#define ROM_SET_PROBE   "epr-15620.ic37"    /* a file of the set by MAME's name: a folder of its files */
#define WINDOW_WIDTH    640
#define WINDOW_HEIGHT   448
#define LINE_GAP        8
#define PROBLEM_LINES   4
#define PROBLEM_WIDTH   (WINDOW_WIDTH - 48)

/* The problem in red, in as many lines as it takes to fit the window (up to PROBLEM_LINES), broken after a
   space or a folder's separator */
static void draw_problem(SDL_Renderer *renderer, const letters *font, const char *problem, int y)
{
	const SDL_Color red = { 255, 110, 90, 255 };
	char line[256];

	for (int count = 0; count < PROBLEM_LINES && *problem; count++)
	{
		size_t length = strlen(problem) < sizeof(line) - 1 ? strlen(problem) : sizeof(line) - 1, fits = 0;

		/* (the longest start of what is left that fits, ending at a break; a word too long, cut where it fits) */
		for (size_t end = 1; end <= length; end++)
		{
			memcpy(line, problem, end);
			line[end] = 0;
			if (letters_width(font, line, 1) > PROBLEM_WIDTH)
				break;
			if (end == length || problem[end - 1] == ' ' || problem[end - 1] == '/' || problem[end - 1] == '\\')
				fits = end;
		}
		if (fits == 0)
		{
			for (fits = 1; fits < length; fits++)
			{
				memcpy(line, problem, fits + 1);
				line[fits + 1] = 0;
				if (letters_width(font, line, 1) > PROBLEM_WIDTH)
					break;
			}
		}
		memcpy(line, problem, fits);
		line[fits] = 0;
		letters_draw(renderer, font, line, y, 1, red, WINDOW_WIDTH);
		y += LETTERS_CELL + LINE_GAP / 2;
		problem += fits;
	}
}

static void draw_window(SDL_Renderer *renderer, const letters *font, const char *title, const char *problem, int reading)
{
	const SDL_Color white = { 255, 255, 255, 255 }, orange = { 255, 170, 60, 255 };
	int y = 120;

	SDL_SetRenderDrawColor(renderer, 16, 24, 64, 255);
	SDL_RenderClear(renderer);
	letters_draw(renderer, font, title, y, 3, orange, WINDOW_WIDTH);
	y += LETTERS_CELL * 3 + LINE_GAP * 4;
	if (reading)
		letters_draw(renderer, font, "Reading the ROM set...", y, 2, white, WINDOW_WIDTH);
	else
	{
		letters_draw(renderer, font, "Drop the arcade ROM set here", y, 2, white, WINDOW_WIDTH);
		y += LETTERS_CELL * 2 + LINE_GAP;
		letters_draw(renderer, font, "orunners.zip: MAME's set, World", y, 1, white, WINDOW_WIDTH);
		y += LETTERS_CELL + LINE_GAP;
		letters_draw(renderer, font, "The game takes its graphics and sounds from it, once.", y, 1, white, WINDOW_WIDTH);
		if (problem && problem[0])
			draw_problem(renderer, font, problem, y + LETTERS_CELL + LINE_GAP * 3);
	}
	SDL_RenderPresent(renderer);
}

/* On macOS, the folder the .app is in (the game's base is the .app's Resources folder: where the player sees
   the game is beside the .app); 0 elsewhere */
static int app_folder(const char *base, char *folder, size_t size)
{
#ifdef __APPLE__
	static const char inside[] = ".app/Contents/Resources/";
	const size_t length = base ? strlen(base) : 0, tail = sizeof(inside) - 1;
	size_t end;

	if (length < tail || length >= size || strcmp(base + length - tail, inside) != 0)
		return 0;
	for (end = length - tail; end > 0 && base[end - 1] != '/'; end--)
		;    /* (back over the .app's name) */
	memcpy(folder, base, end);
	folder[end] = '\0';
	return end > 0;
#else
	(void)base;
	(void)folder;
	(void)size;
	return 0;
#endif
}

static int exists(const char *path)
{
	SDL_RWops *probe = SDL_RWFromFile(path, "rb");

	if (!probe)
		return 0;
	SDL_RWclose(probe);
	return 1;
}

/* What went wrong with a ROM set, for the window: a file of the set missing says it is not the set */
static void say_problem(const char *failed, char *problem, size_t size)
{
	snprintf(problem, size, "%s%s", strncmp(failed, "no ", 3) == 0 ? "Not the set: " : "", failed);
	if (problem[0] >= 'a' && problem[0] <= 'z')
		problem[0] = (char)(problem[0] - 'a' + 'A');
}

/* The assets out of the ROM set in 'place' (a folder, with its separator): orunners.zip there, or its folder
   rom/ with the set's files or its .zip. 1 when they came out; a set there they could not come out of leaves
   what went wrong in 'problem' */
static int extracted_from(const char *place, const char *folder, char *problem, size_t size)
{
	char source[1024], probe[1100];
	const char *failed;

	if (!place)
		return 0;
	snprintf(source, sizeof(source), "%s" ROM_SET_FILE, place);
	if (!exists(source))
	{
		snprintf(source, sizeof(source), "%s" ROM_SET_FOLDER, place);
		snprintf(probe, sizeof(probe), "%s/" ROM_SET_FILE, source);
		if (!exists(probe))
		{
			snprintf(probe, sizeof(probe), "%s/" ROM_SET_PROBE, source);
			if (!exists(probe))
				return 0;
		}
	}
	if ((failed = game_assets_extract(source, folder)) == NULL)
		return 1;
	say_problem(failed, problem, size);
	return 0;
}

int first_run(const char *title, const char *base, const char *saves, const char *folder)
{
	SDL_Window *window;
	SDL_Renderer *renderer;
	letters font;
	char problem[400] = "", beside_app[1024];
	int done = 0, running = 1;

	if (extracted_from(base, folder, problem, sizeof(problem))
		|| (app_folder(base, beside_app, sizeof(beside_app)) && extracted_from(beside_app, folder, problem, sizeof(problem)))
		|| ((!base || !saves || strcmp(base, saves) != 0) && extracted_from(saves, folder, problem, sizeof(problem))))
		return 1;
	window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
	renderer = window ? SDL_CreateRenderer(window, -1, 0) : NULL;
	if (!renderer || !letters_load(renderer, base, &font))
	{
		char message[600];

		/* (no window or no letters: the message alone) */
		if (renderer)
			SDL_DestroyRenderer(renderer);
		if (window)
			SDL_DestroyWindow(window);
		snprintf(message, sizeof(message), "The game takes its graphics and sounds from the arcade ROM set (orunners.zip: "
			"MAME's set, World). Put orunners.zip beside the game and start it again.%s%s", problem[0] ? "\n\n" : "", problem);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, message, NULL);
		return 0;
	}
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	draw_window(renderer, &font, title, problem, 0);
	while (running && !done)
	{
		SDL_Event event;

		if (!SDL_WaitEvent(&event))
			break;
		if (event.type == SDL_QUIT)
			running = 0;
		else if (event.type == SDL_DROPFILE)
		{
			const char *failed;

			draw_window(renderer, &font, title, NULL, 1);
			failed = game_assets_extract(event.drop.file, folder);
			SDL_free(event.drop.file);
			if (!failed)
				done = 1;
			else
			{
				say_problem(failed, problem, sizeof(problem));
				draw_window(renderer, &font, title, problem, 0);
			}
		}
		else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED)
			draw_window(renderer, &font, title, problem, 0);
	}
	letters_free(&font);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	return done;
}
