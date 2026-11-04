/**
 *    Name:        fake.c
 *    Description: citro3d's calls gpu.c makes, in software: axis-aligned quads, nearest, RGBA5551 textures in the
 *                 PICA200's layout, the three shading stages, the alpha test and blending
 *    Author:      yoshisuga
 *
 *    license:BSD-3-Clause
 */
#include <citro3d.h>

#include <math.h>
#include <string.h>

typedef struct { float x, y, z, u, v; uint32_t colour; } vertex;

uint8_t check_screen[240][400][3];
uint8_t check_bottom[240][320][3];
static uint8_t (*drawing)[3];
static int drawing_width = 400;
static const vertex *vertices;
static C3D_Tex *bound;
static C3D_TexEnv stages[3];
static float source_part = 1.0f;
static float target_part = 0.0f;
static float constant_alpha;
static C3D_RenderTarget targets[3];
static int target_count;
static const C3D_RenderTarget *bottom_target;
static uint8_t morton[8][8];
static float depths[240][400];
static bool depth_writes;
static bool depth_tests = true;

void C3D_Init(size_t size)
{
	(void)size;
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++)
			morton[y][x] = (uint8_t)((x & 1) | (y & 1) << 1 | (x & 2) << 1 | (y & 2) << 2 | (x & 4) << 2 | (y & 4) << 3);
}

void C3D_Fini(void) { }
C3D_RenderTarget *C3D_RenderTargetCreate(int w, int h, GPU_COLORBUF c, GPU_DEPTHBUF d) { (void)w; (void)h; (void)c; (void)d; return &targets[target_count++]; }
void C3D_RenderTargetSetOutput(C3D_RenderTarget *t, gfxScreen_t s, gfx3dSide_t side, u32 f) { (void)side; (void)f; if (s == GFX_BOTTOM) bottom_target = t; }
void C3D_RenderTargetClear(C3D_RenderTarget *t, int w, u32 c, u32 d)
{
	(void)w; (void)c; (void)d;
	if (t == bottom_target)
		memset(check_bottom, 0, sizeof(check_bottom));
	else
		memset(check_screen, 0, sizeof(check_screen));
	memset(depths, 0, sizeof(depths));
}

bool C3D_FrameBegin(int flags) { (void)flags; return true; }
void C3D_FrameDrawOn(C3D_RenderTarget *t)
{
	drawing = t == bottom_target ? &check_bottom[0][0] : &check_screen[0][0];
	drawing_width = t == bottom_target ? 320 : 400;
}

void C3D_FrameEnd(int flags) { (void)flags; }

bool C3D_TexInit(C3D_Tex *t, u16 w, u16 h, GPU_TEXCOLOR f)
{
	(void)f;
	t->width = w;
	t->height = h;
	t->size = (u32)w * h * 2u;
	t->data = malloc(t->size);
	return t->data != NULL;
}

void C3D_TexDelete(C3D_Tex *t) { free(t->data); t->data = NULL; }
void C3D_TexBind(int unit, C3D_Tex *t) { (void)unit; bound = t; }
void C3D_DepthTest(bool e, GPU_TESTFUNC f, GPU_WRITEMASK w) { (void)f; depth_tests = e && f != GPU_ALWAYS; depth_writes = w == GPU_WRITE_ALL; }
C3D_TexEnv *C3D_GetTexEnv(int stage) { return &stages[stage]; }
void C3D_TexEnvInit(C3D_TexEnv *e) { memset(e, 0, sizeof(*e)); }
void C3D_TexEnvSrc(C3D_TexEnv *e, C3D_Part p, GPU_SRC a, GPU_SRC b, int c) { (void)p; (void)b; (void)c; e->source = a; }
void C3D_TexEnvFunc(C3D_TexEnv *e, C3D_Part p, GPU_FUNC f) { (void)p; e->func = f; e->active = true; }
void C3D_TexEnvColor(C3D_TexEnv *e, u32 c) { e->colour = c; }
void C3D_BlendingColor(u32 c) { constant_alpha = (float)(c >> 24) / 255.0f; }
void C3D_AlphaBlend(GPU_BLENDEQUATION a, GPU_BLENDEQUATION b, GPU_BLENDFACTOR s, GPU_BLENDFACTOR t, GPU_BLENDFACTOR c, GPU_BLENDFACTOR d)
{
	(void)a; (void)b; (void)c; (void)d;
	source_part = s == GPU_CONSTANT_ALPHA ? constant_alpha : (s == GPU_ZERO ? 0.0f : 1.0f);
	target_part = t == GPU_ONE_MINUS_CONSTANT_ALPHA ? 1.0f - constant_alpha : (t == GPU_CONSTANT_ALPHA ? constant_alpha : 0.0f);
}
C3D_BufInfo *C3D_GetBufInfo(void) { static C3D_BufInfo it; return &it; }
void BufInfo_Add(C3D_BufInfo *b, const void *d, int s, int a, int o) { (void)b; (void)s; (void)a; (void)o; vertices = d; }

