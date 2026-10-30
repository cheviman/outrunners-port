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
 *    are in the height of the controls' area, the same on every phone: on a phone, the whole screen, whose
 *    sides, where they mostly are, are what the wide picture leaves free; on a screen taller than the picture
 *    (a tablet's, an open Fold's: Sebas) the room under the picture, which goes to the top, the game's route
 *    map filling it behind them (touch_map_room). With 2 PLAYERS on a tablet
 *    (Sebas) each player has a set of them on their half of the screen, the second player's turned half round
 *    for the player across (touch_layout_facing). As SegaSonic's touch.c, with the race's controls.
 */
#include "touch.h"

#include "console.h"
#include "letters.h"

#include <math.h>
#include <stdlib.h>

#define FINGERS          10
#define SHAPE_HEIGHT     128     /* the shapes' pictures, scaled to each control */
#define EDGE             0.06    /* a ring's width, in its height */
#define WHEEL_DIRECTION  0.35    /* a turn that is a menu's arrow too, in the ring's radius */
#define BUTTON_MARGIN    0.03    /* a button takes a finger this far past its edge */
#define GLYPH_MIDDLE     7       /* font.bmp's capitals: rows 2 to 11 of their 16 */
#define ROOM_UNDER       0.25    /* the room under the picture, in the screen's height, that takes the controls */
#define AREA_ASPECT      2.2     /* the controls' unit: the area's height, or its width over this, the smaller */

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

/* Where the controls are, in the unit: over the picture on a phone's whole screen (their room at its sides
   and corners), or filling the room under it on a tall screen (the wheel and the pedals as large as it lets
   them, over the route map) */
typedef struct
{
	double wheel_x, wheel_y;     /* the wheel's place while it waits: from the left and the bottom */
	double wheel_radius;         /* its ring: turned all the way */
	double knob_radius;
	button buttons[BUTTONS];
} layout;

static const layout over_picture = {
	0.25, 0.27, 0.15, 0.07,
	{
		[BUTTON_GAS] = { FROM_BOTTOM_RIGHT, 0.20, 0.25, 0.26, 0.26, CONTROL_BIT(CONTROL_GAS),
			ACTION_BIT(ACTION_ACCEPT), "GAS", false },
		[BUTTON_BRAKE] = { FROM_BOTTOM_RIGHT, 0.48, 0.17, 0.21, 0.21, CONTROL_BIT(CONTROL_BRAKE), 0, "BRAKE", false },
		[BUTTON_SHIFT_UP] = { FROM_BOTTOM_RIGHT, 0.13, 0.58, 0.15, 0.15, CONTROL_BIT(CONTROL_SHIFT_UP), 0, "UP",
			true },
		[BUTTON_SHIFT_DOWN] = { FROM_BOTTOM_RIGHT, 0.31, 0.50, 0.15, 0.15, CONTROL_BIT(CONTROL_SHIFT_DOWN), 0,
			"DOWN", true },
		[BUTTON_START] = { FROM_TOP_RIGHT, 0.17, 0.09, 0.24, 0.09, CONTROL_BIT(CONTROL_START),
			ACTION_BIT(ACTION_START), "START", false },
		[BUTTON_MUSIC] = { FROM_BOTTOM_MIDDLE, 0.0, 0.06, 0.20, 0.08, CONTROL_BIT(CONTROL_MUSIC), 0, "MUSIC",
			false },
	},
};

