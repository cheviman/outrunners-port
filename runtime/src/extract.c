/**
 *    Name:        extract.c
 *    Description: the game's first run: its assets out of the player's ROM set, in the files the game loads
 *                 (game/extract.h)
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The folder is the one extract_assets.py writes, each asset in the plainest form runtime/src/assets.c
 *    reads, and it loads into the same memory (ports/headless/assets_check.c compares the two):
 *    - data/ and sound/data/: the pieces of the programs' data extract_layout.c lists (positions and names,
 *      without the code, which the C replaces), a section of hex bytes each in pieces.txt, or a fill where
 *      its bytes are all one;
 *    - tilesets/: the tile chips' 32768 tiles, 2048 a tileset, their pens in grey PNGs of 4 bits a pixel;
 *    - sprites/: the sprite chips' words, a sheet a bank of 1M words, 32 words a row of 8 pens of 4 bits (the
 *      first pen in the top bits): the rows of empty chip (all ones) are left out, as the game reads all ones
 *      where no piece is;
 *    - sound/samples/: the MultiPCM's samples, a WAV of 8 bits for each stretch the chips' empty bytes (FF)
 *      do not break, and its 512 headers by their fields, each with the place it starts at.
 *    The PNGs hold their pixels in zlib's stored blocks, without compression. data/layout.json is written
 *    last: a folder without it is not whole.
 */
#include "game/extract.h"

#include "game/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define TILE_COUNT          0x8000u
#define TILESET_TILES       0x800u      /* tiles a tileset */
#define TILESET_ROW         32u         /* tiles a row of a tileset's PNG */
#define SPRITE_BANKS        4u
#define SPRITE_BANK_WORDS   0x100000u
#define SPRITE_ROW_WORDS    32u         /* words a row of a sprite sheet: 256 pens of 4 bits */
#define PCM_HEADERS         0x200u
#define PCM_HEADER          12u
#define PCM_GAP_MOST        0x400u      /* empty bytes between two stretches of samples that keep them one */
#define PCM_RATE            44100u      /* the WAVs' rate: nominal, as extract_assets.py's */
#define FILL_LEAST          16u         /* a piece this long whose bytes are all one is a fill */
#define HEX_LINE            32u         /* bytes a line of a section */
#define STORED_MOST         0xFFFFu     /* bytes a stored block of DEFLATE holds */
#define PATH_SIZE           1200

static char problem[PATH_SIZE + 64];

/* ---- files ---- */

static void make_folder(const char *path)
{
#ifdef _WIN32
	_mkdir(path);
#else
	mkdir(path, 0755);
#endif
}

/* 'path' made, and every folder it is in that is not there (what is there stays) */
static void make_folders(const char *path)
{
	char partial[PATH_SIZE];
	const size_t length = strlen(path);

	if (length >= sizeof(partial))
		return;
	for (size_t at = 1; at <= length; at++)
		if (at == length || path[at] == '/' || path[at] == '\\')
		{
			memcpy(partial, path, at);
			partial[at] = 0;
			make_folder(partial);
		}
}

/* 'relative' in 'folder' opened to be written, its path in 'path': NULL with the problem set */
static FILE *create(const char *folder, const char *relative, char *path)
{
	FILE *file;

	snprintf(path, PATH_SIZE, "%s/%s", folder, relative);
	if (!(file = fopen(path, "wb")))
		snprintf(problem, sizeof(problem), "%s cannot be written", path);
	return file;
}

/* The file closed: whether everything went into it (the problem set when not) */
static int finish(FILE *file, const char *path, int written)
{
	if (fclose(file) != 0 || !written)
	{
		snprintf(problem, sizeof(problem), "%s cannot be written", path);
		return 0;
	}
	return 1;
}

static void big32(uint8_t *out, uint32_t value)
{
	out[0] = (uint8_t)(value >> 24);
	out[1] = (uint8_t)(value >> 16);
	out[2] = (uint8_t)(value >> 8);
	out[3] = (uint8_t)value;
}

static void little32(uint8_t *out, uint32_t value)
{
	out[0] = (uint8_t)value;
	out[1] = (uint8_t)(value >> 8);
	out[2] = (uint8_t)(value >> 16);
	out[3] = (uint8_t)(value >> 24);
}

