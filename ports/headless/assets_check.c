/**
 *    Name:        assets_check.c
 *    Description: the acceptance check of the game's first run (runtime/src/extract.c): two folders of
 *                 assets compared as the game loads them, and the assets taken out of a ROM set as the first
 *                 run takes them
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Usage: orunners_assets_check FOLDER FOLDER            what the game reads, compared between two folders
 *           orunners_assets_check --extract ROM FOLDER      the assets out of the ROM set (its .zip or folder)
 *           orunners_assets_check --snapshot FOLDER FILE    what the game reads from FOLDER, into FILE
 *    The loader keeps one set of assets (runtime/src/assets.c), so each folder is loaded by a run of this
 *    program of its own (--snapshot, into the system's temporary folder) and the two snapshots are compared:
 *    every piece of the programs' data, its address, size and bytes, as data_at and assets_sound_data find
 *    them; where each of the C's tables lands (game_tables: its piece, its place in it and the bytes from
 *    there to the piece's end); the pens of every tile and the words of every sprite bank (s32_graphics); and
 *    the MultiPCM's 4 MB (assets_pcm_byte). It ends with 0 when nothing differs, 1 when something does.
 */
#include "game/assets.h"
#include "game/extract.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define SNAPSHOT_END    0xFFFFFFFFu    /* after the pieces assets_dump writes */
#define SOUND_PIECE     0x80000000u    /* assets_dump's mark of a piece of the sound program's data */
#define PCM_SIZE        0x400000u
#define SHOWN_MOST      12             /* differences listed of each kind; the rest are counted */

#define USAGE "Usage: orunners_assets_check FOLDER FOLDER | --extract ROM FOLDER | --snapshot FOLDER FILE\n"

typedef struct
{
	uint32_t address;
	uint32_t size;
	const uint8_t *bytes;
} piece;

typedef struct
{
	uint8_t *file;
	size_t size;
	piece *pieces;
	size_t piece_count;
	const uint8_t *rest;    /* what follows the pieces: the tables, the tiles, the sprites, the samples */
	size_t rest_size;
} snapshot;

/* ---- files ---- */

static uint8_t *read_whole(const char *path, size_t *size)
{
	FILE *input = fopen(path, "rb");
	uint8_t *data = NULL;
	long length;

	if (!input)
		return NULL;
	if (fseek(input, 0, SEEK_END) == 0 && (length = ftell(input)) > 0 && fseek(input, 0, SEEK_SET) == 0
		&& (data = malloc((size_t)length)) != NULL && fread(data, 1, (size_t)length, input) != (size_t)length)
	{
		free(data);
		data = NULL;
	}
	fclose(input);
	if (data)
		*size = (size_t)length;
	return data;
}

static uint32_t little32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static int put32(FILE *output, uint32_t value)
{
	const uint8_t bytes[4] = { (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24) };

	return fwrite(bytes, 1, 4, output) == 4;
}

static uint64_t fnv(const uint8_t *bytes, size_t size)
{
	uint64_t hash = 0xcbf29ce484222325ull;

	for (size_t index = 0; index < size; index++)
		hash = (hash ^ bytes[index]) * 0x100000001b3ull;
	return hash;
}

/* The pieces of a file assets_dump wrote, up to its end or SNAPSHOT_END: false when it is cut short */
static int read_pieces(snapshot *into)
{
	size_t at = 0, room = 0;

	while (at + 8 <= into->size && little32(into->file + at) != SNAPSHOT_END)
	{
		const uint32_t address = little32(into->file + at), size = little32(into->file + at + 4);

		if (size > into->size - at - 8)
			return 0;
		if (into->piece_count == room)
		{
			piece *grown;

			room = room ? room * 2 : 2048;
			if (!(grown = realloc(into->pieces, room * sizeof(*grown))))
				return 0;
			into->pieces = grown;
		}
		into->pieces[into->piece_count++] = (piece){ address, size, into->file + at + 8 };
		at += 8 + size;
	}
	if (at + 8 <= into->size)
		at += 8;    /* (past SNAPSHOT_END and its 0) */
	into->rest = into->file + at;
	into->rest_size = into->size - at;
	return 1;
}

/* ---- the snapshot of a folder ---- */

