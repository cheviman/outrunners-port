/**
 *    Name:        main.c
 *    Description: OutRunners Anbernic: the game for Anbernic's H700 handhelds as the folder PORTS for their
 *                 card, made on the player's computer from the game's Anbernic build and the player's ROM set
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Usage: OutRunners Anbernic [--rom FILE] [--anbernic FOLDER] [--out FOLDER]
 *
 *    PORTS is made in make_ports.c. By default the ROM set is beside the tool, the game's Anbernic build in the
 *    folder anbernic beside it, and PORTS is written beside it. A file dropped on the tool is the ROM set
 *    (orunners.zip, or a folder of its files); started so, its console waits for Enter at the end. Started
 *    with nothing (a double click), it opens its window (window.c, when it is built with SDL2 and the
 *    computer can show one), which shows the same inputs, takes files dropped on it and makes the same PORTS;
 *    else it makes PORTS at once with the defaults, as with files dropped on it. With options it is a console
 *    program and nothing else.
 */
#include "make_ports.h"

#ifdef ORUNNERS_ANBERNIC_WINDOW
#include "window.h"
#endif

#include <stdio.h>
#include <string.h>

static int waits;    /* started without options (a double click, files dropped on it): it waits for Enter */

static int finish(int code)
{
	if (waits)
	{
		printf("\nPress Enter to close.\n");
		(void)getchar();
	}
	return code;
}

int main(int argc, char **argv)
{
	char here[1024];
	ports_inputs inputs;
	ports_outcome outcome;

	memset(&inputs, 0, sizeof(inputs));
	waits = 1;
	ports_folder_of(argv[0], here, sizeof(here));
#ifdef ORUNNERS_ANBERNIC_WINDOW
	if (argc < 2)
	{
		const int code = ports_window(here);

		if (code >= 0)
			return code;
	}
#endif
	for (int at = 1; at < argc; at++)
	{
		char *target = NULL;

		if (strncmp(argv[at], "--", 2) == 0)
			waits = 0;    /* (a command line: the window is someone's console) */
		else
		{
			ports_take_dropped(&inputs, argv[at]);    /* (a file dropped on the tool: the ROM set) */
			continue;
		}
		if (strcmp(argv[at], "--rom") == 0)
			target = inputs.rom;
		else if (strcmp(argv[at], "--anbernic") == 0)
			target = inputs.build;
		else if (strcmp(argv[at], "--out") == 0)
			target = inputs.out;
		if (!target || at + 1 >= argc)
		{
			fprintf(stderr, "Usage: OutRunners Anbernic [--rom FILE] [--anbernic FOLDER] [--out FOLDER]\n"
				"  --rom       the arcade ROM set (orunners.zip, MAME's, World; by default beside it)\n"
				"  --anbernic  the game's Anbernic build: OutRunners.sh and the folder outrunners (by default\n"
				"              anbernic beside it)\n"
				"  --out       the folder PORTS goes in: your card's Roms, or any (by default beside it)\n");
			return 2;
		}
		snprintf(target, PORTS_PATH_SIZE, "%s", argv[++at]);
	}
	printf("OutRunners Anbernic: the game for Anbernic's H700 handhelds, with the assets of your ROM set\n\n");
	ports_defaults(&inputs, here);
	if (!make_ports(&inputs, stdout, NULL, NULL, &outcome))
	{
		fprintf(stderr, "%s%s%s\n", outcome.what, outcome.detail[0] ? ": " : "", outcome.detail);
		return finish(1);
	}
	printf("\nCopy PORTS into Roms on your handheld's card (or merge it with the PORTS there).\n");
	return finish(0);
}
