/**
 *    Name:        gpu_video.c
 *    Description: the board's video mixed on the GPU (docs/gpu_video.md): each screen's mixer as a shader
 *                 drawing straight into the window's texture, in SDL's own OpenGL context
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The CPU draws the layers and the sprites as before (runtime/src/video.c); what a screen's mixer reads goes
 *    up as integer textures (the layers' palette offsets, the rows each leaves empty, the sprites' marks, the
 *    palettes, the mixing's order by sprite group), and a fragment shader does what mix_rows does pixel by
 *    pixel, into the region of the renderer's texture the screen goes to. The shader is GLSL 3.30 on a
 *    computer and GLSL ES 3.00 on OpenGL ES 3 (Android, the Switch), the same text with its version line.
 *    SDL keeps some of OpenGL's state between its own draws (its program, its texture, blending, the frame
 *    buffer): what is changed here is put back.
 *    The shaders multiply no integers: shifts and adds where video.c multiplies, floats (exact, on small whole
 *    numbers) where it blends. Mesa makes integer multiplies on the Switch's GPU into IMAD, which Eden, the
 *    emulator, does not take: it drops the whole draw.
 */
#include "gpu_video.h"

#include "s32/machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- OpenGL, as SDL's context has it: the few functions and values used here ---- */

#ifdef _WIN32
#define GL_CALL __stdcall
#else
#define GL_CALL
#endif

typedef unsigned int gl_uint;
typedef int gl_int;
typedef unsigned int gl_enum;
typedef int gl_sizei;
typedef unsigned char gl_boolean;
typedef char gl_char;

#define GL_TEXTURE_2D               0x0DE1
#define GL_NEAREST                  0x2600
#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_TEXTURE_WRAP_S           0x2802
#define GL_TEXTURE_WRAP_T           0x2803
#define GL_CLAMP_TO_EDGE            0x812F
#define GL_UNSIGNED_BYTE            0x1401
#define GL_UNSIGNED_SHORT           0x1403
#define GL_RGBA                     0x1908
#define GL_R8UI                     0x8232
#define GL_R16UI                    0x8234
#define GL_RGBA16UI                 0x8D76
#define GL_RGBA32UI                 0x8D70
#define GL_UNSIGNED_INT             0x1405
#define GL_MAX_TEXTURE_SIZE         0x0D33
#define GL_RED_INTEGER              0x8D94
#define GL_RGBA_INTEGER             0x8D99
#define GL_UNPACK_ALIGNMENT         0x0CF5
#define GL_UNPACK_ROW_LENGTH        0x0CF2
#define GL_PACK_ALIGNMENT           0x0D05
#define GL_PACK_ROW_LENGTH          0x0D02
#define GL_FRAMEBUFFER              0x8D40
#define GL_FRAMEBUFFER_BINDING      0x8CA6
#define GL_COLOR_ATTACHMENT0        0x8CE0
#define GL_FRAMEBUFFER_COMPLETE     0x8CD5
#define GL_CURRENT_PROGRAM          0x8B8D
#define GL_ACTIVE_TEXTURE           0x84E0
#define GL_TEXTURE0                 0x84C0
#define GL_TEXTURE_BINDING_2D       0x8069
#define GL_VIEWPORT                 0x0BA2
#define GL_BLEND                    0x0BE2
#define GL_SCISSOR_TEST             0x0C11
#define GL_VERTEX_ARRAY_BINDING     0x85B5
#define GL_TRIANGLES                0x0004
#define GL_VERTEX_SHADER            0x8B31
#define GL_FRAGMENT_SHADER          0x8B30
#define GL_COMPILE_STATUS           0x8B81
#define GL_LINK_STATUS              0x8B82
#define GL_VERSION                  0x1F02

#define GL_FUNCTIONS(F) \
	F(void, GetIntegerv, (gl_enum, gl_int *)) \
	F(gl_enum, GetError, (void)) \
	F(gl_boolean, IsEnabled, (gl_enum)) \
	F(void, Enable, (gl_enum)) \
	F(void, Disable, (gl_enum)) \
	F(const unsigned char *, GetString, (gl_enum)) \
	F(void, GenTextures, (gl_sizei, gl_uint *)) \
	F(void, DeleteTextures, (gl_sizei, const gl_uint *)) \
	F(void, BindTexture, (gl_enum, gl_uint)) \
	F(void, ActiveTexture, (gl_enum)) \
	F(void, TexParameteri, (gl_enum, gl_enum, gl_int)) \
	F(void, PixelStorei, (gl_enum, gl_int)) \
	F(void, TexImage2D, (gl_enum, gl_int, gl_int, gl_sizei, gl_sizei, gl_int, gl_enum, gl_enum, const void *)) \
	F(void, TexSubImage2D, (gl_enum, gl_int, gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, const void *)) \
	F(gl_uint, CreateShader, (gl_enum)) \
	F(void, ShaderSource, (gl_uint, gl_sizei, const gl_char *const *, const gl_int *)) \
	F(void, CompileShader, (gl_uint)) \
	F(void, GetShaderiv, (gl_uint, gl_enum, gl_int *)) \
	F(void, GetShaderInfoLog, (gl_uint, gl_sizei, gl_sizei *, gl_char *)) \
	F(void, DeleteShader, (gl_uint)) \
	F(gl_uint, CreateProgram, (void)) \
	F(void, AttachShader, (gl_uint, gl_uint)) \
	F(void, LinkProgram, (gl_uint)) \
	F(void, GetProgramiv, (gl_uint, gl_enum, gl_int *)) \
	F(void, GetProgramInfoLog, (gl_uint, gl_sizei, gl_sizei *, gl_char *)) \
	F(void, DeleteProgram, (gl_uint)) \
	F(void, UseProgram, (gl_uint)) \
	F(gl_int, GetUniformLocation, (gl_uint, const gl_char *)) \
	F(void, Uniform1i, (gl_int, gl_int)) \
	F(void, Uniform2i, (gl_int, gl_int, gl_int)) \
	F(void, Uniform4i, (gl_int, gl_int, gl_int, gl_int, gl_int)) \
	F(void, Uniform3iv, (gl_int, gl_sizei, const gl_int *)) \
	F(void, GenFramebuffers, (gl_sizei, gl_uint *)) \
	F(void, DeleteFramebuffers, (gl_sizei, const gl_uint *)) \
	F(void, BindFramebuffer, (gl_enum, gl_uint)) \
	F(void, FramebufferTexture2D, (gl_enum, gl_enum, gl_enum, gl_uint, gl_int)) \
	F(gl_enum, CheckFramebufferStatus, (gl_enum)) \
	F(void, Viewport, (gl_int, gl_int, gl_sizei, gl_sizei)) \
	F(void, GenVertexArrays, (gl_sizei, gl_uint *)) \
	F(void, DeleteVertexArrays, (gl_sizei, const gl_uint *)) \
	F(void, BindVertexArray, (gl_uint)) \
	F(void, DrawArrays, (gl_enum, gl_int, gl_sizei)) \
	F(void, ReadPixels, (gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, void *))