static uint16_t texel(const C3D_Tex *t, float u, float v)
{
	int x = (int)floorf(u * t->width), y = (int)floorf(v * t->height);
	int flipped;

	if (t->wrap == GPU_REPEAT)
	{
		x &= t->width - 1;
		y &= t->height - 1;
	}
	else
	{
		x = x < 0 ? 0 : (x >= t->width ? t->width - 1 : x);
		y = y < 0 ? 0 : (y >= t->height ? t->height - 1 : y);
	}
	flipped = t->height - 1 - y;
	return ((const uint16_t *)t->data)[(flipped & ~7) * t->width + (x & ~7) * 8 + morton[flipped & 7][x & 7]];
}

static void draw_quad(const vertex *q)
{
	const float left = q[0].x, top = q[0].y, right = q[2].x, bottom = q[2].y;
	const bool textured = stages[0].source == GPU_TEXTURE0;

	for (int y = (int)ceilf(top - 0.5f); y < (int)ceilf(bottom - 0.5f); y++)
	{
		if (y < 8 || y >= 232)
			continue;
		for (int x = (int)ceilf(left - 0.5f); x < (int)ceilf(right - 0.5f); x++)
		{
			float colour[3];

			if (x < 0 || x >= drawing_width)
				continue;
			if (textured)
			{
				const float across = ((float)x + 0.5f - left) / (right - left), down = ((float)y + 0.5f - top) / (bottom - top);
				const uint16_t value = texel(bound, q[0].u + across * (q[2].u - q[0].u), q[0].v + down * (q[2].v - q[0].v));

				if (!(value & 1))
					continue;
				colour[0] = (float)((value >> 11) & 31) / 31.0f;
				colour[1] = (float)((value >> 6) & 31) / 31.0f;
				colour[2] = (float)((value >> 1) & 31) / 31.0f;
			}
			else
				for (int channel = 0; channel < 3; channel++)
					colour[channel] = (float)((q[0].colour >> (8 * channel)) & 255) / 255.0f;
			if (depth_tests && q[0].z <= depths[y][x])
				continue;
			if (depth_writes)
				depths[y][x] = q[0].z;
			for (int stage = 1; stage < 3; stage++)
				if (stages[stage].active)
					for (int channel = 0; channel < 3; channel++)
					{
						const float amount = (float)((stages[stage].colour >> (8 * channel)) & 255) / 255.0f;

						colour[channel] += stages[stage].func == GPU_ADD ? amount : -amount;
						colour[channel] = colour[channel] < 0.0f ? 0.0f : (colour[channel] > 1.0f ? 1.0f : colour[channel]);
					}
			for (int channel = 0; channel < 3; channel++)
			{
				uint8_t *out = &drawing[y * drawing_width + x][channel];
				const float value = colour[channel] * 255.0f * source_part + (float)*out * target_part;

				*out = (uint8_t)(value + 0.5f);
			}
		}
	}
}

void C3D_DrawArrays(GPU_PRIMITIVE p, int first, int count)
{
	(void)p;
	for (int index = first; index + 6 <= first + count; index += 6)
		draw_quad(vertices + index);
}
