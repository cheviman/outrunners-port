/**
 *    Name:        rom_pack.c
 *    Description: the makers' assets pack (outrunners.assets) out of the player's ROM set, as the computer's
 *                 first run makes it
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The first run's steps, the game's own: the assets out of the ROM set into a folder (runtime/src/extract.c),
 *    loaded as the game loads them (runtime/src/assets.c), and written as the pack the devices' builds read
 *    (assets_pack_write). The folder is beside the pack and goes once the pack is written, or once a step
 *    fails. The game loads its assets once, so a tool does too: the ROM set it took first is the one it
 *    writes packs of.
 */
#include "rom_pack.h"

#include "game/assets.h"
#include "game/extract.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define FOLDER_SUFFIX ".folder"
#define PATH_SIZE     1200

static char loaded_from[PATH_SIZE];    /* the ROM set whose assets the tool holds, or "" */
static int loaded_whole;               /* ... and they loaded */

/* The folder at 'path' and everything in it, gone (nothing when there is none) */
static void remove_folder(const char *path)
{
	char inside[PATH_SIZE];
#ifdef _WIN32
	WIN32_FIND_DATAA found;
	HANDLE search;

	snprintf(inside, sizeof(inside), "%s\\*", path);
	if ((search = FindFirstFileA(inside, &found)) != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0)
				continue;
			snprintf(inside, sizeof(inside), "%s\\%s", path, found.cFileName);
			if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				remove_folder(inside);
			else
				DeleteFileA(inside);
		} while (FindNextFileA(search, &found));
		FindClose(search);
	}
	RemoveDirectoryA(path);
#else
	DIR *folder = opendir(path);
	struct dirent *entry;

	if (folder)
	{
		while ((entry = readdir(folder)) != NULL)
		{
			struct stat status;

			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;
			snprintf(inside, sizeof(inside), "%s/%s", path, entry->d_name);
			if (lstat(inside, &status) == 0 && S_ISDIR(status.st_mode))
				remove_folder(inside);
			else
				unlink(inside);
		}
		closedir(folder);
	}
	rmdir(path);
#endif
}

const char *rom_pack_make(const char *rom, const char *pack)
{
	char folder[PATH_SIZE];
	const char *problem;

	if (loaded_from[0])
	{
		if (strcmp(loaded_from, rom) != 0)
			return "the tool holds another ROM set's assets";
		return loaded_whole ? assets_pack_write(pack) : "the ROM set's assets cannot be loaded";
	}
	snprintf(folder, sizeof(folder), "%s" FOLDER_SUFFIX, pack);
	remove_folder(folder);    /* (one a tool that was stopped left) */
	if ((problem = game_assets_extract(rom, folder)) == NULL)
	{
		snprintf(loaded_from, sizeof(loaded_from), "%s", rom);    /* (loaded or not: the game's loader runs once) */
		if ((problem = assets_load(folder)) == NULL)
		{
			loaded_whole = 1;
			problem = assets_pack_write(pack);
		}
	}
	remove_folder(folder);
	return problem;
}
