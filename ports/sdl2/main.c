/**
 *    Name:        main.c
 *    Description: the game in a window: SDL2 shows its screens, plays its sound, reads the controls and
 *                 keeps the pace, around the same game and board the headless runs check
 *    Version:     0.1.0
 *    Created:     21-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Usage: "OutRunners Arcade" or OutRunners [--script FILE [--fast]] [--hashes FILE] [--video-hashes FILE]
 *                    [--sound-hashes FILE] [--assets FOLDER]
 *
 *    The game (src/) and the board (runtime/) run as they are: this file only wraps them. Where the board
 *    has its hooks, the port does its part: when VBLANK starts the video draws both screens and the port
 *    puts them in the window, the left screen beside the right (the console version, OutRunners: the left
 *    one alone, both in 2 PLAYERS); when the board reads the controls the port hands it the keyboard and
 *    the game controllers; when the game finishes a frame the port queues the frame's sound and waits for
 *    the frame's time (60 a second) to pass. What the board keeps with the power off is kept in two files,
 *    beside the executable on Windows and in the user's folder for the game elsewhere (saves_folder):
 *    orunners.eeprom the EEPROM (the settings and their copy), orunners.bookkeeping the part of the sound
 *    board's RAM the game keeps on the battery (the bookkeeping, the ten course records and the credits
 *    among it, its checksum and its copy); the console version's are outrunners.eeprom and
 *    outrunners.bookkeeping.
 *
 *    The game's data is its assets (game/assets.h), the files extract_assets.py writes from the player's
 *    ROM set: --assets, or the folder 'assets' beside the executable, beside the .app on macOS, in the
 *    folder of what the game keeps, or in the current folder.
 *
 *    --script plays a control script (runtime/include/s32/script.h) instead of the keyboard, to its end,
 *    from a blank EEPROM and sound RAM that it does not keep; --fast runs it without waiting for the frames'
 *    time.
 *    --hashes, --video-hashes and --sound-hashes write for each frame the hashes ports/headless and
 *    tools/core/system32/reference/s32ref write, so a script run in the window compares with the reference
 *    frame by frame.
 *
 *    Controls (the cabinet's player 1; player 2 on the second game controller):
 *      left, right arrows             the wheel (it comes back to the centre when let go)
 *      up arrow, down arrow           gas, brake
 *      Q, A                           shift up, shift down
 *      space                          DJ (the music)      [ ]    << and >> (the song)
 *      5, 6                           coins 1-2           1, 2   starts 1-2
 *      9                              service             F2     test (the operator's menu)
 *      Escape                         quit                F11 or Alt+Enter   full screen
 *      F3                             smooth or sharp pixels
 *    Game controllers: the left stick is the wheel, the right trigger the gas, the left trigger the brake,
 *    the right and left shoulders shift up and down, Y the DJ, the pad's left and right << and >>, Start
 *    the start, Back a coin.
 */
#include "s32/machine.h"
#include "s32/program.h"
#include "s32/script.h"
#include "s32/video.h"
#include "sound/chips.h"
#include "game/assets.h"
#ifdef ORUNNERS_CONSOLE
#include "console.h"
#include "controls.h"
#endif

#include <SDL.h>

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The console version (console/, OutRunners) shows the left seat's screen alone, both in 2 PLAYERS, and
   plays both sides' sound on both speakers; the arcade one (OutRunners Arcade) both screens, each side's
   sound on its own speaker. Each keeps its own files */
#ifdef ORUNNERS_CONSOLE
#define TITLE               "OutRunners"
#define FILE_NAME           "outrunners"
#else
#define TITLE               "OutRunners Arcade"
#define FILE_NAME           "orunners"
#endif
#define PLAYERS             2
#define WINDOW_SCALE        2
#define QUEUE_FRAMES_MAX    6          /* sound queued beyond this many frames is dropped */
#define WHEEL_LEFT          0x10       /* the wheel's travel, as MAME has it */
#define WHEEL_CENTRE        0x80
#define WHEEL_RIGHT         0xf0
#define WHEEL_TURN          6          /* a frame, while a key holds it */
#define WHEEL_RETURN        8          /* a frame, back to the centre */
#define PEDAL_PRESS         0x20       /* a frame, while a key holds it */
#define PEDAL_LIFT          0x30
#define STICK_DEAD_ZONE     4000
#define BACKUP_RAM_START    0x1f00     /* what the game keeps of the sound board's RAM: 701F00-701FF9 */
#define BACKUP_RAM_SIZE     0xfa

#define USAGE "Usage: orunners [--script FILE [--fast]] [--hashes FILE] [--video-hashes FILE] [--sound-hashes FILE]" \
	" [--assets FILE]"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio;
static SDL_GameController *controllers[PLAYERS];
static bool smooth;
static uint64_t next_frame;           /* performance counter value the next frame waits for */
static char eeprom_path[1024];
static char backup_ram_path[1024];
static uint8_t backup_ram_saved[BACKUP_RAM_SIZE];    /* as the file has it */
static s32_script *script;
static bool fast;
static FILE *hashes;
static FILE *video_hashes;
static FILE *sound_hashes;

/* The wheels and pedals as the keyboard leaves them, frame to frame */
static int wheel[PLAYERS] = { WHEEL_CENTRE, WHEEL_CENTRE };
static int gas[PLAYERS];
static int brake[PLAYERS];

/* ---- files ---- */

static void save_eeprom(void)
{
	FILE *output;

	if (script || !s32_board.eeprom.dirty)    /* (a script starts from a blank EEPROM: the player's stays) */
		return;
	output = fopen(eeprom_path, "wb");
	if (!output)
		return;
	fwrite(s32_board.eeprom.data, 1, sizeof(s32_board.eeprom.data), output);
	fclose(output);
	s32_board.eeprom.dirty = false;
}

static void load_eeprom(void)
{
	FILE *input = fopen(eeprom_path, "rb");
	uint16_t data[S32_EEPROM_WORDS];

	if (!input)
		return;
	if (fread(data, 1, sizeof(data), input) == sizeof(data))
		memcpy(s32_board.eeprom.data, data, sizeof(data));
	fclose(input);
}

/* The sound board's 8 KB RAM is on the board's battery. The game keeps its bookkeeping in it (the checksum
   at 701F00, the block from 701F02 with the course records at 701F32 and the credits at 701F74, its copy
   from 701F7E: bookkeeping_check, load_course_records); the rest is the sound program's, which clears
   E000-FDFF (700000-701DFF) when it starts, and the game clears 701FFF. MAME keeps none of it */