#define GL_POINTER(type, name, arguments) type (GL_CALL *name) arguments;
static struct
{
	GL_FUNCTIONS(GL_POINTER)
} gl;

/* ---- the mixer's shader ---- */

#define WIDTH       S32_VIDEO_MAX_WIDTH
#define HEIGHT      S32_VIDEO_HEIGHT
#define LAYER_ROWS  256    /* the layers' texture: a layer every 256 rows (layer << 8 | y) */
#define PALETTE     128    /* the palettes' textures: 128 x 128 entries */

static const char vertex_text[] =
	"void main()\n"
	"{\n"
	"	vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
	"	gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);\n"    /* (one triangle over the viewport) */
	"}\n";

/* mix_rows (runtime/src/video.c), a pixel at a time */
static const char fragment_text[] =
	"precision highp float;\n"
	"precision highp int;\n"
	"precision highp usampler2D;\n"
	"uniform usampler2D layers;\n"         /* the layers' palette offsets, LAYER_ROWS rows apart */
	"uniform usampler2D sprites;\n"        /* the sprites' (the CPU's: Eden loses what the GPU drew into a texture
	                                          that the CPU writes too) */
	"uniform usampler2D empties;\n"        /* a layer's empty rows: (row, layer) */
	"uniform usampler2D marks;\n"          /* the sprites' marks of the extra palette and the ghosts */
	"uniform usampler2D palette;\n"
	"uniform usampler2D extra_palette;\n"
	"uniform usampler2D extra_set;\n"
	"uniform usampler2D orders;\n"         /* by group: two texels a place (base, sprite blend, blend, layer; shift, offsets) */
	"uniform int blend_factor, group_shift, group_mask, shadow_mask, sprite_bits, shadow_pen, flips, has_marks;\n"
	"uniform ivec3 offsets[3];\n"
	"uniform ivec4 visible;\n"              /* left, right, top, bottom */
	"uniform ivec2 origin;\n"               /* the region's first pixel in the target, less the first column */
	"uniform int use_empties;\n"            /* (the CPU's layers: their rows drawn empty keep stale pixels) */
	"uniform int swap_red_blue;\n"          /* (SDL's OpenGL ES textures of ARGB keep blue where red goes) */
	"out vec4 fragment_colour;\n"
	"int layer_pixel(int layer, ivec2 at)\n"
	"{\n"
	"	if (use_empties != 0 && layer < 6 && texelFetch(empties, ivec2(at.y, layer), 0).r != 0u)\n"
	"		return 0;\n"
	"	return int(texelFetch(layers, ivec2(at.x, layer << 8 | at.y), 0).r);\n"
	"}\n"
	"uvec4 entry(int group, int place, int part)\n"
	"{\n"
	"	return texelFetch(orders, ivec2(place << 1 | part, group), 0);\n"
	"}\n"
	/* the first layer down the list from 'place' that is not transparent there */
	"int top_layer(int group, int place, ivec2 at, int sprite_pixel, inout int shadow, out int pixel)\n"
	"{\n"
	"	for (; place < 8; place++)\n"
	"	{\n"
	"		int layer = int(entry(group, place, 0).a);\n"
	"		if (layer != 6)\n"
	"		{\n"
	"			pixel = layer_pixel(layer, at) & 0x1fff;\n"
	"			if (pixel != 0 || layer == 7)\n"
	"				return place;\n"
	"			continue;\n"
	"		}\n"
	"		pixel = sprite_pixel;\n"
	"		shadow = ~sprite_pixel & shadow_mask;\n"
	"		if ((sprite_pixel & 0x7fff) != 0x7fff)\n"
	"		{\n"
	"			pixel = sprite_pixel & sprite_bits;\n"
	"			if ((pixel & 0x7ffe) != shadow_pen)\n"
	"				return place;\n"
	"			shadow = 1;\n"
	"		}\n"
	"	}\n"
	"	pixel = 0;\n"
	"	return 7;\n"
	"}\n"
	"int palette_index(int group, int place, int pixel)\n"
	"{\n"
	"	uvec4 first = entry(group, place, 0), second = entry(group, place, 1);\n"
	"	return (int(first.r) + ((pixel >> int(second.r)) & 0xfff0) + (pixel & 0x0f)) & 0x3fff;\n"
	"}\n"
	"int palette_colour(int index)\n"
	"{\n"
	"	return int(texelFetch(palette, ivec2(index & 127, index >> 7), 0).r);\n"
	"}\n"
	"ivec3 channels(int colour, int set)\n"
	"{\n"
	"	return ivec3(colour & 0x1f, (colour >> 5) & 0x1f, (colour >> 10) & 0x1f) + offsets[set];\n"
	"}\n"
	"void main()\n"
	"{\n"
	"	ivec2 at = ivec2(gl_FragCoord.xy) - origin;\n"
	"	ivec2 sprite_at = ivec2((flips & 1) != 0 ? visible.y - (at.x - visible.x) : at.x,\n"
	"		(flips & 2) != 0 ? visible.w - (at.y - visible.z) : at.y);\n"
	"	int sprite_pixel = int(texelFetch(sprites, sprite_at, 0).r);\n"
	"	int group = (sprite_pixel >> group_shift) & group_mask;\n"
	"	int shadow = 0, pixel = 0;\n"
	"	int first = top_layer(group, 0, at, sprite_pixel, shadow, pixel);\n"
	"	int index = palette_index(group, first, pixel);\n"
	"	int colour = palette_colour(index);\n"
	"	int first_layer = int(entry(group, first, 0).a);\n"
	"	int mark_bits = has_marks != 0 && first_layer == 6 ? int(texelFetch(marks, sprite_at, 0).r) : 0;\n"
	"	if ((mark_bits & 1) != 0 && texelFetch(extra_set, ivec2(index & 127, index >> 7), 0).r != 0u)\n"
	"		colour = int(texelFetch(extra_palette, ivec2(index & 127, index >> 7), 0).r);\n"
	"	ivec3 rgb = channels(colour, int(entry(group, first, 1).g));\n"
	"	int blend = int(entry(group, first, 0).b);\n"
	"	if (blend != 0)\n"
	"	{\n"
	"		int below = 0;\n"
	"		int second = top_layer(group, first + 1, at, sprite_pixel, shadow, below);\n"
	"		int second_layer = int(entry(group, second, 0).a);\n"
	"		int sprite_blend = int(entry(group, first, 0).g);\n"
	"		if (((blend >> second_layer) & 1) != 0 && (second_layer != 6 || ((sprite_blend >> group) & 1) != 0))\n"
	"		{\n"
	"			ivec3 under = channels(palette_colour(palette_index(group, second, below)), int(entry(group, second, 1).g));\n"
	"			rgb = ivec3(vec3(rgb) * float(7 - blend_factor) + vec3(under) * float(blend_factor + 1)) >> 3;\n"
	"		}\n"
	"	}\n"
	"	if ((mark_bits & 2) != 0)\n"
	"	{\n"
	"		int below = 0, ignored = 0;\n"
	"		int second = top_layer(group, first + 1, at, sprite_pixel, ignored, below);\n"
	"		ivec3 under = channels(palette_colour(palette_index(group, second, below)), int(entry(group, second, 1).g));\n"
	"		ivec3 sum = rgb + under;\n"
	"		rgb = (sum + ivec3(lessThan(sum, ivec3(0)))) >> 1;\n"    /* (sum / 2, toward 0: Mesa divides with IMAD) */
	"	}\n"
	"	if (shadow != 0)\n"
	"		rgb = rgb >> 1;\n"
	"	rgb = clamp(rgb, 0, 31);\n"
	"	if (swap_red_blue != 0)\n"
	"		rgb = rgb.zyx;\n"
	"	fragment_colour = vec4(vec3(rgb << 3) / 255.0, 1.0);\n"
	"}\n";

