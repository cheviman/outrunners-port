/**
 *    Name:        jingle.c
 *    Description: SEGA's jingle on the left seat's sound: SegaSonic's two chords, E flat major then C major,
 *                 through the sound program's own MIDI queue
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The sound program plays screen 1's MIDI queue (the ring at E100, the next free byte at E758, the
 *    messages waiting at E759; 0x700100, 0x700758 and 0x700759 from the V70) on its 13 parts, which all play
 *    from power-on: the sequence players append messages as this does (the bytes, the write index, then the
 *    count), and the sound program takes one a pass of its main loop. The jingle uses channels B and C (parts
 *    9 and 10, the effects', whose volume the music's fades leave alone), B for the chords and C an octave
 *    under them, as SegaSonic's; their programs (an FM one on C that would be silent, a sample of another
 *    page on B) become program 15, the chords sample of Splash Wave in the fixed part of the sample ROM,
 *    and are put back once their notes are off (a program change first would leave the notes hanging:
 *    release_note_screen_1 goes by the part's program).
 */
#include "console_internal.h"

#include "game.h"

#define MIDI_QUEUE          0x700100u    /* screen 1's ring of 256 bytes (E100) */
#define MIDI_WRITE          0x700758u    /* its next free byte (E758) */
#define MIDI_COUNT          0x700759u    /* the messages waiting (E759) */
#define MIDI_COUNT_LIMIT    0x40u        /* the sequence players wait from there: so does the jingle */
#define CHANNEL_CHORDS      0x0Bu
#define CHANNEL_LAYER       0x0Cu        /* an octave under */
#define PART_PROGRAM_B      0x70035Du    /* part 9's program (channel B) */
#define PART_PROGRAM_C      0x700367u    /* part 10's (channel C) */
#define JINGLE_PROGRAM      0x15u        /* Splash Wave's chords: in every sound set */
#define CHORD_NOTES         3
#define AT_SECOND_CHORD     18           /* frames of the left seat's, 30 a second: 0.6 seconds */
#define AT_END              54           /* 1.8 seconds */

static const uint8_t first_chord[CHORD_NOTES] = { 63, 67, 75 };     /* E flat major */
static const uint8_t second_chord[CHORD_NOTES] = { 60, 64, 72 };    /* C major */

static struct
{
	int frame;              /* -1: not playing */
	uint8_t programs[2];    /* B's and C's before it */
	int program;            /* the chords' (console_jingle_program) */
	int layer;              /* the layer's an octave under, or -1: none */
	int transpose;          /* semitones, on both */
} jingle = { -1, { 0, 0 }, JINGLE_PROGRAM, -1, 12 };

void console_jingle_program(int program, int layer, int transpose)
{
	jingle.program = program & 0xFF;
	jingle.layer = layer < 0 ? -1 : layer & 0xFF;
	jingle.transpose = transpose;
}

/* One message into the queue (one to three bytes), as the sequence players put theirs */
static void midi_message(uint8_t status, uint8_t first, uint8_t second, int length)
{
	const uint8_t bytes[3] = { status, first, second };
	uint8_t write = mem_read8(MIDI_WRITE);

	if (mem_read8(MIDI_COUNT) >= MIDI_COUNT_LIMIT)
		return;
	for (int index = 0; index < length; index++)
		mem_write8(MIDI_QUEUE | write++, bytes[index]);
	mem_write8(MIDI_WRITE, write);
	mem_write8(MIDI_COUNT, (uint8_t)(mem_read8(MIDI_COUNT) + 1u));
}

static void chord_on(const uint8_t *notes)
{
	for (int note = 0; note < CHORD_NOTES; note++)
	{
		midi_message(0x90u | CHANNEL_CHORDS, (uint8_t)(notes[note] + jingle.transpose), 0x7Fu, 3);
		if (jingle.layer >= 0)
			midi_message(0x90u | CHANNEL_LAYER, (uint8_t)(notes[note] + jingle.transpose - 12), 0x7Fu, 3);
	}
}

static void chord_off(const uint8_t *notes)
{
	for (int note = 0; note < CHORD_NOTES; note++)
	{
		midi_message(0x80u | CHANNEL_CHORDS, (uint8_t)(notes[note] + jingle.transpose), 0u, 3);
		if (jingle.layer >= 0)
			midi_message(0x80u | CHANNEL_LAYER, (uint8_t)(notes[note] + jingle.transpose - 12), 0u, 3);
	}
}

void console_jingle_start(void)
{
	jingle.programs[0] = mem_read8(PART_PROGRAM_B);
	jingle.programs[1] = mem_read8(PART_PROGRAM_C);
	midi_message(0xB0u | CHANNEL_CHORDS, 0x07u, 0x7Fu, 3);    /* (its volume whole) */
	midi_message(0xB0u | CHANNEL_LAYER, 0x07u, 0x7Fu, 3);
	midi_message(0xC0u | CHANNEL_CHORDS, (uint8_t)jingle.program, 0u, 2);
	if (jingle.layer >= 0)
		midi_message(0xC0u | CHANNEL_LAYER, (uint8_t)jingle.layer, 0u, 2);
	chord_on(first_chord);
	jingle.frame = 0;
}

/* The notes off and the channels' own programs back */
void console_jingle_stop(void)
{
	if (jingle.frame < 0)
		return;
	chord_off(jingle.frame < AT_SECOND_CHORD ? first_chord : second_chord);
	midi_message(0xC0u | CHANNEL_CHORDS, jingle.programs[0], 0u, 2);
	if (jingle.layer >= 0)
		midi_message(0xC0u | CHANNEL_LAYER, jingle.programs[1], 0u, 2);
	jingle.frame = -1;
}

void console_jingle_frame(void)
{
	if (jingle.frame < 0)
		return;
	if (jingle.frame == AT_SECOND_CHORD)
	{
		chord_off(first_chord);
		chord_on(second_chord);
	}
	if (++jingle.frame > AT_END)
		console_jingle_stop();
}
