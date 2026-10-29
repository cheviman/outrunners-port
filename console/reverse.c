/**
 *    Name:        reverse.c
 *    Description: & KNUCKLES > REVERSE: the route from a goal the player picks back to the pre-stage, each
 *                 fork leading a level back, the race finishing at the start line
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The game builds its road from a course (stage_courses: a stage's runs, then its fork's two branches,
 *    each a short branch road that jumps to the next stage's course) and starts the roadside objects from a
 *    stream a stage (road_object_streams), both read through data_at. REVERSE gives it courses of its own,
 *    at addresses the program has no data at (data_elsewhere): each stage's runs as they are, then a fork
 *    whose two branches lead to the stage's parents (the stages whose forks lead to it: fork_next_stages),
 *    through copies of the game's branch roads; the goal stages fork where their goal was; the pre-stage
 *    finishes where its fork was, with a long run after it, as the goals do. The game's own course
 *    builder then takes the route (the fork's stages into route_stage, its STAGE label, palettes, arrow),
 *    and the roadside objects follow each stage's own stream. A goal stage, where REVERSE starts, has a
 *    stream of its own here: the pre-stage's start line among its objects before its goal's approach, then
 *    stage 1's fork objects (its camera, sign, flags, flagman, checkpoint gate) moved to the new fork, and
 *    stage 1's branch streams moved with them. The pre-stage, where it finishes, has its finish where its
 *    fork was (the track's choice), with a goal stage's finish objects in place of the fork's. The background of each stage is its own (stage_backgrounds), the pre-stage keeps the
 *    one before it (it has none); there is no route map after the race and no course record (the game's
 *    are by goal).
 */
#include "console_internal.h"

#include "game.h"

#define CONSOLE_DATA        0x900000u    /* REVERSE's courses and streams, for data_at (data_elsewhere) */
#define CONSOLE_DATA_SIZE   0x8000u
#define STAGES              0x1F
#define FIRST_GOAL          0x15
#define GOALS               10
#define GOAL_FORK           1792         /* a goal stage's goal, where REVERSE forks */
#define STAGE_FORK          1664         /* the other stages' fork */
#define FORK_OBJECTS_FROM   1408         /* stage 1's stream: its fork camera on */
#define GOAL_APPROACH       1700         /* a goal stage's stream: its goal's approach and finish from here */
#define PRE_STAGE_FORK      710          /* the pre-stage's fork: REVERSE's finish */
#define FORK_OBJECTS_FROM_0 454          /* the pre-stage's stream: its fork camera on */
#define START_OBJECTS_TO    84           /* the pre-stage's stream: its start line (gate, flagman, balloons) */
#define LONG_RUN            16384        /* past a finish, as the goal stages' courses go on */
#define SHARED_RECORDS      0x02D900u    /* the stream records the stages share (fork, checkpoint) from here */
#define START_LINE_PASSED   200          /* the race's segment past the start line: the stage's own colours */

enum
{
	COURSE_END = 0,
	COURSE_JUMP = 1,
	COURSE_FORK = 2,
	COURSE_PITCH_0 = 3,
	COURSE_STAGE = 4,
	COURSE_HEADING_0 = 5,
	COURSE_GOAL = 6
};

enum
{
	STREAM_OBJECT = 0,
	STREAM_STAGE_END = 1,
	STREAM_FORK = 3
};

static struct
{
	uint8_t data[CONSOLE_DATA_SIZE];
	uint32_t used;
	bool built;
	uint32_t courses[STAGES];          /* each stage's REVERSE course */
	uint32_t goal_streams[GOALS];      /* each goal stage's stream */
	uint32_t finish_stream;            /* the pre-stage's */
	uint8_t parents[STAGES][2];        /* by branch (the same twice for one parent) */
	int goal;                          /* the picked goal's stage, or 0 */
	bool running;                      /* the game REVERSE started is on */
	int start_colours;                 /* the start line's colours: 0 to come, 1 the pre-stage's, 2 the stage's back */
} reverse;

