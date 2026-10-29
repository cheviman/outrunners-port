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

#endif
