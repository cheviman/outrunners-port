/* The little of libctru ports/3ds/gpu.c uses, on a computer (ports/3ds/check) */
#ifndef CHECK_3DS_H
#define CHECK_3DS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef enum { GFX_TOP, GFX_BOTTOM } gfxScreen_t;
typedef enum { GFX_LEFT, GFX_RIGHT } gfx3dSide_t;
typedef struct { int unused; void *DVLE; } DVLB_s;
typedef struct { void *vertexShader; } shaderProgram_s;
#define GX_TRANSFER_FLIP_VERT(x) 0
#define GX_TRANSFER_OUT_TILED(x) 0
#define GX_TRANSFER_RAW_COPY(x) 0
#define GX_TRANSFER_IN_FORMAT(x) 0
#define GX_TRANSFER_OUT_FORMAT(x) 0
#define GX_TRANSFER_SCALING(x) 0
#define GX_TRANSFER_FMT_RGBA8 0
#define GX_TRANSFER_SCALE_NO 0
static inline float osGet3DSliderState(void) { const char *it = getenv("CHECK_SLIDER"); return it ? (float)atof(it) : 0.0f; }
static inline void gfxSet3D(bool on) { (void)on; }
static inline void *linearAlloc(size_t size) { return malloc(size); }
static inline DVLB_s *DVLB_ParseFile(u32 *data, u32 size) { static void *one; static DVLB_s it = { 0, &one }; (void)data; (void)size; return &it; }
static inline void shaderProgramInit(shaderProgram_s *it) { (void)it; }
static inline void shaderProgramSetVsh(shaderProgram_s *it, void *d) { (void)it; (void)d; }
static inline int shaderInstanceGetUniformLocation(void *s, const char *n) { (void)s; (void)n; return 0; }
#endif