/* A layer's pixels (video.c's page_pixel, text_pixel, bitmap_pixel and the backdrop), through its clip runs
   (draw_row): its LAYER_ROWS of the layers' texture each (drawing into a layer of an array texture, Eden draws
   into the first) */
#define TILES_ACROSS 64    /* the tiles' pens' texture: 64 tiles of 16 x 16 a row */

static const char layer_text[] =
	"precision highp float;\n"
	"precision highp int;\n"
	"precision highp usampler2D;\n"
	"uniform usampler2D vram;\n"           /* the video RAM's words, 256 a row */
	"uniform usampler2D pens;\n"           /* the tiles' pens */
	"uniform usampler2D rows;\n"           /* a layer's rows (row, layer): RGBA32UI */
	"uniform usampler2D runs;\n"           /* a layer's clip runs (layer * 16 + run, row) */
	"uniform int kind, layer, first_drawn, shift, bank, control, scroll, flip, bits8, left, right, side, tile_mask;\n"
	"out uint pixel_out;\n"
	"uint word_at(uint index)\n"
	"{\n"
	"	index &= 0xffffu;\n"
	"	return texelFetch(vram, ivec2(int(index & 255u), int(index >> 8)), 0).r;\n"
	"}\n"
	"uint byte_at(uint at)\n"
	"{\n"
	"	uint word = word_at(at >> 1);\n"
	"	return (at & 1u) != 0u ? word >> 8 : word & 255u;\n"
	"}\n"
	"uint page_pixel(uint page, int y, int x)\n"
	"{\n"
	"	uint cell = word_at(page << 9 | uint(y >> 4) << 5 | uint(x >> 4));\n"
	"	uint tile = (uint(bank) << 13 | (cell & 0x1fffu)) & uint(tile_mask);\n"
	"	int column = (cell & 0x4000u) != 0u ? 15 - (x & 15) : (x & 15);\n"
	"	int line = (cell & 0x8000u) != 0u ? 15 - (y & 15) : (y & 15);\n"
	"	uint pen = texelFetch(pens, ivec2(int(tile & 63u) << 4 | column, int(tile >> 6) << 4 | line), 0).r;\n"
	"	return pen != 0u ? (((cell >> 4) & 0x1ffu) << 4 | pen) : 0u;\n"
	"}\n"
	"uint text_pixel(int y, int x)\n"
	"{\n"
	"	uint bits = uint(control);\n"
	"	uint cell = word_at(((bits >> 4) & 0x1fu) << 11 | uint(y >> 3) << 6 | uint(x >> 3));\n"
	"	uint character = (bits & 7u) << 9 | (cell & 0x1ffu);\n"
	"	uint value = byte_at(character << 5 | uint((y & 7) << 2 | (x & 7) >> 1));\n"
	"	uint pen = (x & 1) != 0 ? (value & 0x0fu) : (value >> 4);\n"
	"	return pen != 0u ? (((cell >> 9) & 0x7fu) << 4 | pen) : 0u;\n"
	"}\n"
	/* the last run over the column says whether it is drawn: the runs alternate from first_drawn */
	"bool drawn_at(int x, int y)\n"
	"{\n"
	"	bool drawn = first_drawn != 0, result = false;\n"
	"	for (int run = 0; run < 15; run++, drawn = !drawn)\n"
	"	{\n"
	"		int start = int(texelFetch(runs, ivec2(layer << 4 | run, y), 0).r);\n"
	"		int end = int(texelFetch(runs, ivec2(layer << 4 | (run + 1), y), 0).r);\n"
	"		if (x >= start && x < end)\n"
	"			result = drawn;\n"
	"		if (end > right)\n"
	"			break;\n"
	"	}\n"
	"	return result;\n"
	"}\n"
	/* step * count (|count| < 1024), a bit of count at a time */
	"uint times(uint step, int count)\n"
	"{\n"
	"	uint magnitude = uint(abs(count)), product = 0u;\n"
	"	for (int bit = 0; bit < 10; bit++)\n"
	"		if (((magnitude >> uint(bit)) & 1u) != 0u)\n"
	"			product += step << uint(bit);\n"
	"	return count < 0 ? 0u - product : product;\n"
	"}\n"
	"void main()\n"
	"{\n"
	"	int x = int(gl_FragCoord.x), y = int(gl_FragCoord.y) - (layer << 8);\n"
	"	uvec4 row = texelFetch(rows, ivec2(y, layer), 0);\n"
	"	uint pixel = 0u;\n"
	"	if (kind == 0)\n"                   /* S32_GPU_TILEMAP */
	"	{\n"
	"		uint column = row.x + times(row.y, x - left - side);\n"
	"		uint page = ((column >> uint(shift + 9)) & 1u) != 0u ? (row.z >> 8) & 0xffu : row.z & 0xffu;\n"
	"		if (drawn_at(x, y))\n"
	"			pixel = page_pixel(page, int(row.w), int((column >> uint(shift)) & 0x1ffu));\n"
	"	}\n"
	"	else if (kind == 1)\n"              /* S32_GPU_TEXT: no windows; WIDE's sides clear */
	"	{\n"
	"		int column = x - side;\n"
	"		if (side == 0 || (column >= 0 && column < 320))\n"
	"			pixel = flip != 0 ? text_pixel(255 - y, 511 - column) : text_pixel(y, column);\n"
	"	}\n"
	"	else if (kind == 2)\n"              /* S32_GPU_BITMAP */
	"	{\n"
	"		int column = (x - side + scroll) & 0x1ff;\n"
	"		if (drawn_at(x, y))\n"
	"		{\n"
	"			if (bits8 != 0)\n"
	"			{\n"
	"				int value = int(byte_at(row.x + uint(column))) + control;\n"
	"				pixel = (value & 0xff) != 0 ? uint(value) & 0xffffu : 0u;\n"
	"			}\n"
	"			else\n"
	"			{\n"
	"				int value = int((word_at(row.x + (uint(column) >> 2)) >> (uint(column & 3) << 2)) & 0x0fu) + control;\n"
	"				pixel = (value & 0x0f) != 0 ? uint(value) & 0xffffu : 0u;\n"
	"			}\n"
	"		}\n"
	"	}\n"
	"	else\n"                             /* S32_GPU_BACKDROP */
	"		pixel = row.x & 0xffffu;\n"
	"	pixel_out = pixel;\n"
	"}\n";