static void save_backup_ram(void)
{
	const uint8_t *kept = &s32_board.shared_ram[BACKUP_RAM_START];
	FILE *output;

	if (script || memcmp(kept, backup_ram_saved, BACKUP_RAM_SIZE) == 0)    /* (a script's stays apart too) */
		return;
	output = fopen(backup_ram_path, "wb");
	if (!output)
		return;
	if (fwrite(kept, 1, BACKUP_RAM_SIZE, output) == BACKUP_RAM_SIZE)
		memcpy(backup_ram_saved, kept, BACKUP_RAM_SIZE);
	fclose(output);
}

static void load_backup_ram(void)
{
	FILE *input = fopen(backup_ram_path, "rb");
	uint8_t data[BACKUP_RAM_SIZE];

	if (!input)
		return;
	if (fread(data, 1, sizeof(data), input) == sizeof(data))
	{
		memcpy(&s32_board.shared_ram[BACKUP_RAM_START], data, sizeof(data));
		memcpy(backup_ram_saved, data, sizeof(data));
	}
	fclose(input);
}

/* ---- the picture ---- */

/* The screens the window shows, from the left one: the arcade's two; the console's left one, both in
   2 PLAYERS */
static int screens_shown(void)
{
#ifdef ORUNNERS_CONSOLE
	return console_both_screens() ? 2 : 1;
#else
	return 2;
#endif
}

/* One screen over the other (the console's 2 PLAYERS, VERTICAL) */
static bool screens_stacked(void)
{
#ifdef ORUNNERS_CONSOLE
	return screens_shown() == 2 && console_window_settings.vertical;
#else
	return false;
#endif
}

/* The window's picture, in the game's pixels: its screens side by side, or one over the other */
/* A screen's picture, in the game's pixels: 320, or 16:9 with the console's WIDE */
#define WIDE_WIDTH 398    /* 16:9 at 224 rows: the video's 416 less 9 at each side */

static int screen_width(void)
{
#ifdef ORUNNERS_CONSOLE
	if (console_window_settings.wide)
		return WIDE_WIDTH;
#endif
	return 320;
}

static void picture_size(int *width, int *height)
{
	*width = (screens_stacked() ? 1 : screens_shown()) * screen_width();
	*height = (screens_stacked() ? 2 : 1) * S32_VIDEO_HEIGHT;
}

/* ---- the pixels: sharp, smooth ("sharp bilinear": a whole number of times with sharp pixels into 'scaled',
   then that softened the rest of the way, so only the pixels' edges blend), plain bilinear, or CRT (a
   multiplying overlay on smooth pixels, the size the picture is shown at), as SegaSonic's ---- */

#define CRT_SCANLINE_DARK  0.45    /* how much the scanlines' edges darken (their middles keep it all) */
#define CRT_MASK           0.80    /* the other two channels in a phosphor column */
#define CRT_VIGNETTE       0.18    /* how much the corners darken */

static SDL_Texture *scaled;
static int scaled_times;
static SDL_Texture *crt_overlay;
static int crt_width, crt_height, crt_rows;

/* For each pixel of the shown picture: its game row's scanline (bright in the middle, dark at the edges),
   the column's phosphor (red, green, blue in turn), and the vignette toward the edges */
static void make_crt_overlay(int width, int height, int rows)
{
	uint32_t *pixels;
	int pitch = 0;

	if (crt_overlay && crt_width == width && crt_height == height && crt_rows == rows)
		return;
	if (crt_overlay)
		SDL_DestroyTexture(crt_overlay);
	crt_overlay = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	crt_width = width;
	crt_height = height;
	crt_rows = rows;
	if (!crt_overlay || SDL_LockTexture(crt_overlay, NULL, (void **)&pixels, &pitch) != 0)
		return;
	for (int y = 0; y < height; y++)
	{
		const double row = (y + 0.5) * rows / height;
		const double scan = 1.0 - CRT_SCANLINE_DARK * (1.0 - sin(3.14159265358979 * (row - floor(row))));
		const double dy = (2.0 * y / height - 1.0);

		for (int x = 0; x < width; x++)
		{
			const double dx = (2.0 * x / width - 1.0);
			const double light = scan * (1.0 - CRT_VIGNETTE * (dx * dx * dy * dy));
			const int phosphor = x % 3;
			const int red = (int)(255.0 * light * (phosphor == 0 ? 1.0 : CRT_MASK));
			const int green = (int)(255.0 * light * (phosphor == 1 ? 1.0 : CRT_MASK));
			const int blue = (int)(255.0 * light * (phosphor == 2 ? 1.0 : CRT_MASK));

			pixels[y * (pitch / 4) + x] = 0xFF000000u | (uint32_t)red << 16 | (uint32_t)green << 8 | (uint32_t)blue;
		}
	}
	SDL_UnlockTexture(crt_overlay);
	SDL_SetTextureBlendMode(crt_overlay, SDL_BLENDMODE_MOD);
}

/* The pixels the window shows: the console's OPTIONS > VIDEO; the arcade's F3, sharp or smooth */
static int pixels_kind(void)
{
#ifdef ORUNNERS_CONSOLE
	return console_window_settings.pixels;
#else
	return smooth ? 1 : 0;
#endif
}

/* The picture (its part 'source' of 'texture') in the window, as large as it fits with its proportions */
static void present(SDL_Rect source)
{
	const int kind = pixels_kind();
	SDL_Rect shown;
	int out_width = 0, out_height = 0;
	double scale;

	SDL_GetRendererOutputSize(renderer, &out_width, &out_height);
	scale = SDL_min((double)out_width / source.w, (double)out_height / source.h);
	shown.w = (int)(source.w * scale);
	shown.h = (int)(source.h * scale);
	shown.x = (out_width - shown.w) / 2;
	shown.y = (out_height - shown.h) / 2;
	SDL_SetTextureScaleMode(texture, kind == 2 ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	SDL_RenderClear(renderer);
	if (kind == 1 || kind == 3)
	{
		const int times = SDL_max(1, (int)scale);
		SDL_Rect whole = { 0, 0, source.w * times, source.h * times };

		if (!scaled || scaled_times != times)
		{
			if (scaled)
				SDL_DestroyTexture(scaled);
			scaled = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
				2 * S32_VIDEO_MAX_WIDTH * times, 2 * S32_VIDEO_HEIGHT * times);
			if (scaled)
				SDL_SetTextureScaleMode(scaled, SDL_ScaleModeLinear);
			scaled_times = times;
		}
		if (scaled)
		{
			SDL_SetRenderTarget(renderer, scaled);
			SDL_RenderCopy(renderer, texture, &source, &whole);
			SDL_SetRenderTarget(renderer, NULL);
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, scaled, &whole, &shown);
		}
		if (kind == 3)
		{
			make_crt_overlay(shown.w, shown.h, source.h);
			if (crt_overlay)
				SDL_RenderCopy(renderer, crt_overlay, NULL, &shown);
		}
	}
	else
		SDL_RenderCopy(renderer, texture, &source, &shown);
	SDL_RenderPresent(renderer);
}