/* An address of the program's chips as the pieces are named (assets.c folded) */
static uint32_t folded(uint32_t address)
{
	address &= 0xffffff;
	if ((address & 0xf00000) == 0xf00000)
		address &= 0xfffff;
	if (address < 0x100000)
		address &= 0x3ffff;
	return address;
}

static int take_snapshot(const char *folder, const char *path)
{
	const char *problem = assets_load(folder);
	const s32_graphics *graphics;
	snapshot dumped = { 0 };
	uint8_t *buffer;
	FILE *output;
	int written;

	if (problem)
	{
		fprintf(stderr, "%s: %s\n", folder, problem);
		return 0;
	}
	if (!assets_dump(path) || !(dumped.file = read_whole(path, &dumped.size)) || !read_pieces(&dumped)
		|| !(output = fopen(path, "ab")))
	{
		fprintf(stderr, "%s cannot be written\n", path);
		return 0;
	}
	written = put32(output, SNAPSHOT_END) && put32(output, 0);
	/* the tables: where each lands, the piece and the place in it, and the bytes from there to its end */
	written = written && put32(output, (uint32_t)game_table_count);
	for (size_t index = 0; index < game_table_count && written; index++)
	{
		const game_table *table = &game_tables[index];
		const uint8_t *data = *table->data;
		const uint32_t place = folded(table->address);
		const piece *holder = NULL;
		uint64_t hash = 0;

		for (size_t at = 0; at < dumped.piece_count && !holder; at++)
			if (!(dumped.pieces[at].address & SOUND_PIECE) && place - dumped.pieces[at].address < dumped.pieces[at].size)
				holder = &dumped.pieces[at];
		if (holder)
			hash = fnv(data, holder->address + holder->size - place);
		written = put32(output, holder ? holder->address : SNAPSHOT_END) && put32(output, holder ? place - holder->address : 0)
			&& put32(output, data_at(table->address) == data) && put32(output, (uint32_t)hash)
			&& put32(output, (uint32_t)(hash >> 32));
	}
	free(dumped.pieces);
	free(dumped.file);
	/* the graphics: every tile's pens, every sprite bank's words */
	graphics = assets_graphics();
	written = written && put32(output, graphics->tile_count) && put32(output, graphics->sprite_banks);
	for (uint32_t tile = 0; tile < graphics->tile_count && written; tile++)
	{
		static const uint8_t none[256];

		written = fwrite(graphics->tile_pens[tile] ? graphics->tile_pens[tile] : none, 1, 256, output) == 256;
	}
	if (!(buffer = malloc(PCM_SIZE)))
		written = 0;
	for (uint32_t bank = 0; bank < graphics->sprite_banks && written; bank++)
	{
		for (uint32_t word = 0; word < 0x100000u; word++)
		{
			const uint32_t value = graphics->sprite_word(bank << 20 | word);

			buffer[word * 4u] = (uint8_t)value;
			buffer[word * 4u + 1u] = (uint8_t)(value >> 8);
			buffer[word * 4u + 2u] = (uint8_t)(value >> 16);
			buffer[word * 4u + 3u] = (uint8_t)(value >> 24);
		}
		written = fwrite(buffer, 1, 0x400000u, output) == 0x400000u;
	}
	/* the MultiPCM's address space */
	for (uint32_t place = 0; place < PCM_SIZE && written; place++)
		buffer[place] = assets_pcm_byte(place);
	written = written && fwrite(buffer, 1, PCM_SIZE, output) == PCM_SIZE;
	free(buffer);
	if (fclose(output) != 0 || !written)
	{
		fprintf(stderr, "%s cannot be written\n", path);
		return 0;
	}
	return 1;
}

/* ---- the comparison ---- */

static int load_snapshot(const char *path, snapshot *into)
{
	memset(into, 0, sizeof(*into));
	if (!(into->file = read_whole(path, &into->size)) || !read_pieces(into))
	{
		fprintf(stderr, "%s cannot be read\n", path);
		return 0;
	}
	return 1;
}

/* The pieces of one program (the sound program's when 'sound'): what one folder has and the other has not,
   or has with another size or other bytes */