enum
{
	L_VRAM, L_PENS, L_ROWS, L_RUNS, L_KIND, L_LAYER, L_FIRST_DRAWN, L_SHIFT, L_BANK, L_CONTROL, L_SCROLL, L_FLIP,
	L_BITS8, L_LEFT, L_RIGHT, L_SIDE, L_TILE_MASK, L_UNIFORMS
};

static const char *const layer_uniform_names[L_UNIFORMS] = { "vram", "pens", "rows", "runs", "kind", "layer",
	"first_drawn", "shift", "bank", "control", "scroll", "flip", "bits8", "left", "right", "side", "tile_mask" };

static struct
{
	bool open;
	bool es;                    /* OpenGL ES (its version line) */
	gl_uint layer_program;
	gl_int layer_places[L_UNIFORMS];
	gl_uint vram, pens, rows, runs;
	uint32_t tile_count;        /* the pens' texture's (0: not made yet) */
	bool layers_drawn;          /* this frame's layers drawn on the GPU (gpu_video_draw_layers) */
	bool said;                  /* the log told how the first mixing went */
	SDL_Renderer *renderer;
	gl_uint program;
	gl_uint array;              /* (a vertex array, which a core context wants bound to draw) */
	gl_uint layers, sprites, empties, marks, palette, extra_palette, extra_set, orders;
	gl_uint framebuffer;
	gl_int places[20];          /* the uniforms' */
} gpu;