/* ---- the console's data, at CONSOLE_DATA ---- */

static const uint8_t *console_data(uint32_t address)
{
	const uint32_t offset = address - CONSOLE_DATA;

	return address >= CONSOLE_DATA && offset < reverse.used ? &reverse.data[offset] : NULL;
}

static uint32_t here(void)
{
	return CONSOLE_DATA + reverse.used;
}

static void put8(uint32_t value)
{
	if (reverse.used < CONSOLE_DATA_SIZE)
		reverse.data[reverse.used++] = (uint8_t)value;
}

static void put16(uint32_t value)
{
	put8(value);
	put8(value >> 8);
}

static void put32(uint32_t value)
{
	put16(value);
	put16(value >> 16);
}

static void patch32(uint32_t address, uint32_t value)
{
	const uint32_t offset = address - CONSOLE_DATA;

	for (int byte = 0; byte < 4; byte++)
		reverse.data[offset + (uint32_t)byte] = (uint8_t)(value >> (8 * byte));
}

static uint8_t byte_at(uint32_t address)
{
	return data_at(address)[0];
}

static int16_t s16_at(uint32_t address)
{
	return (int16_t)(byte_at(address) | byte_at(address + 1u) << 8);
}

static uint32_t u32_at(uint32_t address)
{
	return byte_at(address) | (uint32_t)byte_at(address + 1u) << 8 | (uint32_t)byte_at(address + 2u) << 16
		| (uint32_t)byte_at(address + 3u) << 24;
}

static void copy(uint32_t from, uint32_t count)
{
	for (uint32_t index = 0; index < count; index++)
		put8(byte_at(from + index));
}

/* ---- the courses ---- */

/* The size of the course record at 'place' (its command or a run), 0 for the end, a fork, a goal, a jump */
static uint32_t record_size(uint32_t place)
{
	const int16_t length = s16_at(place);

	if (length > 0)
		return 6;    /* a run: its length, pitch and heading steps, width code, road event */
	switch (-length)
	{
		case COURSE_STAGE: return 4;
		case COURSE_PITCH_0:
		case COURSE_HEADING_0: return 6;
		default: return 0;
	}
}

/* A stage's course up to its fork or goal (not those): copied as it is; the place of the record it stops
   at in 'end' */
static void copy_course(uint32_t course, uint32_t *end)
{
	uint32_t size;

	while ((size = record_size(course)) != 0)
	{
		copy(course, size);
		course += size;
	}
	*end = course;
}

/* The pre-stage's course for REVERSE: its runs, then its finish where its fork was (the track's choice), and
   a long run past it to the end, as a goal's */
static void copy_finish_course(uint32_t course)
{
	uint32_t end;

	copy_course(course, &end);
	put16((uint32_t)(uint16_t)-COURSE_GOAL);
	put16(LONG_RUN);
	put32(0x00050000u);    /* (straight, flat, width code 5, no event) */
	put16(COURSE_END);
}

/* A branch road (one of stage 1's fork's: they are all alike, branch 1 the mirror of branch 0) that leads
   to 'course' */
static uint32_t branch_road(int branch, uint32_t course)
{
	uint32_t fork = stage_courses[1], road;
	const uint32_t start = here();
	uint32_t size;

	while ((size = record_size(fork)) != 0)
		fork += size;    /* (to stage 1's fork) */
	road = u32_at(fork + 2u + 2u + 4u * (uint32_t)branch);
	while ((size = record_size(road)) != 0)
	{
		copy(road, size);
		road += size;
	}
	put16((uint32_t)(uint16_t)-COURSE_JUMP);
	put32(course);
	return start;
}

/* ---- the streams ---- */

/* Stage 1's stream, from its first record: the place of its fork command */
static uint32_t stage_1_fork(void)
{
	uint32_t entry = road_object_streams[1];

	while (s16_at(entry + 2u) == STREAM_OBJECT)
		entry += 12u;
	return entry;
}

/* One of stage 1's branch streams (fork signs, the branch not taken), its places moved by 'shift', to its
   end */