/* ---- PNG ---- */

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
	static uint32_t table[256];

	if (table[1] == 0u)
		for (uint32_t index = 0; index < 256u; index++)
		{
			table[index] = index;
			for (int bit = 0; bit < 8; bit++)
				table[index] = (table[index] >> 1) ^ (0xEDB88320u & (0u - (table[index] & 1u)));
		}
	crc = ~crc;
	for (size_t index = 0; index < size; index++)
		crc = (crc >> 8) ^ table[(crc ^ data[index]) & 0xFFu];
	return ~crc;
}

/* A chunk: its length, its type, its body and the CRC of the two */
static int put_chunk(FILE *file, const char *type, const uint8_t *body, size_t size)
{
	uint8_t head[8], tail[4];

	big32(head, (uint32_t)size);
	memcpy(head + 4, type, 4);
	big32(tail, crc32_update(crc32_update(0u, head + 4, 4), body, size));
	return fwrite(head, 1, 8, file) == 8 && (size == 0 || fwrite(body, 1, size, file) == size) && fwrite(tail, 1, 4, file) == 4;
}

/* A PNG of palette indexes, 'bits' a pixel (4: two a byte, the left one in the high nibble), with a grey for
   each index: 'height' rows of 'stride' bytes, as 'pixels' has them, in a zlib stream of stored blocks */
static int write_png(const char *folder, const char *relative, uint32_t width, uint32_t height, int bits,
	const uint8_t *pixels, size_t stride)
{
	static const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	const size_t raw_size = (stride + 1) * height;
	const size_t blocks = (raw_size + STORED_MOST - 1) / STORED_MOST;
	const int colours = 1 << bits;
	uint8_t header[13], palette[256 * 3];
	uint8_t *raw = malloc(raw_size);
	uint8_t *stream = malloc(2 + blocks * 5 + raw_size + 4);
	uint32_t adler_low = 1, adler_high = 0;
	size_t at = 0;
	char path[PATH_SIZE];
	FILE *file;
	int written;

	if (!raw || !stream)
	{
		free(raw);
		free(stream);
		snprintf(problem, sizeof(problem), "no memory for %s", relative);
		return 0;
	}
	/* the rows, each after its filter byte (0: none), and their Adler-32 */
	for (uint32_t row = 0; row < height; row++)
	{
		raw[row * (stride + 1)] = 0;
		memcpy(&raw[row * (stride + 1) + 1], &pixels[row * stride], stride);
	}
	for (size_t index = 0; index < raw_size; index++)
	{
		adler_low += raw[index];
		if (adler_low >= 65521u)
			adler_low -= 65521u;
		adler_high += adler_low;
		if (adler_high >= 65521u)
			adler_high -= 65521u;
	}
	/* the zlib stream: its header, the rows in stored blocks (the last block's bit, the kind 00, then the
	   length and its complement), the Adler-32 */
	stream[at++] = 0x78;
	stream[at++] = 0x01;
	for (size_t done = 0; done < raw_size;)
	{
		const size_t count = raw_size - done < STORED_MOST ? raw_size - done : STORED_MOST;

		stream[at++] = done + count == raw_size ? 1 : 0;
		stream[at++] = (uint8_t)count;
		stream[at++] = (uint8_t)(count >> 8);
		stream[at++] = (uint8_t)~count;
		stream[at++] = (uint8_t)(~count >> 8);
		memcpy(&stream[at], &raw[done], count);
		at += count;
		done += count;
	}
	free(raw);
	big32(stream + at, adler_high << 16 | adler_low);
	at += 4;
	big32(header, width);
	big32(header + 4, height);
	header[8] = (uint8_t)bits;
	header[9] = 3;    /* (palette indexes) */
	header[10] = header[11] = header[12] = 0;
	for (int colour = 0; colour < colours; colour++)
		memset(&palette[colour * 3], colour * 255 / (colours - 1), 3);
	if (!(file = create(folder, relative, path)))
	{
		free(stream);
		return 0;
	}
	written = fwrite(signature, 1, 8, file) == 8 && put_chunk(file, "IHDR", header, 13)
		&& put_chunk(file, "PLTE", palette, (size_t)colours * 3) && put_chunk(file, "IDAT", stream, at)
		&& put_chunk(file, "IEND", NULL, 0);
	free(stream);
	return finish(file, path, written);
}