enum
{
	U_LAYERS, U_EMPTIES, U_MARKS, U_PALETTE, U_EXTRA_PALETTE, U_EXTRA_SET, U_ORDERS, U_BLEND_FACTOR, U_GROUP_SHIFT,
	U_GROUP_MASK, U_SHADOW_MASK, U_SPRITE_BITS, U_SHADOW_PEN, U_FLIPS, U_HAS_MARKS, U_OFFSETS, U_USE_EMPTIES,
	U_SWAP_RED_BLUE, U_SPRITES
};

static const char *const uniform_names[] = { "layers", "empties", "marks", "palette", "extra_palette", "extra_set",
	"orders", "blend_factor", "group_shift", "group_mask", "shadow_mask", "sprite_bits", "shadow_pen", "flips",
	"has_marks", "offsets", "use_empties", "swap_red_blue", "sprites" };

static gl_uint compile(gl_enum kind, const char *text)
{
	const char *const parts[2] = { gpu.es ? "#version 300 es\n" : "#version 330 core\n", text };
	const gl_uint shader = gl.CreateShader(kind);
	gl_int done = 0;

	gl.ShaderSource(shader, 2, parts, NULL);
	gl.CompileShader(shader);
	gl.GetShaderiv(shader, GL_COMPILE_STATUS, &done);
	if (!done)
	{
		char log[2048];

		gl.GetShaderInfoLog(shader, sizeof(log), NULL, log);
		fprintf(stderr, "A shader of the GPU's video was not compiled: %s\n", log);
		gl.DeleteShader(shader);
		return 0;
	}
	return shader;
}

static gl_uint integer_texture(gl_int format, gl_sizei width, gl_sizei height, gl_enum pixel_format, gl_enum type)
{
	gl_uint texture;

	gl.GenTextures(1, &texture);
	gl.BindTexture(GL_TEXTURE_2D, texture);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	gl.TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, pixel_format, type, NULL);
	return texture;
}

/* ---- SDL's state, kept and put back ---- */

typedef struct
{
	gl_int program, active, texture, framebuffer, array, viewport[4];
	gl_boolean blend, scissor;
} gl_state;

static void keep_state(gl_state *state)
{
	gl.GetIntegerv(GL_CURRENT_PROGRAM, &state->program);
	gl.GetIntegerv(GL_ACTIVE_TEXTURE, &state->active);
	gl.ActiveTexture(GL_TEXTURE0);
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture);
	gl.GetIntegerv(GL_FRAMEBUFFER_BINDING, &state->framebuffer);
	gl.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &state->array);
	gl.GetIntegerv(GL_VIEWPORT, state->viewport);
	state->blend = gl.IsEnabled(GL_BLEND);
	state->scissor = gl.IsEnabled(GL_SCISSOR_TEST);
}

static void put_state_back(const gl_state *state)
{
	for (int unit = 7; unit >= 1; unit--)
	{
		gl.ActiveTexture(GL_TEXTURE0 + (gl_enum)unit);
		gl.BindTexture(GL_TEXTURE_2D, 0);
	}
	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, (gl_uint)state->texture);
	gl.ActiveTexture((gl_enum)state->active);
	gl.UseProgram((gl_uint)state->program);
	gl.BindFramebuffer(GL_FRAMEBUFFER, (gl_uint)state->framebuffer);
	gl.BindVertexArray((gl_uint)state->array);
	gl.Viewport(state->viewport[0], state->viewport[1], state->viewport[2], state->viewport[3]);
	if (state->blend)
		gl.Enable(GL_BLEND);
	if (state->scissor)
		gl.Enable(GL_SCISSOR_TEST);
}

/* The renderer's OpenGL texture (SDL_GL_BindTexture binds it on unit 0; its name read back) */
static gl_uint texture_name(SDL_Texture *texture)
{
	gl_int name = 0;

	if (SDL_GL_BindTexture(texture, NULL, NULL) != 0)
		return 0;
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &name);
	SDL_GL_UnbindTexture(texture);
	return (gl_uint)name;
}

/* ---- the frame ---- */

/* A program of the one-triangle vertex shader and 'fragment_source': 0 where it is refused */
static gl_uint program_of(const char *fragment_source)
{
	const gl_uint vertex = compile(GL_VERTEX_SHADER, vertex_text);
	const gl_uint fragment = vertex ? compile(GL_FRAGMENT_SHADER, fragment_source) : 0;
	gl_uint program;
	gl_int linked = 0;

	if (!fragment)
	{
		if (vertex)
			gl.DeleteShader(vertex);
		return 0;
	}
	program = gl.CreateProgram();
	gl.AttachShader(program, vertex);
	gl.AttachShader(program, fragment);
	gl.LinkProgram(program);
	gl.DeleteShader(vertex);
	gl.DeleteShader(fragment);
	gl.GetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		char log[2048];

		gl.GetProgramInfoLog(program, sizeof(log), NULL, log);
		fprintf(stderr, "A shader of the GPU's video was not linked: %s\n", log);
		gl.DeleteProgram(program);
		return 0;
	}
	return program;
}

