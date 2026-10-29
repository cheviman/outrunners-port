/**
 *    Name:        console.h
 *    Description: what the console build tells the port
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 */
#ifndef ORUNNERS_CONSOLE_H
#define ORUNNERS_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

/* Sets the console's part of the game up, before the program starts: the code of its own objects, which the
   program reaches through call_code (call_code_elsewhere) */
void console_attach(void);

/* True while the right seat's screen is shown beside the left one's (2 PLAYERS); the port shows the left
   screen alone otherwise */
bool console_both_screens(void);

/* The menus' actions, from any player's keys and game controller: the port hands those newly pressed each of
   its frames, and the menus take them on the left seat's next frame (the game's screens run at 30 a second) */
enum
{
	ACTION_UP,
	ACTION_DOWN,
	ACTION_LEFT,
	ACTION_RIGHT,
	ACTION_ACCEPT,
	ACTION_BACK,
	ACTION_START,
	ACTIONS
};
#define ACTION_BIT(action) (1u << (action))

void console_actions(uint32_t pressed);

/* For scripted checks (orunners_console_headless --autopilot): the left seat's wheel that keeps its car near
   the middle of the road */
int console_test_autopilot(void);

/* True once the menu's EXIT was picked: the port closes the game */
bool console_quit_requested(void);

/* The cabinet's controls each seat has, which OPTIONS > CONTROLS binds to keys and buttons (the port keeps
   them, controls.c) */
enum
{
	CONTROL_STEER_LEFT,
	CONTROL_STEER_RIGHT,
	CONTROL_GAS,
	CONTROL_BRAKE,
	CONTROL_SHIFT_UP,
	CONTROL_SHIFT_DOWN,
	CONTROL_MUSIC,         /* the DJ: the radio's song select */
	CONTROL_SONG_BACK,     /* << */
	CONTROL_SONG_NEXT,     /* >> */
	CONTROL_START,
	CONTROLS
};
#define CONTROL_PLAYERS 2

/* OPTIONS > CONTROLS on screen: where, in the left screen's pixels, the pictures of 'control''s keys and
   button go (their top left; false when the page is not shown): the first key's there, the second's and the
   game controller button's that many pixels to the right */
bool console_controls_place(int control, int *x, int *y);
#define CONSOLE_CONTROLS_SECOND_X   36     /* (room for a key two tiles wide, Enter or Space) */
#define CONSOLE_CONTROLS_BUTTON_X   76

/* The player whose controls CONTROLS shows (0, 1) */
int console_controls_player(void);

/* The control CONTROLS waits for a new key or button for, or -1: the port takes the next one pressed, binds
   it (Escape: leaves it as it was) and says so */
int console_controls_waiting(void);
void console_controls_bound(void);

/* DEFAULTS was picked on CONTROLS (true once): the port puts the shown player's keys and buttons back */
bool console_controls_defaults(void);

/* The window's settings OPTIONS > VIDEO and VOLUME show and change: the port fills them in, and applies and
   saves them when 'changed' is set */
enum
{
	PIXELS_SHARP,       /* the board's pixels, square */
	PIXELS_SMOOTH,      /* sharp bilinear: square pixels with soft edges */
	PIXELS_BILINEAR,    /* plain bilinear, softer */
	PIXELS_CRT,         /* scanlines, a phosphor mask and a little vignette over smooth pixels */
	PIXELS_KINDS
};

typedef struct
{
	int window_scale;      /* 1-6: the window, in times the game's picture */
	bool full_screen;
	int pixels;            /* PIXELS_* */
	bool wide;             /* a 16:9 picture */
	bool vertical;         /* 2 PLAYERS: one screen over the other (false: side by side) */
	int volume;            /* 0-10: 5 is the game's own */
	bool changed;
} console_window;

extern console_window console_window_settings;

/* WIDE draws this frame wider (s32_video_set_wide): a race is run on the left seat's screen (a game's or the
   attract mode's demo); the other screens' pictures end at their 320 pixels, so they keep them, between
   black bars */
bool console_wide_now(void);

/* SEGA's jingle: the program of its chords (the sound program's, 0-FF), of the layer an octave under them (-1:
   none) and the semitones both go up, in place of jingle.c's own (orunners_console_headless --jingle, to try
   others by ear) */
void console_jingle_program(int program, int layer, int transpose);

/* The folder TIME ATTACK keeps its ghosts in (ghost_0.bin to ghost_9.bin, a goal each), with its separator */
void console_ghost_folder(const char *folder);

#endif
