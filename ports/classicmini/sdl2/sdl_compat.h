#ifndef SDL_COMPAT_H
#define SDL_COMPAT_H
#include <SDL.h>
#if !SDL_VERSION_ATLEAST(2, 0, 18)
#ifndef SDL_clamp
#define SDL_clamp(x, a, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))
#endif
#define SDL_CONTROLLER_BUTTON_MISC1 15
#define SDL_CONTROLLER_BUTTON_PADDLE1 16
#define SDL_CONTROLLER_BUTTON_PADDLE2 17
#define SDL_CONTROLLER_BUTTON_PADDLE3 18
#define SDL_CONTROLLER_BUTTON_PADDLE4 19
#define SDL_CONTROLLER_BUTTON_TOUCHPAD 20
#define SDL_HINT_ANDROID_TRAP_BACK_BUTTON "SDL_ANDROID_TRAP_BACK_BUTTON"
#define SDL_HINT_GAMECONTROLLERCONFIG_FILE "SDL_GAMECONTROLLERCONFIG_FILE"
#define SDL_HINT_MOUSE_TOUCH_EVENTS "SDL_MOUSE_TOUCH_EVENTS"
#define SDL_HINT_TOUCH_MOUSE_EVENTS "SDL_TOUCH_MOUSE_EVENTS"
typedef enum { SDL_CONTROLLER_TYPE_UNKNOWN = 0, SDL_CONTROLLER_TYPE_XBOX360, SDL_CONTROLLER_TYPE_XBOXONE,
	SDL_CONTROLLER_TYPE_PS3, SDL_CONTROLLER_TYPE_PS4, SDL_CONTROLLER_TYPE_PS5 } SDL_GameControllerType;
typedef enum { SDL_ScaleModeNearest, SDL_ScaleModeLinear, SDL_ScaleModeBest } SDL_ScaleMode;
static inline SDL_GameControllerType SDL_GameControllerGetType(SDL_GameController *c) { (void)c; return SDL_CONTROLLER_TYPE_UNKNOWN; }
static inline int SDL_GameControllerRumble(SDL_GameController *c, Uint16 l, Uint16 h, Uint32 ms) { (void)c; (void)l; (void)h; (void)ms; return -1; }
static inline int SDL_SetTextureScaleMode(SDL_Texture *t, SDL_ScaleMode m) { (void)t; (void)m; return -1; }
static inline int SDL_RenderSetVSync(SDL_Renderer *r, int v) { (void)r; (void)v; return -1; }
static inline int SDL_RenderSetIntegerScale(SDL_Renderer *r, SDL_bool e) { (void)r; (void)e; return 0; }
static inline int SDL_GetDisplayUsableBounds(int i, SDL_Rect *r) { return SDL_GetDisplayBounds(i, r); }
static inline SDL_Surface *SDL_CreateRGBSurfaceWithFormat(Uint32 f, int w, int h, int d, Uint32 fmt)
{
	int bpp; Uint32 rm, gm, bm, am; (void)f; (void)d;
	if (!SDL_PixelFormatEnumToMasks(fmt, &bpp, &rm, &gm, &bm, &am)) return NULL;
	return SDL_CreateRGBSurface(0, w, h, bpp, rm, gm, bm, am);
}
static inline SDL_JoystickID SDL_JoystickGetDeviceInstanceID(int index)
{
	SDL_Joystick *j = SDL_JoystickOpen(index); SDL_JoystickID id = -1;
	if (j) { id = SDL_JoystickInstanceID(j); SDL_JoystickClose(j); }
	return id;
}
#endif
#endif