bool gpu_video_open(SDL_Renderer *renderer)
{
	SDL_RendererInfo info;
	gl_state state;

	if (gpu.open)
		return true;
	if (SDL_GetRendererInfo(renderer, &info) != 0
		|| (strcmp(info.name, "opengl") != 0 && strcmp(info.name, "opengles2") != 0))
		return false;
	gpu.es = strcmp(info.name, "opengles2") == 0;
#define GL_LOAD(type, name, arguments) \
	if ((*(void **)&gl.name = SDL_GL_GetProcAddress("gl" #name)) == NULL) \
	{ \
		fprintf(stderr, "The GPU's mixing: no gl" #name "\n"); \
		return false; \
	}
	GL_FUNCTIONS(GL_LOAD)
	SDL_RenderFlush(renderer);
	keep_state(&state);
	gpu.program = program_of(fragment_text);
	gpu.layer_program = gpu.program ? program_of(layer_text) : 0;
	if (!gpu.layer_program)
	{
		if (gpu.program)
			gl.DeleteProgram(gpu.program);
		put_state_back(&state);
		return false;
	}
	for (size_t uniform = 0; uniform < sizeof(uniform_names) / sizeof(uniform_names[0]); uniform++)
		gpu.places[uniform] = gl.GetUniformLocation(gpu.program, uniform_names[uniform]);
	for (int uniform = 0; uniform < L_UNIFORMS; uniform++)
		gpu.layer_places[uniform] = gl.GetUniformLocation(gpu.layer_program, layer_uniform_names[uniform]);
	gl.ActiveTexture(GL_TEXTURE0);
	gpu.vram = integer_texture(GL_R16UI, 256, 256, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
	gpu.rows = integer_texture(GL_RGBA32UI, HEIGHT, S32_VIDEO_LAYERS, GL_RGBA_INTEGER, GL_UNSIGNED_INT);
	gpu.runs = integer_texture(GL_R16UI, 16 * S32_VIDEO_LAYERS, HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
	gpu.layers = integer_texture(GL_R16UI, WIDTH, LAYER_ROWS * S32_VIDEO_LAYERS, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
	gpu.sprites = integer_texture(GL_R16UI, WIDTH, HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
	gpu.empties = integer_texture(GL_R8UI, HEIGHT, S32_VIDEO_LAYERS, GL_RED_INTEGER, GL_UNSIGNED_BYTE);
	gpu.marks = integer_texture(GL_R8UI, WIDTH, HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_BYTE);
	gpu.palette = integer_texture(GL_R16UI, PALETTE, PALETTE, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
	gpu.extra_palette = integer_texture(GL_R16UI, PALETTE, PALETTE, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
	gpu.extra_set = integer_texture(GL_R8UI, PALETTE, PALETTE, GL_RED_INTEGER, GL_UNSIGNED_BYTE);
	gpu.orders = integer_texture(GL_RGBA16UI, 16, 16, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT);
	gl.GenFramebuffers(1, &gpu.framebuffer);
	gl.GenVertexArrays(1, &gpu.array);
	put_state_back(&state);
	gpu.renderer = renderer;
	gpu.open = true;
	fprintf(stderr, "The GPU mixes the screens (%s)\n", (const char *)gl.GetString(GL_VERSION));
	return true;
}

void gpu_video_close(void)
{
	if (!gpu.open)
		return;
	gl.DeleteProgram(gpu.program);
	gl.DeleteProgram(gpu.layer_program);
	gl.DeleteTextures(1, &gpu.vram);
	gl.DeleteTextures(1, &gpu.rows);
	gl.DeleteTextures(1, &gpu.runs);
	if (gpu.tile_count)
		gl.DeleteTextures(1, &gpu.pens);
	gl.DeleteTextures(1, &gpu.layers);
	gl.DeleteTextures(1, &gpu.sprites);
	gl.DeleteTextures(1, &gpu.empties);
	gl.DeleteTextures(1, &gpu.marks);
	gl.DeleteTextures(1, &gpu.palette);
	gl.DeleteTextures(1, &gpu.extra_palette);
	gl.DeleteTextures(1, &gpu.extra_set);
	gl.DeleteTextures(1, &gpu.orders);
	gl.DeleteFramebuffers(1, &gpu.framebuffer);
	gl.DeleteVertexArrays(1, &gpu.array);
	gpu.open = false;
}

/* The tiles' pens as a texture, once (TILES_ACROSS tiles a row): false where it would be larger than the GPU
   takes, or where the tiles are not a power of two (the shader wraps a tile's number with a mask) */
static bool make_pens(void)
{
	const uint32_t count = s32_video_tile_count();
	const int tile_rows = (int)((count + TILES_ACROSS - 1) / TILES_ACROSS);
	const size_t width = TILES_ACROSS * 16;
	gl_int most = 0;
	uint8_t *atlas;

	gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, &most);
	if (count == 0 || (count & (count - 1)) != 0 || tile_rows * 16 > most
		|| (atlas = calloc(width * (size_t)tile_rows * 16, 1)) == NULL)
		return false;
	for (uint32_t tile = 0; tile < count; tile++)
	{
		const uint8_t *pens = s32_video_tile_pens(tile);

		for (int y = 0; y < 16; y++)
			memcpy(&atlas[((tile / TILES_ACROSS) * 16 + (uint32_t)y) * width + (tile % TILES_ACROSS) * 16], &pens[y * 16], 16);
	}
	gpu.pens = integer_texture(GL_R8UI, (gl_sizei)width, tile_rows * 16, GL_RED_INTEGER, GL_UNSIGNED_BYTE);
	gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (gl_sizei)width, tile_rows * 16, GL_RED_INTEGER, GL_UNSIGNED_BYTE, atlas);
	free(atlas);
	gpu.tile_count = count;
	return true;
}

/* 'texture' on texture unit 'unit', for the sampler at 'place' */
static void bind(int unit, gl_uint texture, gl_int place)
{
	gl.ActiveTexture(GL_TEXTURE0 + (gl_enum)unit);
	gl.BindTexture(GL_TEXTURE_2D, texture);
	gl.Uniform1i(place, unit);
}

bool gpu_video_draw_layers(const s32_gpu_layers *layers)
{
	gl_state state;

	gpu.layers_drawn = false;
	if (!gpu.open)
		return false;
	SDL_RenderFlush(gpu.renderer);
	keep_state(&state);
	gl.ActiveTexture(GL_TEXTURE0);
	if (!gpu.tile_count)
	{
		if (!make_pens())
		{
			put_state_back(&state);
			fprintf(stderr, "The CPU draws the layers: the GPU takes no texture of the tiles' pens\n");
			return false;
		}
		fprintf(stderr, "The GPU draws the layers too (%u tiles)\n", gpu.tile_count);
	}
	gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	gl.BindTexture(GL_TEXTURE_2D, gpu.vram);
	gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RED_INTEGER, GL_UNSIGNED_SHORT, s32_board.video_ram);
	for (int layer = 0; layer < S32_VIDEO_LAYERS; layer++)
	{
		if (layer == S32_VIDEO_LAYER_SPRITES || (layer != S32_VIDEO_LAYER_BACKDROP && !((layers->drawn >> layer) & 1)))
			continue;
		gl.BindTexture(GL_TEXTURE_2D, gpu.rows);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, layer, HEIGHT, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, layers->layers[layer].rows);
		gl.BindTexture(GL_TEXTURE_2D, gpu.runs);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, layer * 16, 0, 16, HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_SHORT,
			layers->layers[layer].runs);
	}
	gl.BindFramebuffer(GL_FRAMEBUFFER, gpu.framebuffer);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gpu.layers, 0);
	if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		put_state_back(&state);
		return false;
	}
	gl.Disable(GL_BLEND);
	gl.Disable(GL_SCISSOR_TEST);
	gl.UseProgram(gpu.layer_program);
	bind(0, gpu.vram, gpu.layer_places[L_VRAM]);
	bind(1, gpu.pens, gpu.layer_places[L_PENS]);
	bind(2, gpu.rows, gpu.layer_places[L_ROWS]);
	bind(3, gpu.runs, gpu.layer_places[L_RUNS]);
	gl.Uniform1i(gpu.layer_places[L_LEFT], layers->left);
	gl.Uniform1i(gpu.layer_places[L_RIGHT], layers->right);
	gl.Uniform1i(gpu.layer_places[L_SIDE], layers->side);
	gl.Uniform1i(gpu.layer_places[L_TILE_MASK], (gl_int)(gpu.tile_count - 1));
	gl.BindVertexArray(gpu.array);
	for (int layer = 0; layer < S32_VIDEO_LAYERS; layer++)
	{
		const s32_gpu_layer *drawn = &layers->layers[layer];

		if (layer == S32_VIDEO_LAYER_SPRITES || (layer != S32_VIDEO_LAYER_BACKDROP && !((layers->drawn >> layer) & 1)))
			continue;
		gl.Viewport(0, layer * LAYER_ROWS, layers->right + 1, HEIGHT);
		gl.Uniform1i(gpu.layer_places[L_KIND], drawn->kind);
		gl.Uniform1i(gpu.layer_places[L_LAYER], layer);
		gl.Uniform1i(gpu.layer_places[L_FIRST_DRAWN], drawn->first_drawn);
		gl.Uniform1i(gpu.layer_places[L_SHIFT], drawn->shift);
		gl.Uniform1i(gpu.layer_places[L_BANK], drawn->bank);
		gl.Uniform1i(gpu.layer_places[L_CONTROL], drawn->control);
		gl.Uniform1i(gpu.layer_places[L_SCROLL], drawn->scroll);
		gl.Uniform1i(gpu.layer_places[L_FLIP], drawn->flip);
		gl.Uniform1i(gpu.layer_places[L_BITS8], drawn->bits8);
		gl.DrawArrays(GL_TRIANGLES, 0, 3);
	}
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
	put_state_back(&state);
	gpu.layers_drawn = true;
	return true;
}


/* What the screen's mixer reads, up to the GPU */
static void upload(const s32_video_mixing *mixing)
{
	uint8_t empties[S32_VIDEO_LAYERS][HEIGHT];
	uint8_t set[PALETTE * PALETTE];
	uint16_t orders[16][16][4];

	gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);    /* (SDL's texture updates leave theirs: rows as long as each) */
	memset(empties, 0, sizeof(empties));
	for (int layer = 0; layer < S32_VIDEO_LAYERS; layer++)
	{
		if (!mixing->layers[layer] || (gpu.layers_drawn && layer != S32_VIDEO_LAYER_SPRITES))
			continue;    /* (the GPU drew the layers: the sprites alone are the CPU's) */
		gl.BindTexture(GL_TEXTURE_2D, layer == S32_VIDEO_LAYER_SPRITES ? gpu.sprites : gpu.layers);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, layer == S32_VIDEO_LAYER_SPRITES ? 0 : layer * LAYER_ROWS, WIDTH, HEIGHT,
			GL_RED_INTEGER, GL_UNSIGNED_SHORT, mixing->layers[layer]);
		for (int row = 0; mixing->empty[layer] && row < HEIGHT; row++)
			empties[layer][row] = mixing->empty[layer][row] ? 1u : 0u;
	}
	gl.BindTexture(GL_TEXTURE_2D, gpu.empties);
	gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HEIGHT, S32_VIDEO_LAYERS, GL_RED_INTEGER, GL_UNSIGNED_BYTE, empties);
	if (mixing->extra_marks)
	{
		gl.BindTexture(GL_TEXTURE_2D, gpu.marks);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIDTH, HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_BYTE, mixing->extra_marks);
		for (int index = 0; index < PALETTE * PALETTE; index++)
			set[index] = mixing->extra_set[index] ? 1u : 0u;
		gl.BindTexture(GL_TEXTURE_2D, gpu.extra_set);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PALETTE, PALETTE, GL_RED_INTEGER, GL_UNSIGNED_BYTE, set);
		gl.BindTexture(GL_TEXTURE_2D, gpu.extra_palette);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PALETTE, PALETTE, GL_RED_INTEGER, GL_UNSIGNED_SHORT, mixing->extra_palette);
	}
	gl.BindTexture(GL_TEXTURE_2D, gpu.palette);
	gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PALETTE, PALETTE, GL_RED_INTEGER, GL_UNSIGNED_SHORT, mixing->palette);
	for (int group = 0; group < 16; group++)
		for (int place = 0; place < 8; place++)
		{
			const s32_mix_entry *entry = &mixing->orders[group][place];

			orders[group][place * 2][0] = entry->palette_base;
			orders[group][place * 2][1] = entry->sprite_blend;
			orders[group][place * 2][2] = entry->blend;
			orders[group][place * 2][3] = entry->layer;
			orders[group][place * 2 + 1][0] = entry->shift;
			orders[group][place * 2 + 1][1] = entry->offsets;
			orders[group][place * 2 + 1][2] = 0;
			orders[group][place * 2 + 1][3] = 0;
		}
	gl.BindTexture(GL_TEXTURE_2D, gpu.orders);
	gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, orders);
}

