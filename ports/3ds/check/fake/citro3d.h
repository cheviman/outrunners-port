/* The little of citro3d ports/3ds/gpu.c uses, on a computer: its quads drawn in software, nearest, as the PICA200's
   blending and alpha test have them (ports/3ds/check) */
#ifndef CHECK_CITRO3D_H
#define CHECK_CITRO3D_H
#include "3ds.h"
#define C3D_DEFAULT_CMDBUF_SIZE 1
#define C3D_FRAME_NONBLOCK 1
#define C3D_CLEAR_ALL 7
typedef enum { GPU_RGBA5551 } GPU_TEXCOLOR;
typedef enum { GPU_NEAREST } GPU_TEXTURE_FILTER_PARAM;
typedef enum { GPU_CLAMP_TO_EDGE, GPU_REPEAT } GPU_TEXTURE_WRAP_PARAM;
typedef enum { GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PREVIOUS, GPU_CONSTANT } GPU_SRC;
typedef enum { GPU_MODULATE, GPU_REPLACE, GPU_ADD, GPU_SUBTRACT } GPU_FUNC;
typedef enum { C3D_Both, C3D_RGB } C3D_Part;
typedef enum { GPU_BLEND_ADD } GPU_BLENDEQUATION;
typedef enum { GPU_ONE, GPU_ZERO, GPU_CONSTANT_ALPHA, GPU_ONE_MINUS_CONSTANT_ALPHA } GPU_BLENDFACTOR;
typedef enum { GPU_TRIANGLES } GPU_PRIMITIVE;
typedef enum { GPU_FLOAT, GPU_UNSIGNED_BYTE } GPU_FORMATS;
typedef enum { GPU_ALWAYS, GPU_GREATER } GPU_TESTFUNC;
typedef enum { GPU_WRITE_COLOR, GPU_WRITE_ALL } GPU_WRITEMASK;
typedef enum { GPU_CULL_NONE } GPU_CULLMODE;
typedef enum { GPU_SCISSOR_NORMAL } GPU_SCISSORMODE;
typedef enum { GPU_VERTEX_SHADER } GPU_SHADER_TYPE;
typedef enum { GPU_RB_RGBA8 } GPU_COLORBUF;
typedef enum { GPU_RB_DEPTH24_STENCIL8 } GPU_DEPTHBUF;
typedef struct { void *data; u16 width, height; u32 size; GPU_TEXTURE_WRAP_PARAM wrap; } C3D_Tex;
typedef struct { GPU_SRC source; GPU_FUNC func; u32 colour; bool active; } C3D_TexEnv;
typedef struct { int unused; } C3D_RenderTarget, C3D_BufInfo, C3D_AttrInfo, C3D_Mtx;
void C3D_Init(size_t size);
void C3D_Fini(void);
C3D_RenderTarget *C3D_RenderTargetCreate(int width, int height, GPU_COLORBUF colour, GPU_DEPTHBUF depth);
void C3D_RenderTargetSetOutput(C3D_RenderTarget *target, gfxScreen_t screen, gfx3dSide_t side, u32 flags);
void C3D_RenderTargetClear(C3D_RenderTarget *target, int what, u32 colour, u32 depth);
bool C3D_FrameBegin(int flags);
void C3D_FrameDrawOn(C3D_RenderTarget *target);
void C3D_FrameEnd(int flags);
bool C3D_TexInit(C3D_Tex *texture, u16 width, u16 height, GPU_TEXCOLOR format);
void C3D_TexDelete(C3D_Tex *texture);
static inline void C3D_TexSetFilter(C3D_Tex *t, GPU_TEXTURE_FILTER_PARAM a, GPU_TEXTURE_FILTER_PARAM b) { (void)t; (void)a; (void)b; }
static inline void C3D_TexSetWrap(C3D_Tex *t, GPU_TEXTURE_WRAP_PARAM a, GPU_TEXTURE_WRAP_PARAM b) { (void)b; t->wrap = a; }
static inline void C3D_TexFlush(C3D_Tex *t) { (void)t; }
void C3D_TexBind(int unit, C3D_Tex *texture);
C3D_TexEnv *C3D_GetTexEnv(int stage);
void C3D_TexEnvInit(C3D_TexEnv *env);
void C3D_TexEnvSrc(C3D_TexEnv *env, C3D_Part part, GPU_SRC a, GPU_SRC b, int c);
void C3D_TexEnvFunc(C3D_TexEnv *env, C3D_Part part, GPU_FUNC func);
void C3D_TexEnvColor(C3D_TexEnv *env, u32 colour);
void C3D_BlendingColor(u32 colour);
void C3D_AlphaBlend(GPU_BLENDEQUATION a, GPU_BLENDEQUATION b, GPU_BLENDFACTOR source, GPU_BLENDFACTOR target, GPU_BLENDFACTOR c, GPU_BLENDFACTOR d);
void C3D_DrawArrays(GPU_PRIMITIVE primitive, int first, int count);
C3D_BufInfo *C3D_GetBufInfo(void);
static inline void BufInfo_Init(C3D_BufInfo *b) { (void)b; }
void BufInfo_Add(C3D_BufInfo *buffers, const void *data, int stride, int attributes, int order);
static inline C3D_AttrInfo *C3D_GetAttrInfo(void) { static C3D_AttrInfo it; return &it; }
static inline void AttrInfo_Init(C3D_AttrInfo *a) { (void)a; }
static inline void AttrInfo_AddLoader(C3D_AttrInfo *a, int r, GPU_FORMATS f, int n) { (void)a; (void)r; (void)f; (void)n; }
static inline void C3D_BindProgram(shaderProgram_s *s) { (void)s; }
#define S32_SCREENS_FAKE
static inline void Mtx_OrthoTilt(C3D_Mtx *m, float a, float b, float c, float d, float e, float f, bool g) { (void)m; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; }
static inline void C3D_FVUnifMtx4x4(GPU_SHADER_TYPE t, int at, C3D_Mtx *m) { (void)t; (void)at; (void)m; }
void C3D_DepthTest(bool e, GPU_TESTFUNC f, GPU_WRITEMASK w);
static inline void C3D_CullFace(GPU_CULLMODE m) { (void)m; }
static inline void C3D_AlphaTest(bool e, GPU_TESTFUNC f, int r) { (void)e; (void)f; (void)r; }
static inline void C3D_SetScissor(GPU_SCISSORMODE m, int a, int b, int c, unsigned d) { (void)m; (void)a; (void)b; (void)c; (void)d; }
/* the picture: 400 x 240, red, green, blue of 0-255 */
extern uint8_t check_screen[240][400][3];
extern uint8_t check_bottom[240][320][3];
#endif
