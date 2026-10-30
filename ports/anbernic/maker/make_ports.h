/**
 *    Name:        make_ports.h
 *    Description: OutRunners Anbernic's work, for its command line and its window alike: the folder PORTS for
 *                 the handheld's card from its inputs, their defaults, and what the window shows of each
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Paths are as the C library's fopen takes them (on Windows, in the system's code page).
 */
#ifndef ORUNNERS_ANBERNIC_MAKE_PORTS_H
#define ORUNNERS_ANBERNIC_MAKE_PORTS_H

#include <stddef.h>
#include <stdio.h>

#define PORTS_PATH_SIZE 1100
#define PORTS_FOLDER    "PORTS"

/* What PORTS is made of, and where it goes */
typedef struct
{
	char rom[PORTS_PATH_SIZE];      /* the arcade ROM set: orunners.zip, or a folder of its files */
	char build[PORTS_PATH_SIZE];    /* the game's Anbernic build: OutRunners.sh, outrunners/OutRunners and
	                                   outrunners/input_prompts.bmp */
	char out[PORTS_PATH_SIZE];      /* the folder PORTS goes in, with its separator: the card's Roms, or any */
} ports_inputs;

/* How making it went */
typedef struct
{
	char what[80];                  /* what failed */
	char detail[PORTS_PATH_SIZE + 128];    /* what it failed on (a file, or what the part that failed tells), or "" */
	size_t size;                    /* the assets pack's bytes, when PORTS is written */
} ports_outcome;

/* Told the step making PORTS is at, a name of its own ("taking the assets out of the ROM set") */
typedef void (*ports_step)(void *context, const char *step);

/* A file dropped on the tool: the ROM set (orunners.zip, or a folder of its files) */
void ports_take_dropped(ports_inputs *inputs, const char *path);

/* The inputs not given, beside the tool (in the folder 'here', with its separator): orunners.zip, the folder
   anbernic, and PORTS beside it */
void ports_defaults(ports_inputs *inputs, const char *here);

/* PORTS in the folder 'out': true when it is written. Each step's lines go to 'log' (the command line's)
   unless it is NULL; 'step' (or NULL) is told each step as it starts. What failed is in 'outcome' */
int make_ports(const ports_inputs *inputs, FILE *log, ports_step step, void *context, ports_outcome *outcome);

/* The folder of a path, with its separator: "" for a bare name */
void ports_folder_of(const char *path, char *folder, size_t size);

/* What the window shows of each input */
int ports_file_exists(const char *path);
int ports_folder_exists(const char *path);
const char *ports_build_problem(const char *build);    /* NULL when the game's build is whole, else what is not */

#endif