#ifdef ORUNNERS_CONSOLE
/* ---- the CONTROLS page's keys and buttons: Kenney's Input Prompts Pixel (CC0), 16 x 16 tiles, as
   SegaSonic's ---- */

typedef struct
{
	int what;      /* an SDL_Scancode or an SDL_GameControllerButton */
	uint8_t row, column, tiles;    /* where in the sheet, and how many tiles wide */
} prompt;

static const prompt key_prompts[] = {
	{ SDL_SCANCODE_ESCAPE, 0, 17, 1 }, { SDL_SCANCODE_F1, 0, 18, 1 }, { SDL_SCANCODE_F2, 0, 19, 1 },
	{ SDL_SCANCODE_F3, 0, 20, 1 }, { SDL_SCANCODE_F4, 0, 21, 1 }, { SDL_SCANCODE_F5, 0, 22, 1 },
	{ SDL_SCANCODE_F6, 0, 23, 1 }, { SDL_SCANCODE_F7, 0, 24, 1 }, { SDL_SCANCODE_F8, 0, 25, 1 },
	{ SDL_SCANCODE_F9, 0, 26, 1 }, { SDL_SCANCODE_F10, 0, 27, 1 }, { SDL_SCANCODE_F11, 0, 28, 1 },
	{ SDL_SCANCODE_F12, 0, 29, 1 },
	{ SDL_SCANCODE_1, 1, 17, 1 }, { SDL_SCANCODE_2, 1, 18, 1 }, { SDL_SCANCODE_3, 1, 19, 1 },
	{ SDL_SCANCODE_4, 1, 20, 1 }, { SDL_SCANCODE_5, 1, 21, 1 }, { SDL_SCANCODE_6, 1, 22, 1 },
	{ SDL_SCANCODE_7, 1, 23, 1 }, { SDL_SCANCODE_8, 1, 24, 1 }, { SDL_SCANCODE_9, 1, 25, 1 },
	{ SDL_SCANCODE_0, 1, 26, 1 }, { SDL_SCANCODE_MINUS, 1, 27, 1 }, { SDL_SCANCODE_EQUALS, 1, 29, 1 },
	{ SDL_SCANCODE_BACKSPACE, 1, 32, 2 },
	{ SDL_SCANCODE_Q, 2, 17, 1 }, { SDL_SCANCODE_W, 2, 18, 1 }, { SDL_SCANCODE_E, 2, 19, 1 },
	{ SDL_SCANCODE_R, 2, 20, 1 }, { SDL_SCANCODE_T, 2, 21, 1 }, { SDL_SCANCODE_Y, 2, 22, 1 },
	{ SDL_SCANCODE_U, 2, 23, 1 }, { SDL_SCANCODE_I, 2, 24, 1 }, { SDL_SCANCODE_O, 2, 25, 1 },
	{ SDL_SCANCODE_P, 2, 26, 1 }, { SDL_SCANCODE_LEFTBRACKET, 2, 27, 1 }, { SDL_SCANCODE_RIGHTBRACKET, 2, 28, 1 },
	{ SDL_SCANCODE_BACKSLASH, 2, 31, 1 },
	{ SDL_SCANCODE_A, 3, 18, 1 }, { SDL_SCANCODE_S, 3, 19, 1 }, { SDL_SCANCODE_D, 3, 20, 1 },
	{ SDL_SCANCODE_F, 3, 21, 1 }, { SDL_SCANCODE_G, 3, 22, 1 }, { SDL_SCANCODE_H, 3, 23, 1 },
	{ SDL_SCANCODE_J, 3, 24, 1 }, { SDL_SCANCODE_K, 3, 25, 1 }, { SDL_SCANCODE_L, 3, 26, 1 },
	{ SDL_SCANCODE_APOSTROPHE, 3, 27, 1 }, { SDL_SCANCODE_SEMICOLON, 3, 30, 1 },
	{ SDL_SCANCODE_RETURN, 3, 32, 2 }, { SDL_SCANCODE_KP_ENTER, 3, 32, 2 },
	{ SDL_SCANCODE_Z, 4, 19, 1 }, { SDL_SCANCODE_X, 4, 20, 1 }, { SDL_SCANCODE_C, 4, 21, 1 },
	{ SDL_SCANCODE_V, 4, 22, 1 }, { SDL_SCANCODE_B, 4, 23, 1 }, { SDL_SCANCODE_N, 4, 24, 1 },
	{ SDL_SCANCODE_M, 4, 25, 1 }, { SDL_SCANCODE_COMMA, 4, 26, 1 }, { SDL_SCANCODE_PERIOD, 4, 27, 1 },
	{ SDL_SCANCODE_SLASH, 4, 29, 1 },
	{ SDL_SCANCODE_UP, 4, 30, 1 }, { SDL_SCANCODE_RIGHT, 4, 31, 1 }, { SDL_SCANCODE_DOWN, 4, 32, 1 },
	{ SDL_SCANCODE_LEFT, 4, 33, 1 },
	{ SDL_SCANCODE_LALT, 5, 17, 2 }, { SDL_SCANCODE_RALT, 5, 17, 2 }, { SDL_SCANCODE_TAB, 5, 19, 2 },
	{ SDL_SCANCODE_DELETE, 5, 21, 2 }, { SDL_SCANCODE_END, 5, 23, 2 },
	{ SDL_SCANCODE_LCTRL, 6, 17, 2 }, { SDL_SCANCODE_RCTRL, 6, 17, 2 }, { SDL_SCANCODE_CAPSLOCK, 6, 19, 2 },
	{ SDL_SCANCODE_HOME, 6, 21, 2 }, { SDL_SCANCODE_PAGEUP, 6, 23, 2 }, { SDL_SCANCODE_PAGEDOWN, 6, 25, 2 },
	{ SDL_SCANCODE_SPACE, 6, 32, 2 },
	{ SDL_SCANCODE_LSHIFT, 7, 17, 2 }, { SDL_SCANCODE_RSHIFT, 7, 17, 2 }, { SDL_SCANCODE_INSERT, 7, 19, 2 },
	{ SDL_SCANCODE_PAUSE, 7, 25, 2 },
};

