/**
 *    Name:        console_draw.h
 *    Description: the console's own sprites in the screen's sprite list (draw.c)
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 */
#ifndef ORUNNERS_CONSOLE_DRAW_H
#define ORUNNERS_CONSOLE_DRAW_H

#include <stdint.h>

/* Letters of the game's sprite font on the screen running ('screen'), in its sprite list this frame: from x,
   y (pixels, the sprite's place as the game's text has it), 'scale' times their 8 x 8 pixels, in the
   colour 'colour' (the entry's colour word, over the font's palette) */
void console_text(int x, int y, int scale, uint16_t colour, const char *text);

/* The width 'text' takes at 'scale', in pixels */
int console_text_width(const char *text, int scale);

/* A rectangle that darkens what is under it (a shadow sprite), on the screen running, in the same depth
   layer as the text: the text drawn before it this frame is drawn over it (the layer's newest group comes
   first in the list) */
void console_shade(int x, int y, int width, int height);

#endif
