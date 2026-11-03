/**
 *    Name:        ios.h
 *    Description: what iOS does its own way (ios.m): the folder the Files app shows, and the ROM set chosen there
 *    Version:     0.1.0
 *    Created:     03-11-2026
 *    Modified:    03-11-2026
 *    Author:      yoshisuga
 *
 *    license:BSD-3-Clause
 */
#ifndef ORUNNERS_PORT_IOS_H
#define ORUNNERS_PORT_IOS_H

#include <SDL.h>

/* Until the system gives the app its scene, and SDL's window goes in it: before SDL starts */
void ios_wait_for_scene(void);

/* The app's Documents folder, with its separator: where the game keeps what it writes. The caller frees it
   (SDL_free) */
char *ios_documents_folder(void);

/* The system's file picker over 'window', for a .zip: the file chosen comes as an SDL_DROPFILE event, a copy of
   it in the app's own folders */
void ios_pick_rom_set(SDL_Window *window);

#endif