static const prompt button_prompts[] = {
	{ SDL_CONTROLLER_BUTTON_A, 0, 4, 1 }, { SDL_CONTROLLER_BUTTON_B, 0, 5, 1 },
	{ SDL_CONTROLLER_BUTTON_X, 0, 6, 1 }, { SDL_CONTROLLER_BUTTON_Y, 0, 7, 1 },
	{ SDL_CONTROLLER_BUTTON_DPAD_UP, 1, 1, 1 }, { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1, 2, 1 },
	{ SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1, 3, 1 }, { SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1, 4, 1 },
	{ SDL_CONTROLLER_BUTTON_BACK, 18, 4, 1 }, { SDL_CONTROLLER_BUTTON_START, 18, 5, 1 },
	{ SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 18, 7, 1 }, { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 18, 8, 1 },
};

#define PROMPT_SIZE  16
#define PROMPT_CLEAR 0xFFFF00FFu    /* magenta: the sheet's see-through */

static SDL_Surface *prompt_sheet;    /* input_prompts.bmp beside the executable, as ARGB */
static uint32_t controls_picture[S32_VIDEO_MAX_WIDTH * S32_VIDEO_HEIGHT];

static void load_prompts(const char *base)
{
	char path[1024];
	SDL_Surface *loaded;

	snprintf(path, sizeof(path), "%sinput_prompts.bmp", base);
	loaded = SDL_LoadBMP(path);
	if (!loaded)
		return;    /* (CONTROLS then shows the names alone) */
	prompt_sheet = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0);
	SDL_FreeSurface(loaded);
}

static const prompt *find_prompt(const prompt *list, size_t count, int what)
{
	for (size_t i = 0; i < count; i++)
		if (list[i].what == what)
			return &list[i];
	return NULL;
}

/* One prompt into the picture, its top left at (x, y) */
static void draw_prompt(const prompt *icon, int x, int y)
{
	const uint32_t *sheet = (const uint32_t *)prompt_sheet->pixels;
	const int pitch = prompt_sheet->pitch / 4;

	for (int row = 0; row < PROMPT_SIZE; row++)
		for (int column = 0; column < PROMPT_SIZE * icon->tiles; column++)
		{
			const uint32_t color = sheet[(icon->row * PROMPT_SIZE + row) * pitch + icon->column * PROMPT_SIZE + column];

			if ((color | 0xFF000000u) == PROMPT_CLEAR || y + row < 0 || y + row >= S32_VIDEO_HEIGHT
				|| x + column >= S32_VIDEO_MAX_WIDTH)
				continue;
			controls_picture[(y + row) * S32_VIDEO_MAX_WIDTH + x + column] = color | 0xFF000000u;
		}
}

/* The left screen's frame with each of the shown player's controls' keys and first button on its line */
static const uint32_t *with_controls(const uint32_t *pixels)
{
	const control_binding *bindings = control_bindings[console_controls_player()];

	memcpy(controls_picture, pixels, sizeof(controls_picture));
	for (int control = 0; control < CONTROLS; control++)
	{
		const prompt *icon, *first = NULL;
		int shown = 0, x, y;

		if (!console_controls_place(control, &x, &y))
			continue;
		for (int key = 0; key < CONTROL_KEYS && shown < 2; key++)
		{
			icon = find_prompt(key_prompts, SDL_arraysize(key_prompts), bindings[control].keys[key]);
			if (!icon || (first && first->row == icon->row && first->column == icon->column))
				continue;    /* (no picture, or the first key's: Enter and the keypad's) */
			draw_prompt(icon, x + (shown++ ? CONSOLE_CONTROLS_SECOND_X : 0), y);
			first = first ? first : icon;
		}
		if ((icon = find_prompt(button_prompts, SDL_arraysize(button_prompts), bindings[control].buttons[0])) != NULL)
			draw_prompt(icon, x + CONSOLE_CONTROLS_BUTTON_X, y);
	}
	return controls_picture;
}
#endif

static void on_frame_shown(void)
{
	SDL_Rect source = { 0, 0, 0, 0 };
	const int shown = screens_shown();
	const bool stacked = screens_stacked();

	s32_video_draw_frame();
	/* each screen into the texture: the left one at its top left, the right one beside it or under it; with
	   WIDE a frame of 416 pixels loses 9 at each side, one of 320 sits between black bars */
	for (int screen = 0; screen < shown; screen++)
	{
		static uint32_t bars[S32_VIDEO_MAX_WIDTH * S32_VIDEO_HEIGHT];    /* (black) */
		int width = 0;
		const uint32_t *pixels = s32_video_frame(screen, &width);
#ifdef ORUNNERS_CONSOLE
		const int cell = screen_width();
#else
		const int cell = width;    /* (the arcade: the board's frame as it is, 320 or 416) */
#endif
		SDL_Rect place;

#ifdef ORUNNERS_CONSOLE
		int ignored_x, ignored_y;

		if (screen == 0 && prompt_sheet && console_controls_place(0, &ignored_x, &ignored_y))
			pixels = with_controls(pixels);    /* (OPTIONS > CONTROLS: the keys' and buttons' pictures) */
#endif

		place.x = stacked ? 0 : screen * cell;
		place.y = stacked ? screen * S32_VIDEO_HEIGHT : 0;
		place.w = cell;
		place.h = S32_VIDEO_HEIGHT;
		if (width > cell)
			pixels += (width - cell) / 2;    /* (the middle of a wider frame) */
		else if (width < cell)
		{
			SDL_UpdateTexture(texture, &place, bars, S32_VIDEO_MAX_WIDTH * (int)sizeof(uint32_t));
			place.x += (cell - width) / 2;
			place.w = width;
		}
		SDL_UpdateTexture(texture, &place, pixels, S32_VIDEO_MAX_WIDTH * (int)sizeof(uint32_t));
		source.w = stacked ? cell : source.w + cell;
		source.h = stacked ? source.h + S32_VIDEO_HEIGHT : S32_VIDEO_HEIGHT;
	}
	present(source);
}

