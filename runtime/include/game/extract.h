/**
 *    Name:        extract.h
 *    Description: the player's ROM set: for the tools that run the original code (the reference and the
 *                 translation scaffold, rom_set.c), and for the game's first run, which takes its assets out
 *                 of it (extract.c)
 *    Version:     0.3.0
 *    Created:     25-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The ROM set is MAME's 'orunners' (OutRunners, World), each of its 17 files known by its CRC32: in a
 *    .zip (stored or deflated) or a folder of its files (by MAME's names). The game itself never runs it: it
 *    loads its assets (game/assets.h), which extract_assets.py or the game's first run takes out of it once.
 */
#ifndef GAME_EXTRACT_H
#define GAME_EXTRACT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The ROM set at 'source' into game/data.h's regions, laid out as the board has them, the code too (for
   the reference and the scaffold, which run it): NULL, or what is wrong with it */
const char *game_rom_set_load(const char *source);

/* A piece of the programs' data the first run takes out of the ROM set (extract_layout.c): where it starts
   in its region (the V70's program space, or the sound program's chip), how many bytes it holds, its name */
typedef struct
{
	uint32_t address;
	uint32_t size;
	const char *name;
} extract_piece;

extern const extract_piece extract_program_pieces[];
extern const size_t extract_program_piece_count;
extern const extract_piece extract_sound_pieces[];
extern const size_t extract_sound_piece_count;

/* The game's assets out of the ROM set at 'source' (a .zip or a folder, as game_rom_set_load reads it) into
   the folder 'folder', made when it is missing: the files assets_load reads, which load as the ones
   extract_assets.py writes do. NULL, or what failed, in words for the player */
const char *game_assets_extract(const char *source, const char *folder);

#ifdef __cplusplus
}
#endif

#endif