static size_t compare_pieces(const snapshot *first, const snapshot *second, int sound, const char *what)
{
	size_t one = 0, other = 0, count = 0, bytes = 0, differences = 0;

	for (;;)
	{
		const piece *mine = NULL, *theirs = NULL;

		while (one < first->piece_count && !(first->pieces[one].address & SOUND_PIECE) != !sound)
			one++;
		while (other < second->piece_count && !(second->pieces[other].address & SOUND_PIECE) != !sound)
			other++;
		if (one == first->piece_count && other == second->piece_count)
			break;
		if (one < first->piece_count)
			mine = &first->pieces[one];
		if (other < second->piece_count)
			theirs = &second->pieces[other];
		if (mine && theirs && mine->address == theirs->address)
		{
			const uint32_t address = mine->address & ~SOUND_PIECE;

			count++;
			bytes += mine->size;
			if (mine->size != theirs->size)
			{
				if (differences++ < SHOWN_MOST)
					printf("  %s %06X: %u bytes, %u in the second folder\n", what, address, mine->size, theirs->size);
			}
			else if (memcmp(mine->bytes, theirs->bytes, mine->size) != 0)
			{
				uint32_t at = 0;

				while (mine->bytes[at] == theirs->bytes[at])
					at++;
				if (differences++ < SHOWN_MOST)
					printf("  %s %06X: differs from its byte %X (%02X, %02X in the second folder)\n", what, address, at,
						mine->bytes[at], theirs->bytes[at]);
			}
			one++;
			other++;
		}
		else if (mine && (!theirs || mine->address < theirs->address))
		{
			if (differences++ < SHOWN_MOST)
				printf("  %s %06X (%u bytes): only in the first folder\n", what, mine->address & ~SOUND_PIECE, mine->size);
			one++;
		}
		else
		{
			if (differences++ < SHOWN_MOST)
				printf("  %s %06X (%u bytes): only in the second folder\n", what, theirs->address & ~SOUND_PIECE, theirs->size);
			other++;
		}
	}
	printf("%s: %zu pieces in both, %zu bytes; %zu differences\n", what, count, bytes, differences);
	return differences;
}

/* 'count' items of 'size' bytes each at 'first' and 'second': how many differ, the first of them listed */
static size_t compare_items(const uint8_t *first, const uint8_t *second, size_t count, size_t size, const char *what,
	const char *item)
{
	size_t differences = 0;

	for (size_t index = 0; index < count; index++)
		if (memcmp(first + index * size, second + index * size, size) != 0 && differences++ < SHOWN_MOST)
			printf("  %s %s %zX differs\n", what, item, index);
	return differences;
}

