/**
 *    Name:        main.c
 *    Description: the game on the New Nintendo 3DS: libctru shows its screen, plays its sound, reads the
 *                 buttons and keeps the pace, around the same game and board as every other port
 *    Version:     0.1.0
 *    Created:     03-11-2026
 *    Modified:    03-11-2026
 *    Author:      yoshisuga
 *
 *    license:BSD-3-Clause
 *    The console version in WIDE 16:9 on the top screen, the middle 400 of its 416 pixels, its 224 rows in the
 *    240's middle. The sound board runs on a core of its own, a frame of its time at each frame's end, while the
 *    game goes on with the next: the two speak as the real boards do, through the RAM they share and the
 *    interrupt, only later than the board's time would have it (the other ports keep the two in step, for the
 *    checks; here the game has no time to wait for it). The assets pack and what the game writes are in sdmc:/3ds/outrunners/. The times of its
 *    frames go to stdout, which 3dslink -s shows on the computer.
 *
 *    Controls: the circle pad or the d-pad steers and moves the menus' cursor, A is the gas (the menus' accept),
 *    B the brake (their back), R and L shift up and down, X the radio, START starts, SELECT and START together
 *    leave the game.
 */
#include "s32/machine.h"
#include "s32/program.h"
#include "s32/video.h"
#include "s32/workers.h"
#include "sound/chips.h"
#include "game/assets.h"
#include "console.h"
#include "gpu.h"

#include <3ds.h>

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FOLDER           "sdmc:/3ds/outrunners/"
#define SCREEN_WIDTH     400
#define SCREEN_HEIGHT    240
#define VIDEO_WORKERS    2           /* the video's rows on cores 1 and 3 too, where it has them */
#define SOUND_CORE       2           /* the application's whole: the system's work slows it on core 1 */
#define SOUND_QUEUE      4           /* frames the sound board may be behind the game */
#define SOUND_PRIORITY   0x20
#define SOUND_STACK      0x40000
#define CORE1_LIMIT      80          /* percent of core 1 the application may have */
#define SKIPPED_MOST     3           /* frames in a row not drawn, when the game is behind */
#define WHEEL_LEFT       0x10        /* the wheel's travel, as the other ports have it */
#define WHEEL_CENTRE     0x80
#define WHEEL_RIGHT      0xf0
#define WHEEL_TURN       6           /* a frame, while the d-pad holds it */
#define WHEEL_RETURN     8           /* a frame, back to the centre */
#define PEDAL_PRESS      0x20
#define PEDAL_LIFT       0x30
#define CIRCLE_DEAD      20          /* of the circle pad's 156 */
#define CIRCLE_FULL      150
#define CIRCLE_ACTION    80          /* past this, the menus' arrows */
#define BACKUP_RAM_START 0x1f00      /* what the game keeps of the sound board's RAM */
#define BACKUP_RAM_SIZE  0xfa
#define SOUND_BUFFERS    8
#define SOUND_SAMPLES    2048        /* stereo samples a buffer, at most */
#define TICKS_FRAME      (SYSCLOCK_ARM11 / 60)
#define TICKS_BEHIND     (SYSCLOCK_ARM11 / 4)
#define SAVE_FRAMES      3600        /* a minute: the SD card takes seconds */
#define SAVE_PRIORITY    0x38
#define SAVE_STACK       0x8000

u32 __stacksize__ = 2 * 1024 * 1024;

static const char eeprom_path[] = FOLDER "outrunners.eeprom";
static const char backup_ram_path[] = FOLDER "outrunners.bookkeeping";
static uint8_t backup_ram_saved[BACKUP_RAM_SIZE];

static int wheel = WHEEL_CENTRE, gas, brake;
static uint32_t actions_held;
static u64 next_frame;

static ndspWaveBuf sound_buffers[SOUND_BUFFERS];
static int16_t *sound_memory;
static bool sound_on;

static int skipped;
static void (*board_time_passes)(uint64_t until);
static void (*board_sound_reset)(bool held);
static LightSemaphore sound_frames, sound_room;    /* frames waiting for the sound board, and room for more */
static uint64_t sound_queue[SOUND_QUEUE];
static int sound_next, sound_last;
static volatile bool sound_quit;

/* ---- files ---- */