static uint32_t branch_stream(int branch, int shift)
{
	const uint32_t start = here();
	uint32_t entry = u32_at(stage_1_fork() + 4u + 4u * (uint32_t)branch);

	for (;;)
	{
		const uint16_t command = (uint16_t)s16_at(entry + 2u);

		put16((uint32_t)(s16_at(entry) + shift));
		put16(command);
		if (command != STREAM_OBJECT)
			break;
		copy(entry + 4u, 8);
		entry += 12u;
	}
	return start;
}

/* A goal stage's stream for REVERSE: the pre-stage's start line (its gate and lights, flagman, balloons and
   crowd) among its own objects before its goal's approach, stage 1's fork objects moved to its fork, and the
   fork with stage 1's branch streams moved alike */
static uint32_t goal_stream(int stage, uint32_t branches[2])
{
	const int shift = GOAL_FORK - STAGE_FORK;
	const uint32_t start = here();
	uint32_t entry = road_object_streams[stage];
	uint32_t start_line = road_object_streams[0];

	while (s16_at(entry + 2u) == STREAM_OBJECT && s16_at(entry) < GOAL_APPROACH)
	{
		/* (the two streams merged by place: the reader takes them in order) */
		while (s16_at(start_line) < START_OBJECTS_TO && s16_at(start_line) <= s16_at(entry))
		{
			copy(start_line, 12);
			start_line += 12u;
		}
		copy(entry, 12);
		entry += 12u;
	}
	for (entry = road_object_streams[1]; s16_at(entry + 2u) == STREAM_OBJECT; entry += 12u)
	{
		const uint32_t parameter = u32_at(entry + 8u);

		if (s16_at(entry) < FORK_OBJECTS_FROM || (parameter != 0u && parameter < SHARED_RECORDS))
			continue;    /* (stage 1's own scenery) */
		put16((uint32_t)(s16_at(entry) + shift));
		copy(entry + 2u, 10);
	}
	put16(GOAL_FORK);
	put16(STREAM_FORK);
	put32(branches[0]);
	put32(branches[1]);
	return start;
}

/* The pre-stage's stream for REVERSE, which finishes there: its objects past its start line and before its
   fork's (the fork camera, the WEST/EAST panel, the sign, the checkpoint gate), then a goal stage's finish
   (the flags of its approach, its flagman and gate, the crowd and parade past it) moved to its fork */
static uint32_t finish_stream(void)
{
	const int shift = PRE_STAGE_FORK - GOAL_FORK;
	const uint32_t start = here();
	uint32_t entry;

	for (entry = road_object_streams[0]; s16_at(entry + 2u) == STREAM_OBJECT; entry += 12u)
		if (s16_at(entry) >= START_OBJECTS_TO && s16_at(entry) < FORK_OBJECTS_FROM_0)
			copy(entry, 12);
	for (entry = road_object_streams[FIRST_GOAL];; entry += 12u)
	{
		const uint16_t command = (uint16_t)s16_at(entry + 2u);

		if (s16_at(entry) < GOAL_APPROACH && command == STREAM_OBJECT)
			continue;
		put16((uint32_t)(s16_at(entry) + shift));
		put16(command);
		if (command != STREAM_OBJECT)
			break;    /* (its end: the reader stops there) */
		copy(entry + 4u, 8);
	}
	return start;
}

/* ---- building it all, once ---- */

static int level_of(int stage)
{
	return stage == 0 ? 0 : stage < 3 ? 1 : stage < 7 ? 2 : stage < 0xD ? 3 : stage < FIRST_GOAL ? 4 : 5;
}