static void toggle_full_screen(void)
{
	const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0;

	SDL_SetWindowFullscreen(window, full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#ifdef ORUNNERS_CONSOLE
	console_window_settings.full_screen = full;
	console_window_settings.changed = true;
#endif
}

static void toggle_smooth(void)
{
#ifdef ORUNNERS_CONSOLE
	console_window_settings.pixels = (console_window_settings.pixels + 1) % PIXELS_KINDS;
	console_window_settings.changed = true;
#else
	smooth = !smooth;
#endif
}

#ifdef ORUNNERS_CONSOLE
/* ---- the console's window settings (OPTIONS > VIDEO, VOLUME), kept in outrunners.ini ---- */

static char settings_path[1024];
static int window_width, window_height;    /* the picture the window was last sized for */

static const char *const pixel_names[PIXELS_KINDS] = { "sharp", "smooth", "bilinear", "crt" };

static void save_window_settings(void)
{
	const console_window *settings = &console_window_settings;
	FILE *output;

	if (script)
		return;
	output = fopen(settings_path, "w");
	if (!output)
		return;
	fprintf(output, "window_scale=%d\nfull_screen=%d\npixels=%s\nwide=%d\ntwo_players=%s\nvolume=%d\n",
		settings->window_scale, settings->full_screen, pixel_names[settings->pixels], settings->wide,
		settings->vertical ? "vertical" : "horizontal", settings->volume);
	fclose(output);
}

static void load_window_settings(void)
{
	console_window *settings = &console_window_settings;
	char line[128], text[32];
	int number;
	FILE *input = fopen(settings_path, "r");

	if (!input)
		return;
	while (fgets(line, sizeof(line), input))
	{
		if (sscanf(line, "window_scale=%d", &number) == 1 && number >= 1 && number <= 6)
			settings->window_scale = number;
		else if (sscanf(line, "full_screen=%d", &number) == 1)
			settings->full_screen = number != 0;
		else if (sscanf(line, "wide=%d", &number) == 1)
			settings->wide = number != 0;
		else if (sscanf(line, "volume=%d", &number) == 1 && number >= 0 && number <= 10)
			settings->volume = number;
		else if (sscanf(line, "pixels=%31s", text) == 1)
		{
			for (int kind = 0; kind < PIXELS_KINDS; kind++)
				if (strcmp(text, pixel_names[kind]) == 0)
					settings->pixels = kind;
		}
		else if (sscanf(line, "two_players=%31s", text) == 1)
			settings->vertical = strcmp(text, "vertical") == 0;
	}
	fclose(input);
}

/* The window as the settings say: full screen or not, and out of it the picture times window_scale (the
   largest whole scale under it at which it fits the screen) */
static void apply_window_settings(void)
{
	const console_window *settings = &console_window_settings;
	const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
	int width, height;

	picture_size(&width, &height);
	if (full != settings->full_screen)
		SDL_SetWindowFullscreen(window, settings->full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if (!settings->full_screen && (width != window_width || height != window_height || settings->changed))
	{
		SDL_Rect usable;
		const int display = SDL_GetWindowDisplayIndex(window);
		int scale = settings->window_scale;

		if (SDL_GetDisplayUsableBounds(display < 0 ? 0 : display, &usable) == 0)
			while (scale > 1 && (width * scale > usable.w - 64 || height * scale > usable.h - 64))
				scale--;
		SDL_SetWindowSize(window, width * scale, height * scale);
		SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	}
	window_width = width;
	window_height = height;
}

/* The menus' actions: the arrows, Enter (START too), Escape or Backspace; each game controller's pad or
   left stick, A, B and Start */
static uint32_t actions_held;
static bool actions_taken;    /* a key or button just bound on CONTROLS: it is no action this frame */

static uint32_t read_actions(const Uint8 *keys)
{
	uint32_t held = 0;

	if (keys[SDL_SCANCODE_UP])
		held |= ACTION_BIT(ACTION_UP);
	if (keys[SDL_SCANCODE_DOWN])
		held |= ACTION_BIT(ACTION_DOWN);
	if (keys[SDL_SCANCODE_LEFT])
		held |= ACTION_BIT(ACTION_LEFT);
	if (keys[SDL_SCANCODE_RIGHT])
		held |= ACTION_BIT(ACTION_RIGHT);
	if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER])
		held |= ACTION_BIT(ACTION_ACCEPT) | ACTION_BIT(ACTION_START);
	if (keys[SDL_SCANCODE_ESCAPE] || keys[SDL_SCANCODE_BACKSPACE])
		held |= ACTION_BIT(ACTION_BACK);
	for (int player = 0; player < PLAYERS; player++)
	{
		SDL_GameController *pad = controllers[player];
		int stick_x, stick_y;

		if (!pad)
			continue;
		stick_x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
		stick_y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || stick_y < -16000)
			held |= ACTION_BIT(ACTION_UP);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || stick_y > 16000)
			held |= ACTION_BIT(ACTION_DOWN);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || stick_x < -16000)
			held |= ACTION_BIT(ACTION_LEFT);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || stick_x > 16000)
			held |= ACTION_BIT(ACTION_RIGHT);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A))
			held |= ACTION_BIT(ACTION_ACCEPT);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
			held |= ACTION_BIT(ACTION_BACK);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START))
			held |= ACTION_BIT(ACTION_START);
	}
	return held;
}
#endif

/* ---- the controls ---- */

static void read_events(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				program_stop();    /* (back to main, which saves: the game runs on a task's stack, which a
				                      longjmp to main's does not leave on Windows) */
				return;
			case SDL_KEYDOWN:
				if (event.key.repeat)
					break;
#ifdef ORUNNERS_CONSOLE
				if (console_controls_waiting() >= 0)
				{
					/* CONTROLS waits for a key: this one (Escape leaves the control as it was) */
					if (event.key.keysym.sym != SDLK_ESCAPE)
						controls_bind_key(console_controls_player(), console_controls_waiting(),
							event.key.keysym.scancode);
					console_controls_bound();
					actions_taken = true;
					break;
				}
#endif
#ifndef ORUNNERS_CONSOLE    /* (the console's Escape is the menus' back; its menu's EXIT quits) */
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					program_stop();
					return;
				}
#endif
				if (event.key.keysym.sym == SDLK_F11
					|| (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)))
					toggle_full_screen();
				if (event.key.keysym.sym == SDLK_F3)
					toggle_smooth();
				break;