static const layout under_picture = {
	0.45, 0.50, 0.36, 0.15,
	{
		[BUTTON_GAS] = { FROM_BOTTOM_RIGHT, 0.34, 0.45, 0.56, 0.56, CONTROL_BIT(CONTROL_GAS),
			ACTION_BIT(ACTION_ACCEPT), "GAS", false },
		[BUTTON_BRAKE] = { FROM_BOTTOM_RIGHT, 0.92, 0.32, 0.44, 0.44, CONTROL_BIT(CONTROL_BRAKE), 0, "BRAKE", false },
		[BUTTON_SHIFT_UP] = { FROM_BOTTOM_RIGHT, 0.70, 0.80, 0.24, 0.24, CONTROL_BIT(CONTROL_SHIFT_UP), 0, "UP",
			true },
		[BUTTON_SHIFT_DOWN] = { FROM_BOTTOM_RIGHT, 1.10, 0.80, 0.24, 0.24, CONTROL_BIT(CONTROL_SHIFT_DOWN), 0,
			"DOWN", true },
		[BUTTON_START] = { FROM_TOP_RIGHT, 0.30, 0.10, 0.40, 0.14, CONTROL_BIT(CONTROL_START),
			ACTION_BIT(ACTION_START), "START", false },
		[BUTTON_MUSIC] = { FROM_BOTTOM_MIDDLE, 0.0, 0.09, 0.30, 0.12, CONTROL_BIT(CONTROL_MUSIC), 0, "MUSIC",
			false },
	},
};

typedef enum { FINGER_FREE, FINGER_WHEEL, FINGER_BUTTON } finger_role;

/* A player's controls: on its part of the screen (the whole, or with 2 PLAYERS on a tablet a half, the
   second player's turned half round for the player across); their places as its player sees them */
typedef struct
{
	SDL_Rect canvas;            /* its part of the screen */
	bool turned;                /* turned half round about the canvas's middle */
	SDL_Rect area;              /* the controls': the canvas, or the room under the picture */
	double unit;                /* their places and sizes, in pixels */
	bool under;                 /* the area is the room under the picture */
	const layout *laid;         /* the controls' places there */
	double wheel_x, wheel_y;    /* the wheel's centre while a finger holds it */
	double turn_x, turn_y;      /* -1 to 1 */
	bool gears;                 /* the gears' buttons: touch_draw's last frame */
} pad;

static struct
{
	SDL_FingerID id;
	finger_role role;
	int pad;                  /* the controls it is on */
	int button;               /* (FINGER_BUTTON) the one under it, or -1 */
} fingers[FINGERS];

static bool shown;
static int screen_width = 1, screen_height = 1;    /* the renderer's, as it last drew */
static pad pads[TOUCH_PLAYERS] = { { .laid = &over_picture, .unit = 1.0 }, { .laid = &over_picture, .unit = 1.0 } };
static int pad_count = 1;                           /* the second's with the screens facing (touch_layout_facing) */
static const button *const buttons = over_picture.buttons;    /* (what each button does: the same in both) */
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

/* The controls' unit in their area */
static void set_unit(pad *it)
{
	it->unit = SDL_min(it->area.h, it->area.w / AREA_ASPECT);
}

void touch_open(SDL_Renderer *renderer, const char *base)
{
	const int pill_width = (int)lround(SHAPE_HEIGHT * buttons[BUTTON_START].width / buttons[BUTTON_START].height);

	SDL_GetRendererOutputSize(renderer, &screen_width, &screen_height);
	pads[0].canvas.w = pads[0].area.w = screen_width;
	pads[0].canvas.h = pads[0].area.h = screen_height;
	set_unit(&pads[0]);
	disc = make_shape(renderer, SHAPE_HEIGHT, SHAPE_HEIGHT, false);
	ring = make_shape(renderer, SHAPE_HEIGHT, SHAPE_HEIGHT, true);
	pill = make_shape(renderer, pill_width, SHAPE_HEIGHT, false);
	pill_ring = make_shape(renderer, pill_width, SHAPE_HEIGHT, true);
	have_font = letters_load(renderer, base, &font) != 0;
#ifdef ORUNNERS_TOUCH_TEST
	shown = true;    /* (a computer's check: no touch screen) */
#else
	shown = SDL_GetNumTouchDevices() > 0;
#endif
}

void touch_layout(int width, int height, SDL_Rect *picture)
{
	pad *const it = &pads[0];

	screen_width = width;
	screen_height = height;
	pad_count = 1;
	it->canvas = (SDL_Rect){ 0, 0, width, height };
	it->turned = false;
	it->under = height - picture->h >= height * ROOM_UNDER;
	if (it->under)
	{
		picture->y = 0;    /* (the picture at the top, the controls in the room under it) */
		it->area = (SDL_Rect){ 0, picture->h, width, height - picture->h };
	}
	else
		it->area = it->canvas;
	set_unit(it);
	it->laid = it->under ? &under_picture : &over_picture;
}