static void build(void)
{
	uint32_t forks[STAGES];
	uint32_t branch_streams[2];
	bool found[STAGES] = { false };

	if (reverse.built)
		return;
	reverse.built = true;
	data_elsewhere = console_data;

	/* the parents, from the forward forks */
	for (int stage = 0; stage < FIRST_GOAL; stage++)
		for (int branch = 0; branch < 2; branch++)
		{
			const int child = fork_next_stages[stage * 2 + branch];

			if (child >= STAGES || child == stage)
				continue;
			reverse.parents[child][found[child] ? 1 : 0] = (uint8_t)stage;
			if (!found[child])
				reverse.parents[child][1] = (uint8_t)stage;
			found[child] = true;
		}

	/* each stage's course, with room for its fork (the pre-stage's ends at its finish) */
	for (int stage = 0; stage < STAGES; stage++)
	{
		uint32_t end;

		reverse.courses[stage] = here();
		if (stage == 0)
		{
			copy_finish_course(stage_courses[0]);
			continue;
		}
		copy_course(stage_courses[stage], &end);
		forks[stage] = here();
		for (int byte = 0; byte < 12; byte++)
			put8(0);
	}
	for (int stage = 1; stage < STAGES; stage++)
	{
		uint32_t roads[2];

		for (int branch = 0; branch < 2; branch++)
			roads[branch] = branch_road(branch, reverse.courses[reverse.parents[stage][branch]]);
		reverse.data[forks[stage] - CONSOLE_DATA] = (uint8_t)(uint16_t)-COURSE_FORK;
		reverse.data[forks[stage] - CONSOLE_DATA + 1u] = (uint8_t)((uint16_t)-COURSE_FORK >> 8);
		reverse.data[forks[stage] - CONSOLE_DATA + 2u] = reverse.parents[stage][0];
		reverse.data[forks[stage] - CONSOLE_DATA + 3u] = reverse.parents[stage][1];
		patch32(forks[stage] + 4u, roads[0]);
		patch32(forks[stage] + 8u, roads[1]);
	}

	/* the goal stages' streams */
	for (int branch = 0; branch < 2; branch++)
		branch_streams[branch] = branch_stream(branch, GOAL_FORK - STAGE_FORK);
	for (int goal = 0; goal < GOALS; goal++)
		reverse.goal_streams[goal] = goal_stream(FIRST_GOAL + goal, branch_streams);
	reverse.finish_stream = finish_stream();
}

/* ---- the race ---- */

static bool reverse_here(void)
{
	return reverse.running && SCREEN(screen)->index == LEFT_SEAT && SCREEN(screen)->game_mode == GAME_MODE_PLAY;
}

void console_reverse_start(int goal)
{
	build();
	reverse.goal = FIRST_GOAL + goal;
	reverse.running = true;
}

bool console_reverse_running(void)
{
	return reverse.running;
}

/* On the left seat's frame, before its game mode's code: the race's first stage the goal picked, before the
   game loads it (game_mode_play step 5); the start line in the pre-stage's colours (its gate and stands take
   them from the pre-stage's scene palette, as the game loads one a stage), the stage's own once the car is
   past it; over with the game */
void console_reverse_frame(uint32_t mode)
{
	if (!reverse.running)
		return;
	if (mode != GAME_MODE_PLAY)
	{
		reverse.running = false;
		return;
	}
	if (SCREEN(screen)->game_step == 5u)
	{
		SCREEN(screen)->first_stage = (uint8_t)reverse.goal;
		SCREEN(screen)->route_stage = (uint8_t)reverse.goal;
		SCREEN(screen)->built_stage = (uint8_t)reverse.goal;
		reverse.start_colours = 0;
	}
	if (SCREEN(screen)->game_step == 23u && reverse.start_colours == 0)
	{
		queue_palette(scene_palettes[0]);
		reverse.start_colours = 1;
	}
	if (reverse.start_colours == 1 && RACE(SCREEN(screen)->race)->segment > START_LINE_PASSED)
	{
		queue_palette(scene_palettes[reverse.goal]);
		reverse.start_colours = 2;
	}
}

/* ---- the game's functions REVERSE puts its own in place of ---- */

