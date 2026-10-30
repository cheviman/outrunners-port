/**
 *    Name:        touch.c
 *    Description: the touch screen's controls (Android): the wheel, GAS, BRAKE, the gears, the radio and START
 *                 drawn over the picture, and the controls and actions the fingers on them hold
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    As few as the race needs (Sebas): the wheel is where a finger lands on the screen's left half (it waits
 *    at the bottom left, dim), turned as far as the finger goes across from there, all the way at its ring,
 *    and the menus' arrows past a third of the way. GAS (the menus' accept) and BRAKE on the right, START at
 *    the top right (the race's pause); SHIFT UP and DOWN above GAS in a race with the manual transmission
 *    alone (none with the automatic); the radio a small oval at the bottom. A finger that slides from one
 *    button to another presses the one under it. Back in the menus is Android's own back. Places and sizes
 *    are in the screen's height, the same on every phone: a phone's sides, where they mostly are, are what
 *    the 16:9 picture leaves free. As SegaSonic's touch.c, with the race's controls.
 */
#include "touch.h"

#include "console.h"
#include "letters.h"

#include <math.h>
#include <stdlib.h>

#define FINGERS          10
#define SHAPE_HEIGHT     128     /* the shapes' pictures, scaled to each control */
#define EDGE             0.06    /* a ring's width, in its height */
#define WHEEL_X          0.25    /* the wheel's place while it waits: from the left and the bottom */
#define WHEEL_Y          0.27
#define WHEEL_RADIUS     0.15    /* its ring: turned all the way */
#define KNOB_RADIUS      0.07
#define WHEEL_DIRECTION  0.35    /* a turn that is a menu's arrow too, in the ring's radius */
#define BUTTON_MARGIN    0.03    /* a button takes a finger this far past its edge */
#define GLYPH_MIDDLE     7       /* font.bmp's capitals: rows 2 to 11 of their 16 */

#define CONTROL_BIT(control) (1u << (control))

typedef enum
{
	FROM_BOTTOM_RIGHT,    /* x from the right, y from the bottom */
	FROM_TOP_RIGHT,
	FROM_BOTTOM_MIDDLE    /* x from the middle */
} anchor;

typedef struct
{
	anchor from;
	double x, y;              /* its centre, from its anchor */
	double width, height;     /* (as wide as high: a disc; wider: a pill) */
	uint32_t controls;        /* CONTROL_BIT */
	uint32_t actions;         /* ACTION_BIT */
	const char *label;
	bool gear;                /* shown in a race with the manual transmission alone */
} button;

enum { BUTTON_GAS, BUTTON_BRAKE, BUTTON_SHIFT_UP, BUTTON_SHIFT_DOWN, BUTTON_START, BUTTON_MUSIC, BUTTONS };

static const button buttons[BUTTONS] = {
	[BUTTON_GAS] = { FROM_BOTTOM_RIGHT, 0.20, 0.25, 0.26, 0.26, CONTROL_BIT(CONTROL_GAS), ACTION_BIT(ACTION_ACCEPT),
		"GAS", false },
	[BUTTON_BRAKE] = { FROM_BOTTOM_RIGHT, 0.48, 0.17, 0.21, 0.21, CONTROL_BIT(CONTROL_BRAKE), 0, "BRAKE", false },
	[BUTTON_SHIFT_UP] = { FROM_BOTTOM_RIGHT, 0.13, 0.58, 0.15, 0.15, CONTROL_BIT(CONTROL_SHIFT_UP), 0, "UP", true },
	[BUTTON_SHIFT_DOWN] = { FROM_BOTTOM_RIGHT, 0.31, 0.50, 0.15, 0.15, CONTROL_BIT(CONTROL_SHIFT_DOWN), 0, "DOWN",
		true },
	[BUTTON_START] = { FROM_TOP_RIGHT, 0.17, 0.09, 0.24, 0.09, CONTROL_BIT(CONTROL_START), ACTION_BIT(ACTION_START),
		"START", false },
	[BUTTON_MUSIC] = { FROM_BOTTOM_MIDDLE, 0.0, 0.06, 0.20, 0.08, CONTROL_BIT(CONTROL_MUSIC), 0, "MUSIC", false },
};

