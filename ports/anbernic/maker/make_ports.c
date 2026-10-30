/**
 *    Name:        make_ports.c
 *    Description: OutRunners Anbernic's work: the folder PORTS for an Anbernic H700 handheld's card, made from
 *                 the game's Anbernic build and the player's ROM set (for the command line and the window
 *                 alike)
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    PORTS is what the stock firmware's Roms/PORTS takes: the launcher OutRunners.sh and beside it the folder
 *    outrunners, with the game, CONTROLS' button pictures and the assets pack, which the tool makes from the
 *    player's orunners.zip as the computer's first run does (ports/switch/nsp/rom_pack.c). Written in the
 *    folder the player picks: beside the tool, or the card's Roms itself; the other ports already in a PORTS
 *    there stay as they are, and the game's files are written over once the new pack is whole (a ROM set
 *    that cannot be used leaves the game there as it was). Nothing it reads or writes leaves the player's
 *    computer. The command line (main.c) and the window (window.c) make it here alike.
 */
#include "make_ports.h"
#include "rom_pack.h"

#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define make_folder(path) _mkdir(path)
#else
#define make_folder(path) mkdir(path, 0777)
#endif

#define LAUNCHER      "OutRunners.sh"
#define GAME_FOLDER   "outrunners"
#define ASSETS_FILE   "outrunners.assets"
#define MAKING_SUFFIX ".making"    /* the pack until it is whole */

/* The game's build: the files PORTS gets of it, as they are in it and in PORTS */
static const char *const build_files[] = { LAUNCHER, GAME_FOLDER "/OutRunners", GAME_FOLDER "/input_prompts.bmp" };

#define BUILD_FILES ((int)(sizeof(build_files) / sizeof(build_files[0])))

int ports_file_exists(const char *path)
{
	FILE *probe = fopen(path, "rb");

	if (probe)
		fclose(probe);
	return probe != NULL;
}

int ports_folder_exists(const char *path)
{
	char folder[PORTS_PATH_SIZE];
	struct stat status;
	size_t length;

	snprintf(folder, sizeof(folder), "%s", path[0] ? path : ".");
	length = strlen(folder);
	/* (without its last separator, which Windows' stat does not take but for a drive's root) */
	if (length > 1 && (folder[length - 1] == '/' || folder[length - 1] == '\\') && folder[length - 2] != ':')
		folder[length - 1] = '\0';
	return stat(folder, &status) == 0 && (status.st_mode & S_IFMT) == S_IFDIR;
}

void ports_folder_of(const char *path, char *folder, size_t size)
{
	const char *slash = strrchr(path, '/'), *backslash = strrchr(path, '\\');
	const char *end = backslash > slash ? backslash : slash;
	const size_t length = end ? (size_t)(end - path) + 1 : 0;

	snprintf(folder, size, "%.*s", (int)(length < size ? length : size - 1), path);
}

/* 'folder' and 'name' as one path: a separator between them where the folder has none at its end */
static void joined(const char *folder, const char *name, char *path, size_t size)
{
	const size_t length = strlen(folder);
	const int ends = length == 0 || folder[length - 1] == '/' || folder[length - 1] == '\\';

	snprintf(path, size, "%s%s%s", folder, ends ? "" : "/", name);
}

const char *ports_build_problem(const char *build)
{
	char path[PORTS_PATH_SIZE + 64];

	for (int file = 0; file < BUILD_FILES; file++)
	{
		joined(build, build_files[file], path, sizeof(path));
		if (!ports_file_exists(path))
			return "not found";
	}
	return NULL;
}

/* ---- the inputs ---- */

void ports_take_dropped(ports_inputs *inputs, const char *path)
{
	snprintf(inputs->rom, sizeof(inputs->rom), "%s", path);    /* (orunners.zip, or a folder of its files) */
}

void ports_defaults(ports_inputs *inputs, const char *here)
{
	if (!inputs->rom[0])
		snprintf(inputs->rom, sizeof(inputs->rom), "%sorunners.zip", here);
	if (!inputs->build[0])
		snprintf(inputs->build, sizeof(inputs->build), "%sanbernic", here);
	if (!inputs->out[0])
		snprintf(inputs->out, sizeof(inputs->out), "%s", here);
}