/* The race's course: REVERSE's for its first stage */
void player_car_start_course(void)
{
	uint16_t width_code;
	uint32_t course;

	if (!reverse_here())
	{
		original_player_car_start_course();
		return;
	}
	/* (as the game's, from REVERSE's course) */
	move_halfwords(screen + 0x2000u, zero_block_ADDRESS, 0x200u);
	course = reverse.courses[SCREEN(screen)->first_stage];
	CAR(player_car)->course = course;
	width_code = COURSE_START(course)->width_code;
	CAR(player_car)->width_code = (uint8_t)width_code;
	CAR(player_car)->road_width = width_code * 0xB0u;
	CAR(player_car)->prebuilt_segments = SCREEN(screen)->draw_distance >> 7;
	do
	{
		player_car_build_segment(0xFFFu);
	} while ((int16_t)CAR(player_car)->built_segments < (int16_t)CAR(player_car)->prebuilt_segments);
	SCREEN(screen)->current_stage_level = SCREEN(screen)->built_stage_level;
	CAR(player_car)->stage_history[0] = SCREEN(screen)->route_stage;
}

/* The roadside objects: a goal stage's, where REVERSE starts, from its own stream; the pre-stage's, where it
   finishes, too (as the game's state 2 takes the next stage's stream) */
void road_object_stream_read(void)
{
	const int stage = SCREEN(screen)->built_stage;

	if (reverse_here() && OBJECT(current_object)->state == 0u && stage >= FIRST_GOAL && stage < STAGES)
	{
		OBJECT(current_object)->state = 1u;
		OBJECT(current_object)->road_object_stream.stream = reverse.goal_streams[stage - FIRST_GOAL];
	}
	if (reverse_here() && OBJECT(current_object)->state == 2u && stage == 0
		&& OBJECT(current_object)->road_object_stream.stage == 0u)
	{
		OBJECT(current_object)->state = 1u;
		OBJECT(current_object)->road_object_stream.stage_start = CAR(player_car)->built_segments;
		OBJECT(current_object)->road_object_stream.stream = reverse.finish_stream;
	}
	original_road_object_stream_read();
}

/* The background at a fork: the stage's own (stage_backgrounds; the game takes the fork's stage and the
   branch, which lead forward), and none new for the pre-stage, which has none */
void race_background_update(void)
{
	const uint8_t built_stage = SCREEN(screen)->built_stage;
	const uint8_t branch = CAR(player_car)->branch;
	uint8_t background;

	if (!reverse_here() || OBJECT(current_object)->state != 1u || SCREEN(screen)->stage_background_change != 0u
		|| CAR(player_car)->new_branch == 0u)
	{
		original_race_background_update();
		return;
	}
	if (SCREEN(screen)->route_stage == 0u)
	{
		CAR(player_car)->new_branch = 0u;
		original_race_background_update();
		return;
	}
	background = stage_backgrounds[SCREEN(screen)->route_stage];
	SCREEN(screen)->built_stage = background >> 1;    /* (the game's choice is built_stage * 2 + branch) */
	CAR(player_car)->branch = background & 1u;
	original_race_background_update();
	SCREEN(screen)->built_stage = built_stage;
	CAR(player_car)->branch = branch;
}

/* The STAGE label: the stage's own level (the game's counts the forks taken, PRE-STAGE to FINAL STAGE) */
void stage_indicator_draw(void)
{
	if (!reverse_here())
	{
		original_stage_indicator_draw();
		return;
	}
	draw_text(stage_level_texts[level_of(SCREEN(screen)->route_stage)]);
	draw_object(0u);
}

/* After the race: no route map (it draws the forward route) */
void route_map_update(void)
{
	if (!reverse_here())
	{
		original_route_map_update();
		return;
	}
	if (SCREEN(screen)->game_step == 14u)
	{
		SCREEN(screen)->game_step = 16u;
		OBJECT(current_object)->flags = 0u;
		OBJECT(current_object)->flags_high = 0u;
	}
}

/* No course record: the game keeps them by goal, and REVERSE finishes at the start (the game's hands back
   its zero flag: clear when beaten) */
uint32_t course_record_beaten(void)
{
	return reverse_here() ? 1u : original_course_record_beaten();
}

void course_records_update(void)
{
	if (!reverse.running)
		original_course_records_update();
}