typedef enum { FINGER_FREE, FINGER_WHEEL, FINGER_BUTTON } finger_role;

static struct
{
	SDL_FingerID id;
	finger_role role;
	int button;               /* (FINGER_BUTTON) the one under it, or -1 */
} fingers[FINGERS];

static bool shown;
static bool gears_shown;                            /* the gears' buttons: touch_draw's last frame */
static int screen_width = 1, screen_height = 1;    /* the renderer's, as it last drew */
static double wheel_x, wheel_y;                     /* the wheel's centre while a finger holds it */
static double turn_x, turn_y;                       /* -1 to 1 */
static SDL_Texture *disc, *ring, *pill, *pill_ring;
static letters font;
static bool have_font;

static double clamped(double value, double low, double high)
{
	return value < low ? low : (value > high ? high : value);
}

/* The distance from (x, y) to the edge of a pill centred on (0, 0), as wide as it is high a disc:
   negative inside */
static double pill_distance(double x, double y, double width, double height)
{
	const double radius = height / 2;
	const double along = fabs(x) - (width / 2 - radius);    /* (past its straight part) */

	return along > 0 ? hypot(along, y) - radius : fabs(y) - radius;
}

/* A shape in white, its edge smooth: filled, or its ring alone; tinted when it is drawn */
static SDL_Texture *make_shape(SDL_Renderer *renderer, int width, int height, bool edge_only)
{
	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
	const double edge = EDGE * height;
	SDL_Texture *texture;

	if (!surface)
		return NULL;
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			const double distance = pill_distance(x + 0.5 - width / 2.0, y + 0.5 - height / 2.0, width, height);
			const double inside = edge_only ? fabs(distance + edge / 2) - edge / 2 : distance;
			const Uint32 cover = (Uint32)lround(255.0 * clamped(0.5 - inside, 0.0, 1.0));

			((Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch))[x] = cover << 24 | 0xFFFFFFu;
		}
	texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	if (texture)
	{
		SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
	}
	return texture;
}

void touch_open(SDL_Renderer *renderer, const char *base)
{
	const int pill_width = (int)lround(SHAPE_HEIGHT * buttons[BUTTON_START].width / buttons[BUTTON_START].height);

	SDL_GetRendererOutputSize(renderer, &screen_width, &screen_height);
	disc = make_shape(renderer, SHAPE_HEIGHT, SHAPE_HEIGHT, false);
	ring = make_shape(renderer, SHAPE_HEIGHT, SHAPE_HEIGHT, true);
	pill = make_shape(renderer, pill_width, SHAPE_HEIGHT, false);
	pill_ring = make_shape(renderer, pill_width, SHAPE_HEIGHT, true);
	have_font = letters_load(renderer, base, &font) != 0;
	shown = SDL_GetNumTouchDevices() > 0;
}

/* A button's centre and size on the screen, in pixels */
static void place(int which, double *x, double *y, double *width, double *height)
{
	const button *it = &buttons[which];

	switch (it->from)
	{
		case FROM_TOP_RIGHT:
			*x = screen_width - it->x * screen_height;
			*y = it->y * screen_height;
			break;
		case FROM_BOTTOM_MIDDLE:
			*x = screen_width / 2.0 + it->x * screen_height;
			*y = screen_height - it->y * screen_height;
			break;
		default:
			*x = screen_width - it->x * screen_height;
			*y = screen_height - it->y * screen_height;
			break;
	}
	*width = it->width * screen_height;
	*height = it->height * screen_height;
}

static bool button_shown(int which)
{
	return !buttons[which].gear || gears_shown;
}