#ifdef ORUNNERS_CONSOLE
			case SDL_CONTROLLERBUTTONDOWN:
				if (console_controls_waiting() >= 0)
				{
					/* ... or a button of the game controller plugged in for that player */
					for (int player = 0; player < PLAYERS; player++)
						if (controllers[player] && player == console_controls_player()
							&& SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controllers[player])) == event.cbutton.which)
						{
							controls_bind_button(player, console_controls_waiting(),
								(SDL_GameControllerButton)event.cbutton.button);
							console_controls_bound();
							actions_taken = true;
						}
				}
				break;
#endif
			case SDL_CONTROLLERDEVICEADDED:
				for (int player = 0; player < PLAYERS; player++)
					if (!controllers[player])
					{
						controllers[player] = SDL_GameControllerOpen(event.cdevice.which);
						break;
					}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				for (int player = 0; player < PLAYERS; player++)
					if (controllers[player]
						&& SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controllers[player])) == event.cdevice.which)
					{
						SDL_GameControllerClose(controllers[player]);
						controllers[player] = NULL;
					}
				break;
			default:
				break;
		}
	}
}

static int clamp(int value, int low, int high)
{
	return value < low ? low : (value > high ? high : value);
}

/* the keyboard's wheel: turned while a key holds it, back to the centre when let go */
static int steer(int position, bool left, bool right)
{
	if (left != right)
		return clamp(position + (right ? WHEEL_TURN : -WHEEL_TURN), WHEEL_LEFT, WHEEL_RIGHT);
	if (position < WHEEL_CENTRE)
		return position + WHEEL_RETURN > WHEEL_CENTRE ? WHEEL_CENTRE : position + WHEEL_RETURN;
	return position - WHEEL_RETURN < WHEEL_CENTRE ? WHEEL_CENTRE : position - WHEEL_RETURN;
}

static int press(int position, bool held)
{
	return clamp(position + (held ? PEDAL_PRESS : -PEDAL_LIFT), 0, 0xff);
}

/* a trigger's travel as a pedal's */
static int trigger_pedal(SDL_GameController *pad, SDL_GameControllerAxis axis)
{
	return SDL_GameControllerGetAxis(pad, axis) * 0xff / 32767;
}