static void save_eeprom(void)
{
	FILE *output;

	if (!s32_board.eeprom.dirty || (output = fopen(eeprom_path, "wb")) == NULL)
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

static void save_backup_ram(void)
{
	const uint8_t *kept = &s32_board.shared_ram[BACKUP_RAM_START];
	FILE *output;

	if (memcmp(kept, backup_ram_saved, BACKUP_RAM_SIZE) == 0 || (output = fopen(backup_ram_path, "wb")) == NULL)
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

/* What the game keeps, taken at a frame's end and written while it goes on: the SD card takes seconds */
static uint16_t eeprom_kept[S32_EEPROM_WORDS];
static uint8_t backup_ram_kept[BACKUP_RAM_SIZE];
static volatile bool eeprom_waiting, backup_ram_waiting;
static LightEvent save_go;

static void save_thread(void *unused)
{
	(void)unused;
	for (;;)
	{
		FILE *output;

		LightEvent_Wait(&save_go);
		if (eeprom_waiting && (output = fopen(eeprom_path, "wb")) != NULL)
		{
			fwrite(eeprom_kept, 1, sizeof(eeprom_kept), output);
			fclose(output);
		}
		if (backup_ram_waiting && (output = fopen(backup_ram_path, "wb")) != NULL)
		{
			fwrite(backup_ram_kept, 1, sizeof(backup_ram_kept), output);
			fclose(output);
		}
		eeprom_waiting = backup_ram_waiting = false;
	}
}

static void save_in_the_background(void)
{
	const uint8_t *kept = &s32_board.shared_ram[BACKUP_RAM_START];

	if (eeprom_waiting || backup_ram_waiting)
		return;
	if (s32_board.eeprom.dirty)
	{
		memcpy(eeprom_kept, s32_board.eeprom.data, sizeof(eeprom_kept));
		s32_board.eeprom.dirty = false;
		eeprom_waiting = true;
	}
	if (memcmp(kept, backup_ram_saved, BACKUP_RAM_SIZE) != 0)
	{
		memcpy(backup_ram_kept, kept, BACKUP_RAM_SIZE);
		memcpy(backup_ram_saved, kept, BACKUP_RAM_SIZE);
		backup_ram_waiting = true;
	}
	if (eeprom_waiting || backup_ram_waiting)
		LightEvent_Signal(&save_go);
}

/* ---- the picture ---- */

static void on_frame_shown(void)
{
	static s32_scene scene;

	/* (the game's time comes first: behind it, frames go undrawn) */
	if (svcGetSystemTick() > next_frame + TICKS_FRAME && skipped < SKIPPED_MOST)
	{
		skipped++;
		return;
	}
	skipped = 0;
	s32_video_scene(&scene);
	gpu_draw(&scene);
}

/* ---- the controls ---- */

static int steer(int position, bool left, bool right)
{
	if (left == right)
	{
		if (position < WHEEL_CENTRE)
			position = position + WHEEL_RETURN > WHEEL_CENTRE ? WHEEL_CENTRE : position + WHEEL_RETURN;
		else if (position > WHEEL_CENTRE)
			position = position - WHEEL_RETURN < WHEEL_CENTRE ? WHEEL_CENTRE : position - WHEEL_RETURN;
		return position;
	}
	position += left ? -WHEEL_TURN : WHEEL_TURN;
	return position < WHEEL_LEFT ? WHEEL_LEFT : (position > WHEEL_RIGHT ? WHEEL_RIGHT : position);
}

static int press(int position, bool held)
{
	position += held ? PEDAL_PRESS : -PEDAL_LIFT;
	return position < 0 ? 0 : (position > 0xff ? 0xff : position);
}

static void on_controls_update(void)
{
	s32_inputs *input = &s32_board.input;
	circlePosition circle;
	u32 keys;
	uint32_t held = 0;
	int wheels;
	bool start;

	if (!aptMainLoop())
	{
		program_stop();
		return;
	}
	hidScanInput();
	keys = hidKeysHeld();
	hidCircleRead(&circle);
	if ((keys & KEY_SELECT) && (keys & KEY_START))
	{
		program_stop();
		return;
	}
	if ((keys & KEY_DUP) || circle.dy > CIRCLE_ACTION)
		held |= ACTION_BIT(ACTION_UP);
	if ((keys & KEY_DDOWN) || circle.dy < -CIRCLE_ACTION)
		held |= ACTION_BIT(ACTION_DOWN);
	if ((keys & KEY_DLEFT) || circle.dx < -CIRCLE_ACTION)
		held |= ACTION_BIT(ACTION_LEFT);
	if ((keys & KEY_DRIGHT) || circle.dx > CIRCLE_ACTION)
		held |= ACTION_BIT(ACTION_RIGHT);
	if (keys & KEY_A)
		held |= ACTION_BIT(ACTION_ACCEPT);
	if (keys & KEY_B)
		held |= ACTION_BIT(ACTION_BACK);
	if (keys & KEY_START)
		held |= ACTION_BIT(ACTION_START);
	s32_video_set_wide_side(console_wide_side());    /* (before VBLANK's sprites and the frame) */
	console_actions(held & ~actions_held);
	actions_held = held;
	if (console_quit_requested())
	{
		program_stop();
		return;
	}
	wheel = steer(wheel, (keys & KEY_DLEFT) != 0, (keys & KEY_DRIGHT) != 0);
	wheels = wheel;
	if (abs(circle.dx) > CIRCLE_DEAD)
	{
		const int reach = circle.dx > 0 ? WHEEL_RIGHT - WHEEL_CENTRE : WHEEL_CENTRE - WHEEL_LEFT;
		const int travel = abs(circle.dx) > CIRCLE_FULL ? CIRCLE_FULL : abs(circle.dx);

		wheels = WHEEL_CENTRE + (circle.dx > 0 ? 1 : -1) * (travel - CIRCLE_DEAD) * reach / (CIRCLE_FULL - CIRCLE_DEAD);
	}
	gas = press(gas, (keys & KEY_A) != 0);
	brake = press(brake, (keys & KEY_B) != 0);
	start = (keys & KEY_START) != 0 && !console_can_pause();

	/* the first player's 315-5296, active low; the second's let alone */
	input->port_a[0] = (uint8_t)(0xff & ~((keys & KEY_R) ? 0x01 : 0) & ~((keys & KEY_L) ? 0x02 : 0));
	input->port_b[0] = (uint8_t)(0xff & ~((keys & KEY_X) ? 0x01 : 0));
	input->port_e[0] = (uint8_t)(0xff & ~(start ? 0x10 : 0));
	input->port_a[1] = input->port_b[1] = input->port_e[1] = 0xff;
	input->analog[0] = (uint8_t)wheels;
	input->analog[1] = (uint8_t)gas;
	input->analog[2] = (uint8_t)brake;
	input->analog[3] = WHEEL_CENTRE;
	input->analog[6] = 0;
	input->analog[7] = 0;
}

/* ---- the sound ---- */

static void open_sound(void)
{
	float mix[12] = { 1.0f, 1.0f };

	if (R_FAILED(ndspInit()))
	{
		printf("no sound: sdmc:/3ds/dspfirm.cdc?\n");
		return;
	}
	sound_memory = linearAlloc(SOUND_BUFFERS * SOUND_SAMPLES * 2 * sizeof(int16_t));
	if (!sound_memory)
		return;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(0);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, (float)sound_chips_mix_rate());
	ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
	ndspChnSetMix(0, mix);
	memset(sound_buffers, 0, sizeof(sound_buffers));
	for (int index = 0; index < SOUND_BUFFERS; index++)
	{
		sound_buffers[index].data_pcm16 = sound_memory + index * SOUND_SAMPLES * 2;
		sound_buffers[index].status = NDSP_WBUF_DONE;
	}
	sound_on = true;
}

static void queue_sound(const int16_t *mix, size_t count)
{
	if (!sound_on || !mix)
		return;
	if (count > SOUND_SAMPLES)
		count = SOUND_SAMPLES;
	for (int index = 0; index < SOUND_BUFFERS; index++)
	{
		ndspWaveBuf *buffer = &sound_buffers[index];

		if (buffer->status != NDSP_WBUF_DONE)
			continue;
		memcpy(buffer->data_pcm16, mix, count * 2 * sizeof(int16_t));
		DSP_FlushDataCache(buffer->data_pcm16, count * 2 * sizeof(int16_t));
		buffer->nsamples = count;
		ndspChnWaveBufAdd(0, buffer);
		return;
	}
}

/* ---- the frame ---- */

/* The board's time passing asks nothing of the sound board: sound_thread has it run at the frame's end */
static void on_time_passes(uint64_t until)
{
	(void)until;
}

static void sound_thread(void *unused)
{
	(void)unused;
	for (;;)
	{
		const int16_t *mix;
		size_t count = 0;
		u64 start;

		uint64_t until;

		LightSemaphore_Acquire(&sound_frames, 1);
		if (sound_quit)
			break;
		until = sound_queue[sound_next];
		sound_next = (sound_next + 1) % SOUND_QUEUE;
		board_time_passes(until);
		sound_chips_end_frame(until);
		mix = sound_chips_frame_mix(&count);
		queue_sound(mix, count);
		LightSemaphore_Release(&sound_room, 1);
	}
}

/* The Z80 reset by the game: never under the sound thread's feet */
static void on_sound_reset(bool held)
{
	for (int frame = 0; frame < SOUND_QUEUE; frame++)
		LightSemaphore_Acquire(&sound_room, 1);
	board_sound_reset(held);
	LightSemaphore_Release(&sound_room, SOUND_QUEUE);
}

static void on_frame_done(void)
{
	u64 now;

	/* (this frame's sound queued for its core, which works on through the frames the video takes long over) */
	LightSemaphore_Acquire(&sound_room, 1);
	sound_queue[sound_last] = s32_board.cycles;
	sound_last = (sound_last + 1) % SOUND_QUEUE;
	LightSemaphore_Release(&sound_frames, 1);
	if (program_frames() % SAVE_FRAMES == 0)
		save_in_the_background();
	console_network_frame();
	next_frame += TICKS_FRAME;
	now = svcGetSystemTick();
	if (now > next_frame + TICKS_BEHIND)
		next_frame = now;
	while (now < next_frame)
	{
		svcSleepThread((s64)((next_frame - now) * (1000000000.0 / SYSCLOCK_ARM11)));
		now = svcGetSystemTick();
	}
}

static void on_irq_changed(bool asserted)
{
	program_irq_line(asserted);
}

static void on_log(const char *message)
{
	(void)message;
}

/* What went wrong, on the bottom screen until START */
static int fail(const char *what, const char *detail)
{
	printf("%s\n%s\n\nSTART leaves\n", what, detail ? detail : "");
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		hidScanInput();
		if (hidKeysDown() & KEY_START)
			break;
	}
	return 1;
}