/* The button nearest a finger at (x, y), if it reaches it; else -1 */
static int button_at(double x, double y)
{
	int nearest = -1;
	double nearest_distance = BUTTON_MARGIN * screen_height;

	for (int which = 0; which < BUTTONS; which++)
	{
		double centre_x, centre_y, width, height, distance;

		if (!button_shown(which))
			continue;
		place(which, &centre_x, &centre_y, &width, &height);
		distance = pill_distance(x - centre_x, y - centre_y, width, height);
		if (distance <= nearest_distance)
		{
			nearest = which;
			nearest_distance = distance;
		}
	}
	return nearest;
}

/* The finger's slot, or with 'take' a free one for it; -1 when there is none */
static int finger_slot(SDL_FingerID id, bool take)
{
	int free_slot = -1;

	for (int slot = 0; slot < FINGERS; slot++)
	{
		if (fingers[slot].role != FINGER_FREE && fingers[slot].id == id)
			return slot;
		if (free_slot < 0 && fingers[slot].role == FINGER_FREE)
			free_slot = slot;
	}
	return take ? free_slot : -1;
}

static bool wheel_held(void)
{
	for (int slot = 0; slot < FINGERS; slot++)
		if (fingers[slot].role == FINGER_WHEEL)
			return true;
	return false;
}

/* The wheel toward a finger at (x, y), turned all the way at its ring and beyond */
static void turn_to(double x, double y)
{
	const double radius = WHEEL_RADIUS * screen_height;
	const double length = hypot(x - wheel_x, y - wheel_y) / radius;
	const double keep = length > 1.0 ? 1.0 / length : 1.0;

	turn_x = (x - wheel_x) / radius * keep;
	turn_y = (y - wheel_y) / radius * keep;
}

static void finger_down(SDL_FingerID id, double x, double y)
{
	const double radius = WHEEL_RADIUS * screen_height;
	const int slot = finger_slot(id, true);

	if (slot < 0)
		return;
	fingers[slot].id = id;
	fingers[slot].button = button_at(x, y);
	if (fingers[slot].button >= 0)
		fingers[slot].role = FINGER_BUTTON;
	else if (x < screen_width / 2.0 && !wheel_held())
	{
		/* the wheel where the finger lands, all of its ring on the screen */
		fingers[slot].role = FINGER_WHEEL;
		wheel_x = clamped(x, radius, screen_width / 2.0);
		wheel_y = clamped(y, radius, screen_height - radius);
		turn_to(x, y);
	}
}

void touch_event(const SDL_Event *event)
{
	int slot;

	switch (event->type)
	{
		case SDL_FINGERDOWN:
			shown = true;
			finger_down(event->tfinger.fingerId, event->tfinger.x * screen_width, event->tfinger.y * screen_height);
			break;
		case SDL_FINGERMOTION:
			if ((slot = finger_slot(event->tfinger.fingerId, false)) < 0)
				break;
			if (fingers[slot].role == FINGER_WHEEL)
				turn_to(event->tfinger.x * screen_width, event->tfinger.y * screen_height);
			else
				fingers[slot].button = button_at(event->tfinger.x * screen_width, event->tfinger.y * screen_height);
			break;
		case SDL_FINGERUP:
			if ((slot = finger_slot(event->tfinger.fingerId, false)) < 0)
				break;
			if (fingers[slot].role == FINGER_WHEEL)
				turn_x = turn_y = 0.0;
			fingers[slot].role = FINGER_FREE;
			break;
		case SDL_KEYDOWN:
			if (event->key.keysym.scancode != SDL_SCANCODE_AC_BACK)    /* (Android's back: the screen's too) */
				shown = false;
			break;
		case SDL_CONTROLLERBUTTONDOWN:
			shown = false;
			break;
		case SDL_CONTROLLERAXISMOTION:
			if (abs(event->caxis.value) > TOUCH_WHEEL_FULL / 2)
				shown = false;
			break;
		default:
			break;
	}
}

/* The buttons the fingers are on */
static uint32_t buttons_held(void)
{
	uint32_t held = 0;

	for (int slot = 0; slot < FINGERS; slot++)
		if (fingers[slot].role == FINGER_BUTTON && fingers[slot].button >= 0 && button_shown(fingers[slot].button))
			held |= 1u << fingers[slot].button;
	return held;
}