/* the state of every control once a frame, where the board reads them (VBLANK) */
static void on_controls_update(void)
{
	const Uint8 *keys;
	s32_inputs *input = &s32_board.input;
	bool shift_up[PLAYERS] = { false }, shift_down[PLAYERS] = { false };
	bool music[PLAYERS] = { false }, previous[PLAYERS] = { false }, next[PLAYERS] = { false };
	bool coin[PLAYERS] = { false }, start[PLAYERS] = { false };
	int wheels[PLAYERS], gases[PLAYERS], brakes[PLAYERS];

	read_events();
	if (script)
	{
		s32_script_apply(script, s32_board.frame);
		return;
	}
	keys = SDL_GetKeyboardState(NULL);
#ifdef ORUNNERS_CONSOLE
	{
		/* the menus' actions newly pressed; EXIT; the settings OPTIONS changed */
		const uint32_t held = read_actions(keys);

		s32_video_set_wide(console_wide_now());    /* (before VBLANK's sprites and the frame) */

		if (!actions_taken)
			console_actions(held & ~actions_held);
		actions_taken = false;
		actions_held = held;
		if (console_controls_defaults())
			controls_defaults(console_controls_player());
		if (console_quit_requested())
		{
			program_stop();
			return;
		}
		if (console_window_settings.changed)
		{
			apply_window_settings();
			save_window_settings();
			console_window_settings.changed = false;
		}
		else
		{
			int width, height;

			picture_size(&width, &height);
			if (width != window_width || height != window_height)
				apply_window_settings();    /* (2 PLAYERS: the window takes both screens, and back) */
		}
	}
#endif
#ifdef ORUNNERS_CONSOLE
	/* each seat's keys and game controller buttons, as OPTIONS > CONTROLS binds them (controls.c) */
	for (int player = 0; player < PLAYERS; player++)
	{
		SDL_GameController *pad = controllers[player];

		wheel[player] = steer(wheel[player], controls_holds(player, pad, CONTROL_STEER_LEFT),
			controls_holds(player, pad, CONTROL_STEER_RIGHT));
		gas[player] = press(gas[player], controls_holds(player, pad, CONTROL_GAS));
		brake[player] = press(brake[player], controls_holds(player, pad, CONTROL_BRAKE));
		shift_up[player] = controls_holds(player, pad, CONTROL_SHIFT_UP);
		shift_down[player] = controls_holds(player, pad, CONTROL_SHIFT_DOWN);
		music[player] = controls_holds(player, pad, CONTROL_MUSIC);
		previous[player] = controls_holds(player, pad, CONTROL_SONG_BACK);
		next[player] = controls_holds(player, pad, CONTROL_SONG_NEXT);
		start[player] = controls_holds(player, pad, CONTROL_START);
	}
#else
	wheel[0] = steer(wheel[0], keys[SDL_SCANCODE_LEFT], keys[SDL_SCANCODE_RIGHT]);
	gas[0] = press(gas[0], keys[SDL_SCANCODE_UP]);
	brake[0] = press(brake[0], keys[SDL_SCANCODE_DOWN]);
	shift_up[0] = keys[SDL_SCANCODE_Q] != 0;
	shift_down[0] = keys[SDL_SCANCODE_A] != 0;
	music[0] = keys[SDL_SCANCODE_SPACE] != 0;
	previous[0] = keys[SDL_SCANCODE_LEFTBRACKET] != 0;
	next[0] = keys[SDL_SCANCODE_RIGHTBRACKET] != 0;
#endif
	for (int player = 0; player < PLAYERS; player++)
	{
		SDL_GameController *pad = controllers[player];

#ifndef ORUNNERS_CONSOLE
		coin[player] = keys[SDL_SCANCODE_5 + player] != 0;
		start[player] = keys[SDL_SCANCODE_1 + player] != 0;
#endif
		wheels[player] = wheel[player];
		gases[player] = gas[player];
		brakes[player] = brake[player];
		if (!pad)
			continue;
		/* the controller's stick and triggers, where they are moved, over the keyboard's */
		{
			const int stick = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
			const int pad_gas = trigger_pedal(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
			const int pad_brake = trigger_pedal(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);

			if (stick > STICK_DEAD_ZONE || stick < -STICK_DEAD_ZONE)
				wheels[player] = clamp(WHEEL_CENTRE + stick * (WHEEL_RIGHT - WHEEL_CENTRE) / 32767, WHEEL_LEFT, WHEEL_RIGHT);
			if (pad_gas > gases[player])
				gases[player] = pad_gas;
			if (pad_brake > brakes[player])
				brakes[player] = pad_brake;
		}
#ifndef ORUNNERS_CONSOLE    /* (the console's buttons are its bindings') */
		shift_up[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
		shift_down[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
		music[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y) != 0;
		previous[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
		next[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
		start[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START) != 0;
		coin[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) != 0;
#endif
	}

#ifdef ORUNNERS_CONSOLE
	/* the console: no coins, no operator's switches; a seat whose screen is not shown is let alone */
	for (int player = 0; player < PLAYERS; player++)
	{
		coin[player] = false;
		if (player >= screens_shown())
		{
			shift_up[player] = shift_down[player] = music[player] = previous[player] = next[player] = false;
			start[player] = false;
			wheels[player] = WHEEL_CENTRE;
			gases[player] = brakes[player] = 0;
		}
	}
#endif
	/* each player's 315-5296 (chip 0 player 1, chip 1 player 2); its ports are active low: a cleared bit
	   is a pressed button */
	for (int player = 0; player < PLAYERS; player++)
	{
		input->port_a[player] = (uint8_t)(0xff & ~(shift_up[player] ? 0x01 : 0) & ~(shift_down[player] ? 0x02 : 0));
		input->port_b[player] = (uint8_t)(0xff & ~(music[player] ? 0x01 : 0) & ~(previous[player] ? 0x02 : 0)
			& ~(next[player] ? 0x04 : 0));
		input->port_e[player] = (uint8_t)(0xff & ~(coin[player] ? 0x04 : 0) & ~(start[player] ? 0x10 : 0));
	}
#ifndef ORUNNERS_CONSOLE
	if (keys[SDL_SCANCODE_9])
		input->port_e[0] &= (uint8_t)~0x01;
	if (keys[SDL_SCANCODE_F2])
		input->port_e[0] &= (uint8_t)~0x02;
#endif
	input->analog[0] = (uint8_t)wheels[0];
	input->analog[1] = (uint8_t)gases[0];
	input->analog[2] = (uint8_t)brakes[0];
	input->analog[3] = (uint8_t)wheels[1];
	input->analog[6] = (uint8_t)gases[1];
	input->analog[7] = (uint8_t)brakes[1];
}

/* ---- the frame ---- */

static void wait_for_frame(void)
{
	const uint64_t frequency = SDL_GetPerformanceFrequency();
	uint64_t now = SDL_GetPerformanceCounter();

	next_frame += frequency / S32_FRAME_RATE;
	if (now > next_frame + frequency / 4)
		next_frame = now;    /* far behind (the window was moved, the machine was busy): start over */
	while ((now = SDL_GetPerformanceCounter()) < next_frame)
	{
		const uint64_t left = (next_frame - now) * 1000 / frequency;
		if (left > 2)
			SDL_Delay((Uint32)(left - 1));
	}
}

static void on_frame_done(void)
{
	const uint64_t frame = program_frames() - 1;
	size_t count;
	const int16_t *mix;

	/* (in the headless runs' order: the board's hashes, then the sound) */
	if (hashes)
		fprintf(hashes, "%" PRIu64 " %016" PRIx64 "\n", frame, s32_state_hash());
	if (video_hashes)
		fprintf(video_hashes, "%" PRIu64 " %016" PRIx64 "\n", frame, s32_video_frame_hash());

	/* the sound board up to the frame's end, then the samples that start before it */
	if (s32_board.time_passes)
		s32_board.time_passes(s32_board.cycles);
	sound_chips_end_frame(s32_board.cycles);
	mix = sound_chips_frame_mix(&count);
#ifdef ORUNNERS_CONSOLE
	if (console_window_settings.volume != 5 && mix)
	{
		/* VOLUME: 0-10, 5 the game's own */
		static int16_t loud[4096];

		if (count * 2 > sizeof(loud) / sizeof(loud[0]))
			count = sizeof(loud) / sizeof(loud[0]) / 2;
		for (size_t index = 0; index < count * 2; index++)
		{
			const int value = mix[index] * console_window_settings.volume / 5;

			loud[index] = (int16_t)(value > 32767 ? 32767 : (value < -32768 ? -32768 : value));
		}
		mix = loud;
	}
#endif
	if (audio && !fast && SDL_GetQueuedAudioSize(audio) < sound_chips_mix_rate() / S32_FRAME_RATE * 4 * QUEUE_FRAMES_MAX)
		SDL_QueueAudio(audio, mix, (Uint32)(count * 2 * sizeof(int16_t)));
	if (sound_hashes)
		fprintf(sound_hashes, "%" PRIu64 " %016" PRIx64 " %016" PRIx64 "\n", frame, sound_chips_frame_hash(),
			sound_board_state_hash());
	if (script && s32_script_frames(script) && program_frames() >= s32_script_frames(script))
		program_stop();

	if (program_frames() % 600 == 0)
	{
		save_eeprom();
		save_backup_ram();
	}
	if (!fast)
		wait_for_frame();
}

static void on_irq_changed(bool asserted)
{
	program_irq_line(asserted);
}

static void on_log(const char *message)
{
	(void)message;
}

/* A window with what went wrong: the port has no console */
static int fail(const char *what, const char *detail)
{
	char message[1200];

	snprintf(message, sizeof(message), "%s\n%s", what, detail);
	fprintf(stderr, "%s\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, TITLE, message, NULL);
	return 1;
}

static FILE *open_hashes(const char *path)
{
	return path ? fopen(path, "w") : NULL;
}

/* Where the game keeps what it writes (the EEPROM, the sound RAM's bookkeeping): beside the executable on
   Windows; elsewhere the user's own folder for the game (SDL_GetPrefPath: ~/.local/share/OutRunners,
   ~/Library/Application Support/OutRunners), as the executable's may not be written (an .app, /usr/bin).
   The caller frees it (SDL_free) */
static char *saves_folder(void)
{
#ifdef _WIN32
	return SDL_GetBasePath();
#else
	return SDL_GetPrefPath("", "OutRunners");
#endif
}

/* On macOS, the folder the .app is in (the game's base is the .app's Resources folder: where the player sees
   the game is beside the .app); 0 elsewhere */
static int app_folder(const char *base, char *folder, size_t size)
{
#ifdef __APPLE__
	static const char inside[] = ".app/Contents/Resources/";
	const size_t length = base ? strlen(base) : 0, tail = sizeof(inside) - 1;
	size_t end;

	if (length < tail || length >= size || strcmp(base + length - tail, inside) != 0)
		return 0;
	for (end = length - tail; end > 0 && base[end - 1] != '/'; end--)
		;    /* (back over the .app's name) */
	memcpy(folder, base, end);
	folder[end] = '\0';
	return end > 0;
#else
	(void)base;
	(void)folder;
	(void)size;
	return 0;
#endif
}

int main(int argc, char **argv)
{
	SDL_AudioSpec wanted;
	const char *script_path = NULL;
	const char *hashes_path = NULL, *video_hashes_path = NULL, *sound_hashes_path = NULL;
	const char *assets_path = NULL;
	char *base, *saves;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
		return fail("SDL could not start.", SDL_GetError());
	for (int index = 1; index < argc; index++)
	{
		if (strcmp(argv[index], "--script") == 0 && index + 1 < argc)
			script_path = argv[++index];
		else if (strcmp(argv[index], "--hashes") == 0 && index + 1 < argc)
			hashes_path = argv[++index];
		else if (strcmp(argv[index], "--video-hashes") == 0 && index + 1 < argc)
			video_hashes_path = argv[++index];
		else if (strcmp(argv[index], "--sound-hashes") == 0 && index + 1 < argc)
			sound_hashes_path = argv[++index];
		else if (strcmp(argv[index], "--fast") == 0)
			fast = true;
		else if (strcmp(argv[index], "--assets") == 0 && index + 1 < argc)
			assets_path = argv[++index];
		else
			return fail("Unknown option.", USAGE);
	}
	if (script_path)
	{
		char error[256];

		script = s32_script_load(script_path, error, sizeof(error));
		if (!script)
			return fail("The control script cannot be read.", error);
	}
	fast = fast && script;
	hashes = open_hashes(hashes_path);
	video_hashes = open_hashes(video_hashes_path);
	sound_hashes = open_hashes(sound_hashes_path);
	if ((hashes_path && !hashes) || (video_hashes_path && !video_hashes) || (sound_hashes_path && !sound_hashes))
		return fail("A hash file cannot be written.", hashes_path && !hashes ? hashes_path
			: (video_hashes_path && !video_hashes ? video_hashes_path : sound_hashes_path));

	base = SDL_GetBasePath();
	saves = saves_folder();
	snprintf(eeprom_path, sizeof(eeprom_path), "%s" FILE_NAME ".eeprom", saves ? saves : "");
	snprintf(backup_ram_path, sizeof(backup_ram_path), "%s" FILE_NAME ".bookkeeping", saves ? saves : "");
#ifdef ORUNNERS_CONSOLE
	snprintf(settings_path, sizeof(settings_path), "%s" FILE_NAME ".ini", saves ? saves : "");
	load_window_settings();
	{
		char controls_path[1024];

		snprintf(controls_path, sizeof(controls_path), "%scontrols.ini", saves ? saves : "");
		controls_open(controls_path);
		console_ghost_folder(saves ? saves : "");
		load_prompts(base ? base : "");    /* (the pictures come with the game, beside it) */
	}
#endif
	{
		/* the folder of the game's assets: --assets, else beside the executable, beside the .app on macOS, in
		   the folder of what the game keeps, or in the current folder */
		char beside_app[1024], folder[1100], layout[1200];
		const char *places[4];
		const char *found = assets_path;
		const char *problem;
		size_t count = 0;

		places[count++] = base ? base : "";
		if (app_folder(base, beside_app, sizeof(beside_app)))
			places[count++] = beside_app;
		if (saves)
			places[count++] = saves;
		places[count++] = "";
		for (size_t place = 0; place < count && !found; place++)
		{
			FILE *probe;

			snprintf(folder, sizeof(folder), "%sassets", places[place]);
			snprintf(layout, sizeof(layout), "%s/data/layout.json", folder);
			if ((probe = fopen(layout, "rb")) != NULL)
			{
				fclose(probe);
				found = folder;
			}
		}
		if (!found)
			return fail("The game's assets are missing.", "No folder 'assets' beside the game.");
		if ((problem = assets_load(found)) != NULL)
			return fail("The game's assets cannot be read.", problem);
	}
	SDL_free(base);
	SDL_free(saves);

	window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		screens_shown() * 320 * WINDOW_SCALE, S32_VIDEO_HEIGHT * WINDOW_SCALE, SDL_WINDOW_RESIZABLE);
#ifdef ORUNNERS_CONSOLE
	if (window)
		apply_window_settings();
#endif
	renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
	if (window && !renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		2 * S32_VIDEO_MAX_WIDTH, 2 * S32_VIDEO_HEIGHT) : NULL;    /* (room for two screens, either way) */
	if (!texture)
		return fail("The window could not be opened.", SDL_GetError());
	SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
	SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

	/* the mix at the YM3438's rate; SDL takes it to the device's */
	SDL_zero(wanted);
	wanted.freq = (int)sound_chips_mix_rate();
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;
	wanted.samples = 1024;
	audio = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
	if (audio)
		SDL_PauseAudioDevice(audio, 0);

	/* the board, the video, the sound board and the game, as the headless runs set them up */
	s32_board.irq_changed = on_irq_changed;
	s32_board.controls_update = on_controls_update;
	s32_board.log = on_log;
	s32_board.frame_timing = true;
	s32_board.program_at = data_at;
	s32_init(NULL);
	s32_video_init_graphics(assets_graphics());
	s32_board.frame_shown = on_frame_shown;
	s32_board.sound_at = assets_sound_data;
	s32_board.pcm_at = assets_pcm_byte;
	sound_chips_attach();
	sound_cpu_attach();
#ifdef ORUNNERS_CONSOLE
	sound_chips_join_sides(true);
	console_attach();
#endif
	if (!script)
	{
		load_eeprom();
		load_backup_ram();
	}
	program_reset();
	program_frame_done = on_frame_done;
	next_frame = SDL_GetPerformanceCounter();

	program_run();

	save_eeprom();
	save_backup_ram();
	if (hashes)
		fclose(hashes);
	if (video_hashes)
		fclose(video_hashes);
	if (sound_hashes)
		fclose(sound_hashes);
	if (audio)
		SDL_CloseAudioDevice(audio);
	SDL_Quit();
	return 0;
}
