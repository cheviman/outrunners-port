/**
 *    Name:        route_select.c
 *    Description: & KNUCKLES > REVERSE's goal, picked on the route map: the map of the race's end, whole, its
 *                 ten goals, a car on the one picked
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The scene is the console's own game mode on the left seat's screen (console.c), as the SEGA
 *    presentation is: its objects are the game's, with handlers of the console's. The map is the one the
 *    race's end shows whole (route_map_update's last view): the routes over the world at 1:1, wider than the
 *    screen: the pre-stage's globe in the middle, the routes east of it to five goals and west of it to the
 *    other five, each half where route_map_whole_scroll_xs puts the screen for it; the WEST / EAST panel
 *    under the globe (route_map_direction_sign_show), moved with the map. The car is the route map's own
 *    (route_map_car_animations, model 0): it drives from goal to goal as the player picks them, up and down
 *    through a side's, left and right across to the other side's, and the map scrolls after it. START or
 *    ACCEPT starts REVERSE there, BACK goes back to & KNUCKLES.
 */
#include "console.h"
#include "console_internal.h"
#include "console_draw.h"

#include "game.h"

#define GOALS               10
#define SIDE_GOALS          5
#define EAST                0          /* the pre-stage's branch 0 (route_map_whole_scroll_xs' first) */
#define WEST                1
#define CAR_MODEL           0          /* the generic car: the route map's first model */
#define CAR_SIZE            0x50u      /* as the route map's */
#define CAR_FRAME           0xCu
#define CAR_OFFSET          10         /* the car beside the end of the goal's route, away from the globe */
#define MOVE_FRAMES         8          /* the car from goal to goal on a side */
#define CROSS_FRAMES        24         /* and across the map */
#define PANEL_X             0x220      /* the panel's x on the map (its screen x 18 for the east half) */
#define HEADING_Y           12
#define NAME_Y              30
#define SCREEN_WIDTH        320

/* Each goal's place on the map (stages 15-1E: its x, the screen's scroll x added; its y), where its route
   ends: the east's five from the bottom up, the west's from the top down */
static const int16_t goal_places[GOALS][2] = {
	{ 780, 191 }, { 791, 150 }, { 802, 109 }, { 813, 68 }, { 824, 27 },    /* AUSTRALIA to RUSSIA */
	{ 307, 27 }, { 297, 68 }, { 285, 109 }, { 274, 150 }, { 264, 191 }     /* NORTHERN EUROPE to KENYA */
};

/* the goals, as the test mode's BOOKKEEPING names them (stages 15-1E) */
static const char *const goal_names[GOALS] = { "AUSTRALIA", "HONGKONG", "JAPAN", "CHINA", "RUSSIA", "NORTHERN EUROPE",
	"SWITZERLAND", "ATLANTIC OCEAN", "SPAIN", "KENYA" };

static struct
{
	bool running;
	int frame;
	int goal;                      /* the goal picked (0-9) */
	int scroll;                    /* the screen's place on the map */
	int car_x, car_y;              /* the car on the map, driving to the goal's place */
	int from_x, from_y, moving, move_frames;
} pick;

static int side_of(int goal)
{
	return goal < SIDE_GOALS ? EAST : WEST;
}

/* From the top down on its side */
static int rank_of(int goal)
{
	return side_of(goal) == EAST ? SIDE_GOALS - 1 - goal : goal - SIDE_GOALS;
}

static int goal_at(int side, int rank)
{
	return side == EAST ? SIDE_GOALS - 1 - rank : SIDE_GOALS + rank;
}

static int car_place_x(int goal)
{
	return goal_places[goal][0] + (side_of(goal) == EAST ? CAR_OFFSET : -CAR_OFFSET);
}

/* ---- the map ---- */

static const struct route_map_registers *map_registers(void)
{
	return ROUTE_MAP_REGISTERS(route_map_register_sets_ADDRESS + ((uint32_t)SCREEN(screen)->index << 6));
}

/* The map's scroll (the routes' tilemap and the world's under it, as route_map_update has them) and the
   panel's place with it */
static void map_scroll(int scroll)
{
	const struct route_map_registers *registers = map_registers();
	const uint32_t panel = console_find_object(route_map_direction_sign_show_ADDRESS, 0u);

	pick.scroll = scroll;
	VIDEO_REGISTER_WORDS(registers->scroll_x_address)[0] = (uint16_t)scroll;
	VIDEO_REGISTER_WORDS(registers->register_scroll_x_address)[0] = (uint16_t)scroll;
	VIDEO_REGISTER_WORDS(registers->second_scroll_x_address)[0] = (uint16_t)(scroll - 0x88);
	VIDEO_REGISTER_WORDS(registers->second_register_scroll_x_address)[0] = (uint16_t)(scroll - 0x88);
	send_video_registers = 0xFFu;
	if (panel)
		OBJECT(panel)->x = (uint16_t)(PANEL_X - scroll);
}

/* The whole map of the race's end: its tilemaps, palette, zoom and scrolls as route_map_update leaves them
   (states 0, 1 and 16), the sea black, the WEST / EAST panel */