int main(int argc, char **argv)
{
	const char *problem;

	(void)argc;
	(void)argv;
	osSetSpeedupEnable(true);
	gfxInit(GSP_RGBA8_OES, GSP_BGR8_OES, false);
	consoleInit(GFX_BOTTOM, NULL);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	gpu_open();
	mkdir("sdmc:/3ds", 0777);
	mkdir(FOLDER, 0777);
	console_window_settings.full_screen = true;
	console_window_settings.wide = WIDE_16_9;
	console_window_settings.pixels = PIXELS_SHARP;
	console_window_settings.vertical = false;
	console_ghost_folder(FOLDER);
	/* (in the game's folder, or where 3dslink leaves what it is sent) */
	problem = assets_load_pack(FOLDER "outrunners.assets");
	if (problem)
		problem = assets_load_pack("sdmc:/3ds/outrunners.assets");
	if (problem)
		return fail("The game's assets cannot be read:", problem);
	APT_SetAppCpuTimeLimit(CORE1_LIMIT);
	s32_workers_start(VIDEO_WORKERS);
	open_sound();

	s32_board.irq_changed = on_irq_changed;
	s32_board.controls_update = on_controls_update;
	s32_board.log = on_log;
	s32_board.frame_timing = true;
	s32_board.program_at = data_at;
	s32_init(NULL);
	s32_video_init_graphics(assets_graphics());
	s32_board.frame_shown = on_frame_shown;
	s32_video_use_scene();
	s32_video_show_screens(1);    /* (the cabinet's second monitor has sprites of its own: the attract's, while we race) */
	s32_board.sound_at = assets_sound_data;
	s32_board.pcm_at = assets_pcm_byte;
	sound_chips_attach();
	sound_cpu_attach();
	board_time_passes = s32_board.time_passes;
	s32_board.time_passes = on_time_passes;
	board_sound_reset = s32_board.sound_reset_changed;
	s32_board.sound_reset_changed = on_sound_reset;
	LightSemaphore_Init(&sound_frames, 0, SOUND_QUEUE + 1);
	LightSemaphore_Init(&sound_room, SOUND_QUEUE, SOUND_QUEUE);
	LightEvent_Init(&save_go, RESET_ONESHOT);
	if (!threadCreate(save_thread, NULL, SAVE_STACK, SAVE_PRIORITY, 0, true))
		return fail("The saves' thread could not start.", NULL);
	if (!threadCreate(sound_thread, NULL, SOUND_STACK, SOUND_PRIORITY, SOUND_CORE, true))
		return fail("The sound's thread could not start.", NULL);
	sound_chips_join_sides(true);
	console_attach();
	load_eeprom();
	load_backup_ram();
	program_reset();
	program_frame_done = on_frame_done;
	next_frame = svcGetSystemTick();

	program_run();

	for (int frame = 0; frame < SOUND_QUEUE; frame++)
		LightSemaphore_Acquire(&sound_room, 1);
	sound_quit = true;
	LightSemaphore_Release(&sound_frames, 1);
	save_eeprom();
	save_backup_ram();
	if (sound_on)
		ndspExit();
	gpu_close();
	gfxExit();
	return 0;
}