static int compare(const char *first_path, const char *second_path)
{
	snapshot first, second;
	size_t differences = 0, at = 0, tables, table_bytes, tiles, banks, needed;

	if (!load_snapshot(first_path, &first) || !load_snapshot(second_path, &second))
		return 2;
	differences += compare_pieces(&first, &second, 0, "program data");
	differences += compare_pieces(&first, &second, 1, "sound program data");
	/* the tables: as many in both (the same program), each landing in the same piece, at the same place, on
	   the same bytes */
	if (first.rest_size < 4 || second.rest_size < 4 || little32(first.rest) != little32(second.rest))
	{
		printf("the snapshots are not of the same program\n");
		return 2;
	}
	tables = little32(first.rest);
	table_bytes = tables * 20u;
	at = 4 + table_bytes;
	if (first.rest_size < at + 8 || second.rest_size < at + 8)
	{
		printf("a snapshot is cut short\n");
		return 2;
	}
	{
		size_t table_differences = 0;

		for (size_t index = 0; index < tables; index++)
		{
			const uint8_t *mine = first.rest + 4 + index * 20u, *theirs = second.rest + 4 + index * 20u;

			if (little32(mine + 8) != 1 || little32(theirs + 8) != 1)
			{
				if (table_differences++ < SHOWN_MOST)
					printf("  table %s: data_at does not reach the piece the loader gave it\n", game_tables[index].name);
			}
			else if (memcmp(mine, theirs, 20) != 0 && table_differences++ < SHOWN_MOST)
				printf("  table %s: at %06X+%X, at %06X+%X in the second folder, or on other bytes\n", game_tables[index].name,
					little32(mine), little32(mine + 4), little32(theirs), little32(theirs + 4));
		}
		printf("tables: %zu, each in its piece; %zu differences\n", tables, table_differences);
		differences += table_differences;
	}
	tiles = little32(first.rest + at);
	banks = little32(first.rest + at + 4);
	if (tiles != little32(second.rest + at) || banks != little32(second.rest + at + 4))
	{
		printf("the graphics: %zu tiles and %zu sprite banks, %u and %u in the second folder\n", tiles, banks,
			little32(second.rest + at), little32(second.rest + at + 4));
		return 1;
	}
	at += 8;
	needed = at + tiles * 256u + banks * 0x400000u + PCM_SIZE;
	if (first.rest_size != needed || second.rest_size != needed)
	{
		printf("a snapshot is cut short\n");
		return 2;
	}
	{
		const size_t tile_differences = compare_items(first.rest + at, second.rest + at, tiles, 256, "tiles:", "tile");
		size_t word_differences, pcm_differences;

		printf("tiles: %zu of 256 pens; %zu differences\n", tiles, tile_differences);
		at += tiles * 256u;
		word_differences = compare_items(first.rest + at, second.rest + at, banks * 0x100000u, 4, "sprites:", "word");
		printf("sprites: %zu banks of 1M words; %zu differences\n", banks, word_differences);
		at += banks * 0x400000u;
		pcm_differences = compare_items(first.rest + at, second.rest + at, PCM_SIZE, 1, "MultiPCM:", "byte");
		printf("MultiPCM: %u bytes; %zu differences\n", PCM_SIZE, pcm_differences);
		differences += tile_differences + word_differences + pcm_differences;
	}
	printf("differences: %zu\n", differences);
	free(first.pieces);
	free(first.file);
	free(second.pieces);
	free(second.file);
	return differences ? 1 : 0;
}

/* ---- running itself ---- */

/* This program run with --snapshot FOLDER FILE: whether it wrote the snapshot */
static int snapshot_apart(const char *program, const char *folder, const char *path)
{
	char command[4096];

#ifdef _WIN32
	/* (cmd /c takes the outer quotes away) */
	snprintf(command, sizeof(command), "\"\"%s\" --snapshot \"%s\" \"%s\"\"", program, folder, path);
#else
	snprintf(command, sizeof(command), "'%s' --snapshot '%s' '%s'", program, folder, path);
#endif
	fflush(stdout);
	return system(command) == 0;
}

int main(int argc, char **argv)
{
	if (argc == 4 && strcmp(argv[1], "--extract") == 0)
	{
		const clock_t start = clock();
		const char *problem = game_assets_extract(argv[2], argv[3]);

		if (problem)
		{
			fprintf(stderr, "%s\n", problem);
			return 1;
		}
		printf("%s: the assets out of %s (%.1f s)\n", argv[3], argv[2], (double)(clock() - start) / CLOCKS_PER_SEC);
		return 0;
	}
	if (argc == 4 && strcmp(argv[1], "--snapshot") == 0)
		return take_snapshot(argv[2], argv[3]) ? 0 : 1;
	if (argc == 3 && argv[1][0] != '-')
	{
		char program[1024], first[1100], second[1100];
		const char *temporary;
		int result;

#ifdef _WIN32
		if (!GetModuleFileNameA(NULL, program, sizeof(program)))
			snprintf(program, sizeof(program), "%s", argv[0]);
		temporary = getenv("TEMP") ? getenv("TEMP") : (getenv("TMP") ? getenv("TMP") : ".");
#else
		snprintf(program, sizeof(program), "%s", argv[0]);
		temporary = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
#endif
		snprintf(first, sizeof(first), "%s/orunners_assets_check_%lu_1.snapshot", temporary, (unsigned long)time(NULL));
		snprintf(second, sizeof(second), "%s/orunners_assets_check_%lu_2.snapshot", temporary, (unsigned long)time(NULL));
		printf("first folder:  %s\nsecond folder: %s\n", argv[1], argv[2]);
		if (!snapshot_apart(program, argv[1], first) || !snapshot_apart(program, argv[2], second))
			result = 2;
		else
			result = compare(first, second);
		remove(first);
		remove(second);
		return result;
	}
	fprintf(stderr, USAGE);
	return 2;
}