/* ---- the programs' data ---- */

/* 'size' bytes in hex, HEX_LINE a line */
static int put_hex(FILE *file, const uint8_t *bytes, uint32_t size)
{
	static const char digits[] = "0123456789ABCDEF";
	char line[HEX_LINE * 3];

	for (uint32_t at = 0; at < size; at += HEX_LINE)
	{
		const uint32_t count = size - at < HEX_LINE ? size - at : HEX_LINE;
		size_t length = 0;

		for (uint32_t index = 0; index < count; index++)
		{
			line[length++] = digits[bytes[at + index] >> 4];
			line[length++] = digits[bytes[at + index] & 15];
			line[length++] = index + 1 < count ? ' ' : '\n';
		}
		if (fwrite(line, 1, length, file) != length)
			return 0;
	}
	return 1;
}

/* Whether the piece's bytes are a fill: all one, and enough of them */
static int is_fill(const uint8_t *bytes, uint32_t size)
{
	if (size < FILL_LEAST)
		return 0;
	for (uint32_t index = 1; index < size; index++)
		if (bytes[index] != bytes[0])
			return 0;
	return 1;
}

/* A region's pieces into 'subfolder' (data, sound/data): pieces.txt, their bytes in hex, a section each (or
   a fill in the layout, where the bytes are all one); names.txt, empty (no piece is typed, so none names an
   address); and layout.json, the list of the pieces */
static int write_region(const char *folder, const char *subfolder, const uint8_t *region, uint32_t region_size,
	const extract_piece *pieces, size_t count)
{
	char relative[256], path[PATH_SIZE];
	FILE *file;
	int written;

	for (size_t index = 0; index < count; index++)
		if (pieces[index].size == 0 || pieces[index].address >= region_size
			|| pieces[index].size > region_size - pieces[index].address
			|| (index > 0 && pieces[index].address < pieces[index - 1].address + pieces[index - 1].size))
		{
			snprintf(problem, sizeof(problem), "the piece %s of the game's data is not in the ROM set", pieces[index].name);
			return 0;
		}
	snprintf(relative, sizeof(relative), "%s/pieces.txt", subfolder);
	if (!(file = create(folder, relative, path)))
		return 0;
	written = fprintf(file, "# The pieces of %s, a section each: their bytes in hex (the game's first run took them out "
		"of the ROM set)\n", subfolder) > 0;
	for (size_t index = 0; index < count && written; index++)
	{
		const extract_piece *piece = &pieces[index];

		if (is_fill(region + piece->address, piece->size))
			continue;
		written = fprintf(file, "======== %s (%06X) ========\n", piece->name, piece->address) > 0
			&& put_hex(file, region + piece->address, piece->size);
	}
	if (!finish(file, path, written))
		return 0;
	snprintf(relative, sizeof(relative), "%s/names.txt", subfolder);
	if (!(file = create(folder, relative, path)))
		return 0;
	written = fputs("# NAME ADDRESS: none, as no piece in pieces.txt gives an address by a name\n", file) >= 0;
	if (!finish(file, path, written))
		return 0;
	snprintf(relative, sizeof(relative), "%s/layout.json", subfolder);
	if (!(file = create(folder, relative, path)))
		return 0;
	written = fputs("{\n \"base\": \"000000\",\n \"pieces\": [\n", file) >= 0;
	for (size_t index = 0; index < count && written; index++)
	{
		const extract_piece *piece = &pieces[index];
		const char *comma = index + 1 < count ? "," : "";

		if (is_fill(region + piece->address, piece->size))
			written = fprintf(file, "  {\"address\": \"%06X\", \"name\": \"%s\", \"size\": %u, \"format\": \"fill\", "
				"\"fill\": \"%02X\"}%s\n", piece->address, piece->name, piece->size, region[piece->address], comma) > 0;
		else
			written = fprintf(file, "  {\"address\": \"%06X\", \"name\": \"%s\", \"size\": %u, \"format\": \"bytes\", "
				"\"file\": \"pieces.txt\", \"section\": \"%s\"}%s\n", piece->address, piece->name, piece->size,
				piece->name, comma) > 0;
	}
	written = written && fputs(" ]\n}\n", file) >= 0;
	return finish(file, path, written);
}

/* ---- the tiles ---- */