void touch_layout_facing(int width, int height, int picture_width, int picture_height, SDL_Rect pictures[TOUCH_PLAYERS])
{
	screen_width = width;
	screen_height = height;
	pad_count = TOUCH_PLAYERS;
	for (int player = 0; player < TOUCH_PLAYERS; player++)
	{
		pad *const it = &pads[player];
		double scale;

		/* the first player's the bottom half, the second's the top one, turned */
		it->canvas = player == 0 ? (SDL_Rect){ 0, height / 2, width, height - height / 2 } : (SDL_Rect){ 0, 0, width,
			height / 2 };
		it->turned = player == 1;
		it->under = false;
		it->area = it->canvas;
		set_unit(it);
		it->laid = &over_picture;
		scale = SDL_min((double)it->canvas.w / picture_width, (double)it->canvas.h / picture_height);
		pictures[player].w = (int)(picture_width * scale);
		pictures[player].h = (int)(picture_height * scale);
		pictures[player].x = it->canvas.x + (it->canvas.w - pictures[player].w) / 2;
		pictures[player].y = it->canvas.y + (it->canvas.h - pictures[player].h) / 2;
	}
}

/* A point of the screen as the pad's player sees it, or back (a turned pad's turned half round about its
   canvas's middle; the same for the others) */
static void seen(const pad *it, double *x, double *y)
{
	if (!it->turned)
		return;
	*x = 2.0 * it->canvas.x + it->canvas.w - *x;
	*y = 2.0 * it->canvas.y + it->canvas.h - *y;
}

/* The pad a point of the screen is on */
static int pad_at(double x, double y)
{
	for (int player = 1; player < pad_count; player++)
	{
		const SDL_Rect *canvas = &pads[player].canvas;

		if (x >= canvas->x && x < canvas->x + canvas->w && y >= canvas->y && y < canvas->y + canvas->h)
			return player;
	}
	return 0;
}

/* A button's centre and size, in pixels, as the pad's player sees them */
static void place(const pad *it, int which, double *x, double *y, double *width, double *height)
{
	const button *button = &it->laid->buttons[which];
	const double right = it->area.x + it->area.w, bottom = it->area.y + it->area.h;

	switch (button->from)
	{
		case FROM_TOP_RIGHT:
			*x = right - button->x * it->unit;
			*y = it->area.y + button->y * it->unit;
			break;
		case FROM_BOTTOM_MIDDLE:
			*x = it->area.x + it->area.w / 2.0 + button->x * it->unit;
			*y = bottom - button->y * it->unit;
			break;
		default:
			*x = right - button->x * it->unit;
			*y = bottom - button->y * it->unit;
			break;
	}
	*width = button->width * it->unit;
	*height = button->height * it->unit;
}

static bool button_shown(const pad *it, int which)
{
	return !buttons[which].gear || it->gears;
}