static void map_set_up(int side)
{
	const uint32_t index = SCREEN(screen)->index;
	const struct route_map_registers *registers = map_registers();

	SCREEN(screen)->route_branches[0] = (uint8_t)side;    /* (the panel's first place) */
	set_up_background(0x4868u + (index * 10u + (uint32_t)side * 5u) * 4u);
	queue_palette(route_map_background_palette);
	io_write16(route_map_sea_colour_addresses[index], 0u);
	PAGE_SELECT(registers->pages_address)[0] = route_map_routes_pages[index];
	PAGE_SELECT(registers->second_pages_address)[0] = route_map_world_pages[index];
	VIDEO_REGISTER_WORDS(registers->zoom_x_address)[0] = 0x200u;
	VIDEO_REGISTER_WORDS(registers->zoom_y_address)[0] = 0x200u;
	VIDEO_REGISTER_WORDS(registers->zoom_center_x_address)[0] = 0xA0u;
	VIDEO_REGISTER_WORDS(registers->zoom_center_y_address)[0] = 0x70u;
	VIDEO_REGISTER_WORDS(registers->scroll_y_address)[0] = 0x70u;
	VIDEO_REGISTER_WORDS(registers->register_scroll_y_address)[0] = 0x70u;
	VIDEO_REGISTER_WORDS(registers->second_scroll_y_address)[0] = 0u;
	VIDEO_REGISTER_WORDS(registers->second_register_scroll_y_address)[0] = 0u;
	OBJECT(start_object())->handler = route_map_direction_sign_show_ADDRESS;
	map_scroll(route_map_whole_scroll_xs[side]);
}

/* ---- the car (console_code) ---- */

void route_select_car_update(void)
{
	if (OBJECT(current_object)->state == 0u)
	{
		OBJECT(current_object)->draw_mode = 0xAu;
		animation_start(route_map_car_animations[CAR_MODEL]);
		OBJECT(current_object)->palette = route_map_car_palettes[CAR_MODEL];
		OBJECT(current_object)->width_scale = CAR_SIZE;
		OBJECT(current_object)->height_scale = CAR_SIZE;
		OBJECT(current_object)->frame = CAR_FRAME;
		OBJECT(current_object)->state++;
	}
	OBJECT(current_object)->x = (uint16_t)(pick.car_x - pick.scroll);
	OBJECT(current_object)->y = (uint16_t)pick.car_y;
	map_depth_from_y();
	draw_object(0u);
}

/* ---- the scene ---- */

void console_route_select_start(int goal)
{
	pick.running = true;
	pick.frame = 0;
	pick.goal = goal >= 0 && goal < GOALS ? goal : 0;
	pick.moving = 0;
}

bool console_route_select_running(void)
{
	return pick.running;
}

int console_route_select_goal(void)
{
	return pick.goal;
}

int console_route_select_frame(uint32_t pressed)
{
	const int side = side_of(pick.goal), rank = rank_of(pick.goal);
	int choice = pick.goal;

	if (pick.frame++ == 0)
	{
		/* as the title clears its screen, from black */
		clear_objects();
		reset_tile_layers();
		clear_sprite_group_count();
		clear_text_layer();
		map_set_up(side);
		pick.car_x = car_place_x(pick.goal);
		pick.car_y = goal_places[pick.goal][1];
		OBJECT(start_object())->handler = CONSOLE_CODE_ROUTE_CAR;
		SCREEN(screen)->fade_kind = 1u;
		SCREEN(screen)->flags |= 1u;
		return ROUTE_SELECT_NOTHING;
	}
	if (pressed & (ACTION_BIT(ACTION_START) | ACTION_BIT(ACTION_ACCEPT)))
	{
		queue_sound(PICK_SOUND);
		pick.running = false;
		return ROUTE_SELECT_START;
	}
	if (pressed & ACTION_BIT(ACTION_BACK))
	{
		queue_sound(PICK_SOUND);
		pick.running = false;
		return ROUTE_SELECT_BACK;
	}
	if (pressed & ACTION_BIT(ACTION_UP))
		choice = goal_at(side, rank > 0 ? rank - 1 : SIDE_GOALS - 1);
	else if (pressed & ACTION_BIT(ACTION_DOWN))
		choice = goal_at(side, rank < SIDE_GOALS - 1 ? rank + 1 : 0);
	else if (pressed & (ACTION_BIT(ACTION_LEFT) | ACTION_BIT(ACTION_RIGHT)))
		choice = goal_at(1 - side, rank);
	if (choice != pick.goal)
	{
		queue_sound(MOVE_SOUND);
		pick.goal = choice;
		pick.from_x = pick.car_x;
		pick.from_y = pick.car_y;
		pick.move_frames = side_of(choice) != side ? CROSS_FRAMES : MOVE_FRAMES;
		pick.moving = pick.move_frames;
	}
	if (pick.moving)
	{
		const int done = pick.move_frames - --pick.moving;

		pick.car_x = pick.from_x + (car_place_x(pick.goal) - pick.from_x) * done / pick.move_frames;
		pick.car_y = pick.from_y + (goal_places[pick.goal][1] - pick.from_y) * done / pick.move_frames;
	}
	{
		/* the map after the car, from one side's place to the other's */
		const int west = route_map_whole_scroll_xs[WEST], east = route_map_whole_scroll_xs[EAST];
		int scroll = pick.car_x - SCREEN_WIDTH / 2;

		scroll = scroll < west ? west : (scroll > east ? east : scroll);
		if (scroll != pick.scroll)
			map_scroll(scroll);
	}
	{
		const char *name = goal_names[pick.goal];

		console_text((SCREEN_WIDTH - console_text_width("REVERSE", 2)) / 2, HEADING_Y, 2, 0x0100u, "REVERSE");
		console_text((SCREEN_WIDTH - console_text_width(name, 1)) / 2, NAME_Y, 1, 0x00F0u, name);
	}
	return ROUTE_SELECT_NOTHING;
}