/* ---- PORTS ---- */

static int copy_file(const char *from, const char *to)
{
	static char block[1 << 16];
	FILE *input = fopen(from, "rb"), *output;
	size_t got;
	int copied = 1;

	if (!input)
		return 0;
	if (!(output = fopen(to, "wb")))
	{
		fclose(input);
		return 0;
	}
	while ((got = fread(block, 1, sizeof(block), input)) > 0)
		if (fwrite(block, 1, got, output) != got)
		{
			copied = 0;
			break;
		}
	copied = copied && !ferror(input);
	fclose(input);
	return fclose(output) == 0 && copied;
}

/* The folder at 'path', made when it is missing: true when it is there */
static int folder_made(const char *path)
{
	if (!ports_folder_exists(path))
		make_folder(path);
	return ports_folder_exists(path);
}

/* What failed, and on what (NULL for nothing): false, for the caller to return */
static int failed(ports_outcome *outcome, const char *what, const char *detail)
{
	snprintf(outcome->what, sizeof(outcome->what), "%s", what);
	snprintf(outcome->detail, sizeof(outcome->detail), "%s", detail ? detail : "");
	return 0;
}

static void tell(ports_step step, void *context, const char *name)
{
	if (step)
		step(context, name);
}

int make_ports(const ports_inputs *inputs, FILE *log, ports_step step, void *context, ports_outcome *outcome)
{
	char ports[PORTS_PATH_SIZE + 16], game[PORTS_PATH_SIZE + 32], from[PORTS_PATH_SIZE + 64], to[PORTS_PATH_SIZE + 64];
	char pack[PORTS_PATH_SIZE + 64];
	const char *problem;
	struct stat status;

	memset(outcome, 0, sizeof(*outcome));

	/* the game's Anbernic build */
	tell(step, context, "reading the game's Anbernic build");
	if (ports_build_problem(inputs->build) != NULL)
		return failed(outcome, "The game's Anbernic build is not whole", inputs->build);
	if (log)
		fprintf(log, "game      %s\n", inputs->build);

	/* PORTS and its folder outrunners */
	joined(inputs->out, PORTS_FOLDER, ports, sizeof(ports));
	joined(ports, GAME_FOLDER, game, sizeof(game));
	if (!folder_made(ports) || !folder_made(game))
		return failed(outcome, "The folder PORTS cannot be made", ports);

	/* the assets, as the computer's first run takes them out, under a name of their own until they are
	   whole: a game already there stays as it was when the ROM set cannot be used */
	tell(step, context, "taking the assets out of the ROM set");
	if (log)
		fprintf(log, "ROM set   %s\n", inputs->rom);
	joined(game, ASSETS_FILE MAKING_SUFFIX, pack, sizeof(pack));
	if ((problem = rom_pack_make(inputs->rom, pack)) != NULL)
	{
		remove(pack);
		return failed(outcome, "The ROM set cannot be used", problem);
	}

	/* the game's files, then the pack in its place */
	tell(step, context, "writing the game");
	for (int file = 0; file < BUILD_FILES; file++)
	{
		joined(inputs->build, build_files[file], from, sizeof(from));
		joined(ports, build_files[file], to, sizeof(to));
		if (!copy_file(from, to))
		{
			remove(pack);
			return failed(outcome, "The game's files cannot be written", to);
		}
	}
	joined(game, ASSETS_FILE, to, sizeof(to));
	remove(to);
	if (rename(pack, to) != 0)
	{
		remove(pack);
		return failed(outcome, "The assets pack cannot be written", to);
	}
	outcome->size = stat(to, &status) == 0 ? (size_t)status.st_size : 0;
	if (log)
		fprintf(log, "\nwritten   %s (the assets pack %.1f MB)\n", ports, (double)outcome->size / (1024.0 * 1024.0));
	return 1;
}