/* The tiles 2048 a tileset: a PNG each, 32 tiles a row, and tilesets.txt, which says which tiles it holds.
   A tile is 16 rows of 8 bytes, two pens a byte with the high nibble first, the byte pairs of each half of a
   row swapped (as the video reads them, runtime/src/video.c): a PNG of 4 bits a pixel takes the bytes back
   in order */
static int write_tilesets(const char *folder)
{
	static const uint8_t byte_of_pair[8] = { 0, 2, 1, 3, 4, 6, 5, 7 };
	const uint32_t stride = TILESET_ROW * 8u, height = TILESET_TILES / TILESET_ROW * 16u;
	uint8_t *pixels = malloc((size_t)stride * height);
	char path[PATH_SIZE];
	FILE *list;
	int written;

	if (!pixels)
	{
		snprintf(problem, sizeof(problem), "no memory for the tilesets");
		return 0;
	}
	if (!(list = create(folder, "tilesets/tilesets.txt", path)))
	{
		free(pixels);
		return 0;
	}
	written = fputs("# The tile chips' tiles, 2048 a tileset: NAME FIRST-LAST (the game's first run took them out of "
		"the ROM set)\n", list) >= 0;
	for (uint32_t first = 0; first < TILE_COUNT && written; first += TILESET_TILES)
	{
		char name[64], relative[96];

		for (uint32_t row = 0; row < height; row++)
			for (uint32_t column = 0; column < TILESET_ROW; column++)
			{
				const uint8_t *tile = &game_tiles[(size_t)(first + row / 16u * TILESET_ROW + column) * 128u + row % 16u * 8u];

				for (int pair = 0; pair < 8; pair++)
					pixels[(size_t)row * stride + column * 8u + (uint32_t)pair] = tile[byte_of_pair[pair]];
			}
		snprintf(name, sizeof(name), "tiles_%04X", first);
		snprintf(relative, sizeof(relative), "tilesets/%s.png", name);
		if (!write_png(folder, relative, TILESET_ROW * 16u, height, 4, pixels, stride))
		{
			free(pixels);
			fclose(list);
			return 0;
		}
		written = fprintf(list, "%s %04X-%04X\n", name, first, first + TILESET_TILES - 1u) > 0;
	}
	free(pixels);
	return finish(list, path, written);
}

/* ---- the sprites ---- */

/* The sprite chips' words, a sheet a bank: rows of 32 words, a piece each stretch of rows that are not all
   ones, the pieces one under the other in the PNG; each word's 8 pens of 4 bits are its bytes as the chips'
   image has them (big endian, the first pen in the top bits). sheets.txt lists the sheets */
static int write_sprites(const char *folder)
{
	const uint32_t stride = SPRITE_ROW_WORDS * 4u, rows = SPRITE_BANK_WORDS / SPRITE_ROW_WORDS;
	uint8_t *pixels = malloc((size_t)stride * rows);
	char path[PATH_SIZE], json_path[PATH_SIZE];
	FILE *list;
	int written;

	if (!pixels)
	{
		snprintf(problem, sizeof(problem), "no memory for the sprites");
		return 0;
	}
	if (!(list = create(folder, "sprites/sheets.txt", path)))
	{
		free(pixels);
		return 0;
	}
	written = fputs("# The sprite sheets, a bank of the sprite chips each: NAME.png and NAME.json beside this file (the "
		"game's first run took them out of the ROM set)\n", list) >= 0;
	for (uint32_t bank = 0; bank < SPRITE_BANKS && written; bank++)
	{
		const uint8_t *chips = &game_sprites[(size_t)bank * SPRITE_BANK_WORDS * 4u];
		char name[32], relative[96];
		uint32_t height = 0, row = 0;
		FILE *json;

		snprintf(name, sizeof(name), "bank_%u", bank);
		snprintf(relative, sizeof(relative), "sprites/%s.json", name);
		if (!(json = create(folder, relative, json_path)))
		{
			free(pixels);
			fclose(list);
			return 0;
		}
		written = fputs("{\n \"bits\": 4,\n \"frames\": [", json) >= 0;
		while (row < rows && written)
		{
			uint32_t first;

			for (; row < rows; row++)
			{
				const uint8_t *bytes = &chips[(size_t)row * stride];
				uint32_t at = 0;

				while (at < stride && bytes[at] == 0xFF)
					at++;
				if (at < stride)
					break;
			}
			if (row == rows)
				break;
			/* (a piece: the rows up to the next that is all ones) */
			first = row;
			for (; row < rows; row++)
			{
				const uint8_t *bytes = &chips[(size_t)row * stride];
				uint32_t at = 0;

				while (at < stride && bytes[at] == 0xFF)
					at++;
				if (at == stride)
					break;
				memcpy(&pixels[(size_t)(height + row - first) * stride], bytes, stride);
			}
			written = fprintf(json, "%s\n  {\"x\": 0, \"y\": %u, \"width\": %u, \"height\": %u, \"word\": \"%06X\"}",
				height ? "," : "", height, SPRITE_ROW_WORDS * 8u, row - first,
				bank * SPRITE_BANK_WORDS + first * SPRITE_ROW_WORDS) > 0;
			height += row - first;
		}
		written = written && fputs("\n ]\n}\n", json) >= 0;
		if (!finish(json, json_path, written))
		{
			free(pixels);
			fclose(list);
			return 0;
		}
		if (height == 0)
		{
			remove(json_path);    /* (a bank all ones: no sheet) */
			continue;
		}
		snprintf(relative, sizeof(relative), "sprites/%s.png", name);
		if (!write_png(folder, relative, SPRITE_ROW_WORDS * 8u, height, 4, pixels, stride))
		{
			free(pixels);
			fclose(list);
			return 0;
		}
		written = fprintf(list, "%s\n", name) > 0;
	}
	free(pixels);
	return finish(list, path, written);
}

