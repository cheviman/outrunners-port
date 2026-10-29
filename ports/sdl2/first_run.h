/**
 *    Name:        first_run.h
 *    Description: the first run: the game's assets out of the player's ROM set (first_run.c)
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 */
#ifndef ORUNNERS_SDL2_FIRST_RUN_H
#define ORUNNERS_SDL2_FIRST_RUN_H

/* The game's assets into 'folder' from the player's ROM set: orunners.zip or a folder rom/ beside the
   executable ('base', with its separator), beside the .app on macOS, or in the folder of what the game keeps
   ('saves'); else one dropped on a window that asks for it, with 'title' on it. 1 when the assets are written,
   0 when the window is closed first */
int first_run(const char *title, const char *base, const char *saves, const char *folder);

#endif