/* The button of the pad nearest a finger at (x, y) as its player sees it, if it reaches it; else -1 */
static int button_at(const pad *it, double x, double y)
{
	int nearest = -1;
	double nearest_distance = BUTTON_MARGIN * it->unit;

	for (int which = 0; which < BUTTONS; which++)
	{
		double centre_x, centre_y, width, height, distance;

		if (!button_shown(it, which))
			continue;
		place(it, which, &centre_x, &centre_y, &width, &height);
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

static bool wheel_held(int player)
{
	for (int slot = 0; slot < FINGERS; slot++)
		if (fingers[slot].role == FINGER_WHEEL && fingers[slot].pad == player)
			return true;
	return false;
}

/* The pad's wheel toward a finger at (x, y) as its player sees it, turned all the way at its ring and beyond */
static void turn_to(pad *it, double x, double y)
{
	const double radius = it->laid->wheel_radius * it->unit;
	const double length = hypot(x - it->wheel_x, y - it->wheel_y) / radius;
	const double keep = length > 1.0 ? 1.0 / length : 1.0;

	it->turn_x = (x - it->wheel_x) / radius * keep;
	it->turn_y = (y - it->wheel_y) / radius * keep;
}

static void finger_down(SDL_FingerID id, double x, double y)
{
	const int slot = finger_slot(id, true);
	const int player = pad_at(x, y);
	pad *const it = &pads[player];
	const double radius = it->laid->wheel_radius * it->unit;

	if (slot < 0)
		return;
	seen(it, &x, &y);
	fingers[slot].id = id;
	fingers[slot].pad = player;
	fingers[slot].button = button_at(it, x, y);
	if (fingers[slot].button >= 0)
		fingers[slot].role = FINGER_BUTTON;
	else if (x < it->canvas.x + it->canvas.w / 2.0 && !wheel_held(player))
	{
		/* the wheel where the finger lands, all of its ring in the controls' area */
		const SDL_Rect *area = &it->area;

		fingers[slot].role = FINGER_WHEEL;
		it->wheel_x = clamped(x, area->x + radius, area->x + area->w / 2.0);
		it->wheel_y = area->h > 2 * radius ? clamped(y, area->y + radius, area->y + area->h - radius)
			: area->y + area->h / 2.0;
		turn_to(it, x, y);
	}
}

/* A finger that moved: the wheel it holds turned, or the button under it */
static void finger_moved(int slot, double x, double y)
{
	pad *const it = &pads[fingers[slot].pad];

	seen(it, &x, &y);
	if (fingers[slot].role == FINGER_WHEEL)
		turn_to(it, x, y);
	else
		fingers[slot].button = button_at(it, x, y);
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
			if ((slot = finger_slot(event->tfinger.fingerId, false)) >= 0)
				finger_moved(slot, event->tfinger.x * screen_width, event->tfinger.y * screen_height);
			break;
		case SDL_FINGERUP:
			if ((slot = finger_slot(event->tfinger.fingerId, false)) < 0)
				break;
			if (fingers[slot].role == FINGER_WHEEL)
				pads[fingers[slot].pad].turn_x = pads[fingers[slot].pad].turn_y = 0.0;
			fingers[slot].role = FINGER_FREE;
			break;
		case SDL_KEYDOWN:
#ifndef ORUNNERS_TOUCH_TEST    /* (a computer's check: the keyboard drives the game to them) */
			if (event->key.keysym.scancode != SDL_SCANCODE_AC_BACK)    /* (Android's back: the screen's too) */
				shown = false;
#endif
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

/* The buttons the fingers are on, of a player's controls */
static uint32_t buttons_held(int player)
{
	uint32_t held = 0;

	for (int slot = 0; slot < FINGERS; slot++)
		if (fingers[slot].role == FINGER_BUTTON && fingers[slot].pad == player && fingers[slot].button >= 0
			&& button_shown(&pads[player], fingers[slot].button))
			held |= 1u << fingers[slot].button;
	return held;
}

uint32_t touch_actions(void)
{
	uint32_t actions = 0;

	for (int player = 0; player < pad_count; player++)
	{
		const pad *it = &pads[player];
		const uint32_t held = buttons_held(player);

		if (it->turn_x < -WHEEL_DIRECTION)
			actions |= ACTION_BIT(ACTION_LEFT);
		if (it->turn_x > WHEEL_DIRECTION)
			actions |= ACTION_BIT(ACTION_RIGHT);
		if (it->turn_y < -WHEEL_DIRECTION)
			actions |= ACTION_BIT(ACTION_UP);
		if (it->turn_y > WHEEL_DIRECTION)
			actions |= ACTION_BIT(ACTION_DOWN);
		for (int which = 0; which < BUTTONS; which++)
			if (held & (1u << which))
				actions |= buttons[which].actions;
	}
	return actions;
}

bool touch_holds(int player, int control)
{
	const uint32_t held = player < pad_count ? buttons_held(player) : 0;

	for (int which = 0; which < BUTTONS; which++)
		if ((held & (1u << which)) && (buttons[which].controls & CONTROL_BIT(control)))
			return true;
	return false;
}

int touch_wheel(int player)
{
	return player < pad_count ? (int)lround(pads[player].turn_x * TOUCH_WHEEL_FULL) : 0;
}

/* A shape centred on (x, y) as the pad's player sees it, its colour and how much it shows (the shapes are
   the same turned half round) */
static void draw_shape(SDL_Renderer *renderer, const pad *it, SDL_Texture *shape, double x, double y, double width,
	double height, Uint8 grey, Uint8 alpha)
{
	SDL_Rect where;

	if (!shape)
		return;
	seen(it, &x, &y);
	where = (SDL_Rect){ (int)lround(x - width / 2), (int)lround(y - height / 2), (int)lround(width), (int)lround(height) };
	SDL_SetTextureColorMod(shape, grey, grey, grey);
	SDL_SetTextureAlphaMod(shape, alpha);
	SDL_RenderCopy(renderer, shape, NULL, &where);
}

bool touch_map_room(SDL_Rect *room)
{
	if (pad_count != 1 || !pads[0].under)
		return false;
	*room = pads[0].area;
	return true;
}

/* A player's controls */
static void draw_pad(SDL_Renderer *renderer, int player)
{
	const pad *it = &pads[player];
	const bool wheel = wheel_held(player);
	const uint32_t held = buttons_held(player);
	const double radius = it->laid->wheel_radius * it->unit;
	const double knob = it->laid->knob_radius * it->unit;
	const double centre_x = wheel ? it->wheel_x : it->area.x + it->laid->wheel_x * it->unit;
	const double centre_y = wheel ? it->wheel_y : it->area.y + it->area.h - it->laid->wheel_y * it->unit;

	draw_shape(renderer, it, disc, centre_x, centre_y, radius * 2, radius * 2, 0, wheel ? 70 : 45);
	draw_shape(renderer, it, ring, centre_x, centre_y, radius * 2, radius * 2, 255, wheel ? 150 : 90);
	draw_shape(renderer, it, disc, centre_x + it->turn_x * radius, centre_y + it->turn_y * radius, knob * 2, knob * 2,
		255, wheel ? 170 : 90);
	for (int which = 0; which < BUTTONS; which++)
	{
		const bool pressed = (held & (1u << which)) != 0;
		const bool round = buttons[which].width == buttons[which].height;
		double x, y, width, height;

		if (!button_shown(it, which))
			continue;
		place(it, which, &x, &y, &width, &height);
		draw_shape(renderer, it, round ? disc : pill, x, y, width, height, pressed ? 255 : 0, pressed ? 110 : 80);
		draw_shape(renderer, it, round ? ring : pill_ring, x, y, width, height, 255, pressed ? 220 : 150);
		if (have_font)
		{
			const int scale = SDL_max(1, (int)lround((round ? width * 0.5 : height) * 0.4 / LETTERS_CELL));
			const SDL_Color white = { 255, 255, 255, 255 };

			SDL_SetTextureAlphaMod(font.sheet, pressed ? 255 : 190);
			seen(it, &x, &y);
			if (it->turned)
				letters_draw_turned(renderer, &font, buttons[which].label, (int)lround(x), (int)lround(y) - GLYPH_MIDDLE * scale,
					scale, white, (SDL_Point){ (int)lround(x), (int)lround(y) });
			else
				letters_draw_at(renderer, &font, buttons[which].label, (int)lround(x), (int)lround(y) - GLYPH_MIDDLE * scale,
					scale, white);
		}
	}
}

void touch_draw(SDL_Renderer *renderer, const bool gears[TOUCH_PLAYERS])
{
	for (int player = 0; player < TOUCH_PLAYERS; player++)
		pads[player].gears = gears[player];
	if (!shown)
		return;
	for (int player = 0; player < pad_count; player++)
		draw_pad(renderer, player);
}