uint32_t touch_actions(void)
{
	const uint32_t held = buttons_held();
	uint32_t actions = 0;

	if (turn_x < -WHEEL_DIRECTION)
		actions |= ACTION_BIT(ACTION_LEFT);
	if (turn_x > WHEEL_DIRECTION)
		actions |= ACTION_BIT(ACTION_RIGHT);
	if (turn_y < -WHEEL_DIRECTION)
		actions |= ACTION_BIT(ACTION_UP);
	if (turn_y > WHEEL_DIRECTION)
		actions |= ACTION_BIT(ACTION_DOWN);
	for (int which = 0; which < BUTTONS; which++)
		if (held & (1u << which))
			actions |= buttons[which].actions;
	return actions;
}

bool touch_holds(int control)
{
	const uint32_t held = buttons_held();

	for (int which = 0; which < BUTTONS; which++)
		if ((held & (1u << which)) && (buttons[which].controls & CONTROL_BIT(control)))
			return true;
	return false;
}

int touch_wheel(void)
{
	return (int)lround(turn_x * TOUCH_WHEEL_FULL);
}

/* A shape centred on (x, y), its colour and how much it shows */
static void draw_shape(SDL_Renderer *renderer, SDL_Texture *shape, double x, double y, double width, double height,
	Uint8 grey, Uint8 alpha)
{
	const SDL_Rect where = { (int)lround(x - width / 2), (int)lround(y - height / 2), (int)lround(width),
		(int)lround(height) };

	if (!shape)
		return;
	SDL_SetTextureColorMod(shape, grey, grey, grey);
	SDL_SetTextureAlphaMod(shape, alpha);
	SDL_RenderCopy(renderer, shape, NULL, &where);
}

void touch_draw(SDL_Renderer *renderer, bool gears)
{
	bool wheel;
	uint32_t held;
	double radius, knob, centre_x, centre_y;

	SDL_GetRendererOutputSize(renderer, &screen_width, &screen_height);
	gears_shown = gears;
	if (!shown)
		return;
	wheel = wheel_held();
	held = buttons_held();
	radius = WHEEL_RADIUS * screen_height;
	knob = KNOB_RADIUS * screen_height;
	centre_x = wheel ? wheel_x : WHEEL_X * screen_height;
	centre_y = wheel ? wheel_y : screen_height - WHEEL_Y * screen_height;
	draw_shape(renderer, disc, centre_x, centre_y, radius * 2, radius * 2, 0, wheel ? 70 : 45);
	draw_shape(renderer, ring, centre_x, centre_y, radius * 2, radius * 2, 255, wheel ? 150 : 90);
	draw_shape(renderer, disc, centre_x + turn_x * radius, centre_y + turn_y * radius, knob * 2, knob * 2, 255,
		wheel ? 170 : 90);
	for (int which = 0; which < BUTTONS; which++)
	{
		const bool pressed = (held & (1u << which)) != 0;
		const bool round = buttons[which].width == buttons[which].height;
		double x, y, width, height;

		if (!button_shown(which))
			continue;
		place(which, &x, &y, &width, &height);
		draw_shape(renderer, round ? disc : pill, x, y, width, height, pressed ? 255 : 0, pressed ? 110 : 80);
		draw_shape(renderer, round ? ring : pill_ring, x, y, width, height, 255, pressed ? 220 : 150);
		if (have_font)
		{
			const int scale = SDL_max(1, (int)lround((round ? width * 0.5 : height) * 0.4 / LETTERS_CELL));
			const SDL_Color white = { 255, 255, 255, 255 };

			SDL_SetTextureAlphaMod(font.sheet, pressed ? 255 : 190);
			letters_draw_at(renderer, &font, buttons[which].label, (int)lround(x), (int)lround(y) - GLYPH_MIDDLE * scale,
				scale, white);
		}
	}
}
