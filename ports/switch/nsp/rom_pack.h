/**
 *    Name:        rom_pack.h
 *    Description: the makers' assets pack (outrunners.assets) out of the player's ROM set, as the computer's
 *                 first run makes it (OutRunners NSP's, and the other devices' makers')
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Paths are as the C library's fopen takes them (on Windows, in the system's code page).
 */
#ifndef ORUNNERS_NSP_ROM_PACK_H
#define ORUNNERS_NSP_ROM_PACK_H

/* The assets pack out of the ROM set at 'rom' (orunners.zip, or a folder of its files) into the file 'pack':
   NULL, or what failed, in words for the player. A tool takes one ROM set's assets: the same set again
   writes the same pack */
const char *rom_pack_make(const char *rom, const char *pack);

#endif