/* ---- the samples ---- */

/* A WAV of 8 bits, mono: the chip's signed bytes + 128 */
static int write_wav(const char *folder, const char *relative, const uint8_t *bytes, uint32_t length)
{
	uint8_t head[44];
	uint8_t *samples = malloc(length + 1u);
	char path[PATH_SIZE];
	FILE *file;
	int written;

	if (!samples)
	{
		snprintf(problem, sizeof(problem), "no memory for %s", relative);
		return 0;
	}
	for (uint32_t at = 0; at < length; at++)
		samples[at] = (uint8_t)(bytes[at] ^ 0x80u);
	samples[length] = 0;    /* (a chunk of an odd length takes a byte more) */
	memcpy(head, "RIFF", 4);
	little32(head + 4, 36u + length + (length & 1u));
	memcpy(head + 8, "WAVEfmt ", 8);
	little32(head + 16, 16u);
	little32(head + 20, 1u | 1u << 16);    /* (PCM, one channel) */
	little32(head + 24, PCM_RATE);
	little32(head + 28, PCM_RATE);
	little32(head + 32, 1u | 8u << 16);    /* (a byte a sample, 8 bits) */
	memcpy(head + 36, "data", 4);
	little32(head + 40, length);
	if (!(file = create(folder, relative, path)))
	{
		free(samples);
		return 0;
	}
	written = fwrite(head, 1, sizeof(head), file) == sizeof(head)
		&& fwrite(samples, 1, length + (length & 1u), file) == length + (length & 1u);
	free(samples);
	return finish(file, path, written);
}

/* The MultiPCM's 4 MB: its samples past the headers, a WAV each stretch that the chips' empty bytes (FF)
   do not break for PCM_GAP_MOST bytes or more, listed in samples.txt (OFFSET LENGTH NAME); and headers.txt,
   its 512 headers by their fields, each with the place it starts at (assets_media.c header_line) */