void gpu_video_mix(SDL_Texture *texture, const SDL_Rect *place, int first, const s32_video_mixing *mixing)
{
	gl_uint target;
	gl_int offsets[9];
	gl_state state;

	if (!gpu.open)
		return;
	SDL_RenderFlush(gpu.renderer);    /* (SDL's own draws so far, before these) */
	if ((target = texture_name(texture)) == 0)
		return;
	keep_state(&state);
	gl.ActiveTexture(GL_TEXTURE0);
	upload(mixing);
	gl.BindFramebuffer(GL_FRAMEBUFFER, gpu.framebuffer);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
	if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
	{
		gl.Disable(GL_BLEND);
		gl.Disable(GL_SCISSOR_TEST);
		gl.Viewport(place->x, place->y, place->w, place->h);
		gl.UseProgram(gpu.program);
		bind(0, gpu.layers, gpu.places[U_LAYERS]);
		bind(1, gpu.empties, gpu.places[U_EMPTIES]);
		bind(2, gpu.marks, gpu.places[U_MARKS]);
		bind(3, gpu.palette, gpu.places[U_PALETTE]);
		bind(4, gpu.extra_palette, gpu.places[U_EXTRA_PALETTE]);
		bind(5, gpu.extra_set, gpu.places[U_EXTRA_SET]);
		bind(6, gpu.orders, gpu.places[U_ORDERS]);
		bind(7, gpu.sprites, gpu.places[U_SPRITES]);
		gl.Uniform1i(gpu.places[U_BLEND_FACTOR], mixing->blend_factor);
		gl.Uniform1i(gpu.places[U_GROUP_SHIFT], mixing->group_shift);
		gl.Uniform1i(gpu.places[U_GROUP_MASK], mixing->group_mask);
		gl.Uniform1i(gpu.places[U_SHADOW_MASK], mixing->shadow_mask);
		gl.Uniform1i(gpu.places[U_SPRITE_BITS], mixing->sprite_bits);
		gl.Uniform1i(gpu.places[U_SHADOW_PEN], mixing->shadow_pen);
		gl.Uniform1i(gpu.places[U_FLIPS], mixing->sprite_flips);
		gl.Uniform1i(gpu.places[U_HAS_MARKS], mixing->extra_marks != NULL);
		gl.Uniform1i(gpu.places[U_USE_EMPTIES], !gpu.layers_drawn);
		gl.Uniform1i(gpu.places[U_SWAP_RED_BLUE], gpu.es);
		for (int set = 0; set < 3; set++)
			for (int channel = 0; channel < 3; channel++)
				offsets[set * 3 + channel] = mixing->offsets[set][channel];
		gl.Uniform3iv(gpu.places[U_OFFSETS], 3, offsets);
		gl.Uniform4i(gl.GetUniformLocation(gpu.program, "visible"), mixing->left, mixing->right, mixing->top, mixing->bottom);
		gl.Uniform2i(gl.GetUniformLocation(gpu.program, "origin"), place->x - first, place->y);
		gl.BindVertexArray(gpu.array);
		gl.DrawArrays(GL_TRIANGLES, 0, 3);
	}
	if (!gpu.said)
	{
		/* (once, for the log: what the GPU drew into, and how it went) */
		gpu.said = true;
		fprintf(stderr, "The GPU mixes into texture %u: framebuffer %s, OpenGL error %X\n", target,
			gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE ? "complete" : "incomplete", gl.GetError());
	}
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
	put_state_back(&state);
}

bool gpu_video_read(SDL_Texture *texture, const SDL_Rect *place, uint32_t *pixels, int pitch)
{
	static uint8_t bytes[WIDTH * 2 * HEIGHT * 4];
	const gl_uint target = texture_name(texture);
	gl_state state;

	if (!gpu.open || !target || place->w > WIDTH * 2 || place->h > HEIGHT)
		return false;
	keep_state(&state);
	gl.BindFramebuffer(GL_FRAMEBUFFER, gpu.framebuffer);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
	gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
	gl.PixelStorei(GL_PACK_ROW_LENGTH, 0);
	gl.ReadPixels(place->x, place->y, place->w, place->h, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
	put_state_back(&state);
	for (int y = 0; y < place->h; y++)
		for (int x = 0; x < place->w; x++)
		{
			const uint8_t *at = &bytes[((size_t)y * (size_t)place->w + (size_t)x) * 4];

			pixels[y * pitch + x] = gpu.es ? (uint32_t)at[2] << 16 | (uint32_t)at[1] << 8 | at[0]
				: (uint32_t)at[0] << 16 | (uint32_t)at[1] << 8 | at[2];
		}
	return true;
}