static int write_samples(const char *folder)
{
	const uint8_t *pcm = game_pcm;
	uint32_t place = PCM_HEADERS * PCM_HEADER;
	char path[PATH_SIZE];
	FILE *list;
	int written;

	if (!(list = create(folder, "sound/samples/samples.txt", path)))
		return 0;
	written = fputs("# The MultiPCM's samples: OFFSET LENGTH NAME, in hex (the game's first run took them out of the "
		"ROM set)\n", list) >= 0;
	while (written)
	{
		uint32_t start, end;
		char name[32], relative[96];

		while (place < GAME_PCM_SIZE && pcm[place] == 0xFF)
			place++;
		if (place == GAME_PCM_SIZE)
			break;
		start = place;
		end = place + 1u;
		for (place = end; place < GAME_PCM_SIZE && place - end < PCM_GAP_MOST; place++)
			if (pcm[place] != 0xFF)
				end = place + 1u;
		place = end;
		snprintf(name, sizeof(name), "pcm_%06X", start);
		snprintf(relative, sizeof(relative), "sound/samples/%s.wav", name);
		if (!write_wav(folder, relative, pcm + start, end - start))
		{
			fclose(list);
			return 0;
		}
		written = fprintf(list, "%06X %06X %s\n", start, end - start, name) > 0;
	}
	if (!finish(list, path, written))
		return 0;
	if (!(list = create(folder, "sound/samples/headers.txt", path)))
		return 0;
	written = fputs("# The MultiPCM's 512 sample headers by their fields (the game's first run took them out of the "
		"ROM set)\n", list) >= 0;
	for (uint32_t header = 0; header < PCM_HEADERS && written; header++)
	{
		const uint8_t *raw = &pcm[header * PCM_HEADER];
		const uint32_t start = (uint32_t)raw[0] << 16 | (uint32_t)raw[1] << 8 | raw[2];

		written = fprintf(list, "%03X fixed at=%06X loop=%04X length=%X format=%X lfo_vibrato=%02X attack=%X decay1=%X "
			"decay_level=%X decay2=%X key_rate_scale=%X release=%X lfo_amplitude=%02X\n", header, start & 0x1FFFFFu,
			(uint32_t)raw[3] << 8 | raw[4], 0x10000u - ((uint32_t)raw[5] << 8 | raw[6]), start >> 21, raw[7],
			raw[8] >> 4, raw[8] & 15u, raw[9] >> 4, raw[9] & 15u, raw[10] >> 4, raw[10] & 15u, raw[11]) > 0;
	}
	return finish(list, path, written);
}

/* ---- the first run ---- */

static uint32_t fnv_add(uint32_t hash, const void *bytes, size_t size)
{
	const uint8_t *byte = bytes;

	for (size_t index = 0; index < size; index++)
		hash = (hash ^ byte[index]) * 16777619u;
	return hash;
}

uint32_t game_assets_version(void)
{
	static const struct
	{
		const extract_piece *pieces;
		const size_t *count;
	} regions[] = { { extract_program_pieces, &extract_program_piece_count }, { extract_sound_pieces, &extract_sound_piece_count } };
	uint32_t hash = 2166136261u;

	for (size_t region = 0; region < sizeof(regions) / sizeof(regions[0]); region++)
		for (size_t index = 0; index < *regions[region].count; index++)
		{
			const extract_piece *piece = &regions[region].pieces[index];

			hash = fnv_add(hash, &piece->address, sizeof(piece->address));
			hash = fnv_add(hash, &piece->size, sizeof(piece->size));
			hash = fnv_add(hash, piece->name, strlen(piece->name) + 1);
		}
	return hash;
}

const char *game_assets_extract(const char *source, const char *folder)
{
	static const char *const folders[] = { "tilesets", "sprites", "sound/samples", "sound/data", "data" };
	const char *wrong = game_rom_set_load(source);
	char path[PATH_SIZE];

	if (wrong)
		return wrong;
	problem[0] = 0;
	for (size_t index = 0; index < sizeof(folders) / sizeof(folders[0]); index++)
	{
		snprintf(path, sizeof(path), "%s/%s", folder, folders[index]);
		make_folders(path);
	}
	/* (until the folder is whole again, the game does not take it for its assets) */
	snprintf(path, sizeof(path), "%s/data/layout.json", folder);
	remove(path);
	if (!write_tilesets(folder) || !write_sprites(folder) || !write_samples(folder)
		|| !write_region(folder, "sound/data", game_sound, GAME_SOUND_SIZE, extract_sound_pieces, extract_sound_piece_count)
		|| !write_region(folder, "data", game_program, GAME_PROGRAM_SIZE, extract_program_pieces, extract_program_piece_count))
		return problem;
	{
		/* (the folder whole: its version, for a later build of the game to tell) */
		FILE *version = create(folder, GAME_ASSETS_VERSION_FILE, path);

		if (!version || !finish(version, path, fprintf(version, "%08X\n", (unsigned)game_assets_version()) > 0))
			return problem;
	}
	return NULL;
}
