/**
 *    Name:        network.c
 *    Description: the network game: rooms (HOST, JOIN) of up to four devices, each one of the arcade's linked
 *                 cabinets in the room's races, and the link's race records sent and received over UDP
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The arcade links up to four cabinets of two seats: each one runs its own two cars and sends its two race
 *    records (their first 70 bytes) to the others through the link board, which hands every cabinet all
 *    eight, its own among them, 30 times a second; machine 1's left seat (player 0) takes the race's entries,
 *    the first one in a race (its host) keeps its time and checkpoints, and every screen reads them from their
 *    records. Nothing waits frame by frame for the others: each cabinet takes the newest records it has.
 *
 *    Here each device is a cabinet and the link board is UDP: every device sends its records to every other
 *    of the room each frame, and takes theirs as the board would hand them (link_receive_records). A room is
 *    hosted by a device, which sends a beacon to the local network every half second for the others to find it
 *    (JOIN) and keeps its list of devices; its START links them in the list's order (the host machine 1) and
 *    presses each one's START, as an ARCADE race of all of them. Every datagram is written byte by byte, so
 *    that the devices need not be alike (a computer, a phone, a Switch).
 */
#include "console.h"
#include "console_internal.h"
#include "net.h"

#include "game.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define VERSION             1          /* the datagrams' and the game's: devices of another version are not heard */
#define MACHINES            4          /* the link's cabinets */
#define SLOTS               8          /* their seats, the race records */
#define RECORD_BYTES        0x70u      /* of a race record, those the link carries */
#define RECORD_SIZE         0x80u
#define ROOMS               8          /* the rooms JOIN lists */
#define BEACON_FRAMES       30         /* a room's beacon, every half second */
#define ROOM_FORGOTTEN      180        /* a room unheard for 3 seconds is gone from JOIN's list */
#define JOIN_FRAMES         10         /* JOIN again until the room lists the device */
#define JOIN_GIVEN_UP       300        /* a room that does not answer in 5 seconds */
#define SILENT_FRAMES       20         /* a device unheard for a third of a second: its race waits */
#define LOST_FRAMES         180        /* and for 3 seconds: it is gone */
#define PACKET_MOST         1400
#define ENTRIES_CUT         0x28u      /* race_entries_update's entry_frames once every player is in */
#define RACE_PART           0x30u      /* a race record's race (race_record_clear): the car's state, its passes */

static const uint8_t magic[4] = { 'O', 'R', 'U', 'N' };

enum { PACKET_BEACON = 1, PACKET_JOIN, PACKET_STATE, PACKET_LEAVE };

/* What a device's STATE says of it (flags) */
#define BUSY                0x01u      /* a seat of it plays a race */

typedef struct
{
	uint32_t id;
	char name[CONSOLE_NAME_LENGTH + 1];
	uint8_t players;           /* 1, or 2 (both seats) */
	net_address address;       /* as the room's host hears it (0: the host itself) */
} member;

/* A device heard: its address (where its datagrams come from), when, and what its STATE said */
typedef struct
{
	uint32_t id;
	net_address address;
	uint32_t heard;            /* the frame */
	uint8_t flags;
} peer;

static struct
{
	int role;
	uint32_t id;               /* this device's, at random */
	uint32_t frame;
	char problem[32];          /* what went wrong, for the NETWORK page (once) */

	/* the room: its id (its first host's), its list (the host first, then the link's machines' order) */
	uint32_t room;
	member members[MACHINES];
	int member_count;
	uint16_t list_version;
	uint8_t start_serial;      /* the host's STARTs */
	peer peers[SLOTS];
	uint32_t role_frame;       /* JOINING: since when */
	net_address joining;       /* JOINING: the room's host */

	/* the rooms found (JOIN) */
	struct
	{
		uint32_t room;
		net_address address;
		char name[CONSOLE_NAME_LENGTH + 1];
		int members;
		uint32_t heard;
	} rooms[ROOMS];
	int room_count;

	/* the link: the START it is for, this device's machine (1-4), the machines and who they are */
	bool linked;
	uint8_t link_serial;
	int machine;
	int machines;
	int players;               /* the players of the race's devices, all of them */
	uint32_t link_ids[MACHINES];
	bool link_lost[MACHINES];
	uint8_t slots[SLOTS][RECORD_BYTES];
	int start_pending;         /* the room's START, for the left seat's frame: 0, 1 or 2 seats */
	bool waiting;              /* a race waits for a device unheard */
} net;

enum { ROLE_OFF, ROLE_SEARCHING, ROLE_JOINING, ROLE_IN_ROOM };

static int players_wanted = 1;    /* NETWORK > PLAYERS */

/* ---- bytes ---- */

typedef struct
{
	uint8_t *bytes;
	size_t size;
	size_t room;
} writer;

typedef struct
{
	const uint8_t *bytes;
	size_t size;
	size_t at;
	bool short_of;             /* read past its end */
} reader;

static void put8(writer *out, uint32_t value)
{
	if (out->size < out->room)
		out->bytes[out->size] = (uint8_t)value;
	out->size++;
}

static void put16(writer *out, uint32_t value)
{
	put8(out, value);
	put8(out, value >> 8);
}

static void put32(writer *out, uint32_t value)
{
	put16(out, value);
	put16(out, value >> 16);
}

static void put_bytes(writer *out, const void *bytes, size_t size)
{
	for (size_t at = 0; at < size; at++)
		put8(out, ((const uint8_t *)bytes)[at]);
}

static void put_name(writer *out, const char *name)
{
	size_t length = strlen(name);

	for (size_t at = 0; at < CONSOLE_NAME_LENGTH; at++)
		put8(out, at < length ? (uint8_t)name[at] : 0u);
}

static uint32_t get8(reader *in)
{
	if (in->at >= in->size)
	{
		in->short_of = true;
		return 0;
	}
	return in->bytes[in->at++];
}

static uint32_t get16(reader *in)
{
	const uint32_t low = get8(in);

	return low | get8(in) << 8;
}

static uint32_t get32(reader *in)
{
	const uint32_t low = get16(in);

	return low | get16(in) << 16;
}

static void get_bytes(reader *in, void *bytes, size_t size)
{
	for (size_t at = 0; at < size; at++)
		((uint8_t *)bytes)[at] = (uint8_t)get8(in);
}

/* A name of the datagram: the font's letters only (others as spaces) */
static void get_name(reader *in, char name[CONSOLE_NAME_LENGTH + 1])
{
	for (int at = 0; at < CONSOLE_NAME_LENGTH; at++)
	{
		const char letter = (char)get8(in);

		name[at] = (letter >= 'A' && letter <= 'Z') || (letter >= '0' && letter <= '9') || letter == '.' || letter == '-'
			|| letter == 0 ? letter : ' ';
	}
	name[CONSOLE_NAME_LENGTH] = '\0';
}

/* A datagram's start: who sends it, of what room */
static void put_header(writer *out, int kind)
{
	put_bytes(out, magic, sizeof(magic));
	put8(out, VERSION);
	put8(out, (uint32_t)kind);
	put32(out, net.id);
	put32(out, net.room);
}

/* ---- the devices heard ---- */

static peer *peer_of(uint32_t id, bool add)
{
	peer *oldest = &net.peers[0];

	for (int index = 0; index < SLOTS; index++)
	{
		if (net.peers[index].id == id)
			return &net.peers[index];
		if (net.peers[index].heard < oldest->heard)
			oldest = &net.peers[index];
	}
	if (!add)
		return NULL;
	memset(oldest, 0, sizeof(*oldest));
	oldest->id = id;
	oldest->heard = net.frame;
	return oldest;
}

/* Where a member's datagrams go: where they come from once heard, else where the room's host hears them */
static bool member_address(const member *who, net_address *address)
{
	const peer *heard = peer_of(who->id, false);

	if (heard && heard->address.ip)
	{
		*address = heard->address;
		return true;
	}
	if (who->address.ip)
	{
		*address = who->address;
		return true;
	}
	return false;
}

static int member_index(uint32_t id)
{
	for (int index = 0; index < net.member_count; index++)
		if (net.members[index].id == id)
			return index;
	return -1;
}

static bool hosting(void)
{
	return net.role == ROLE_IN_ROOM && net.member_count > 0 && net.members[0].id == net.id;
}

/* Frames since a member was last heard (0 for this device) */
static uint32_t unheard(uint32_t id)
{
	const peer *heard;

	if (id == net.id)
		return 0;
	heard = peer_of(id, false);
	return heard ? net.frame - heard->heard : LOST_FRAMES;
}

/* ---- the link ---- */

/* The race records' entries as a link starts them: none taken, none asked for, every game over cleared */
static void clear_entries(uint32_t race)
{
	RACE(race)->entry_request = 0u;
	RACE(race)->entry_frames = 0u;
	RACE(race)->entry_serial_seen = 0u;
	RACE(race)->entry_serial = 0u;
	RACE(race)->entrant = 0xFFu;
	RACE(race)->game_over = 0u;
}

static void clear_screen_entries(uint32_t screen_block)
{
	memset(SCREEN(screen_block)->accepted_players, 0, sizeof(SCREEN(screen_block)->accepted_players));
	SCREEN(screen_block)->entrant_count = 0u;
	SCREEN(screen_block)->entry_accepted = 0u;
	SCREEN(screen_block)->is_host = 0u;
}

/* The two seats as machine 'machine' of 'machines' (network_check_join and network_check_result's set-up,
   with the car numbers the players'), every record's copy and entries cleared; machine 1's settings in its
   first record for the others (the master_ fields) */
static void link_set_up(int machine, int machines)
{
	const uint32_t first = (uint32_t)(machine - 1) * 2u;

	link_off = 0u;
	link_failed = 0u;
	machine_count = (uint8_t)machines;
	player_count = (uint8_t)(machines * 2);
	for (int player = 0; player < SLOTS; player++)
		car_numbers[player] = (uint8_t)player;
	for (int seat = 0; seat < 2; seat++)
	{
		const uint32_t race = seat ? RACE_2 : RACE_1, screen_block = seat ? SCREEN_2 : SCREEN_1;

		RACE(race)->machine = (uint8_t)machine;
		RACE(race)->player = (uint8_t)(first + seat);
		RACE(race)->car_number = (uint8_t)(first + seat);
		SCREEN(screen_block)->car_number = (uint8_t)(first + seat);
		SCREEN(screen_block)->player = (uint8_t)(first + seat);
		SCREEN(screen_block)->race = race;
		SCREEN(screen_block)->race_copy = race_copy_offsets[first + seat] + GAME_VARIABLES;
		clear_entries(race);
		clear_screen_entries(screen_block);
		memset((uint8_t *)RACE(race) + RACE_PART, 0, RECORD_SIZE - RACE_PART);    /* (race_record_clear's) */
	}
	if (machine == 1)
		console_master_settings();
	memset(RACE(RACE_COPY_1), 0, SLOTS * RECORD_SIZE);
	memset(net.slots, 0, sizeof(net.slots));
	memset(net.link_lost, 0, sizeof(net.link_lost));
	net.linked = true;
	net.machine = machine;
	net.machines = machines;
	net.waiting = false;
}

/* The link off: the machine alone again (network_check_stand_alone) */
static void link_end(void)
{
	if (!net.linked)
		return;
	net.linked = false;
	net.waiting = false;
	network_check_stand_alone();
	clear_entries(RACE_1);
	clear_entries(RACE_2);
	clear_screen_entries(SCREEN_1);
	clear_screen_entries(SCREEN_2);
	memset(RACE(RACE_COPY_1), 0, SLOTS * RECORD_SIZE);
	car_numbers[0] = 0u;
	car_numbers[1] = 1u;
}

/* The machine whose datagrams carry a slot's record: its own, or the first one still heard for one gone */
static int publisher(int slot)
{
	const int owner = slot / 2 + 1;

	if (!net.link_lost[owner - 1])
		return owner;
	for (int machine = 1; machine <= net.machines; machine++)
		if (!net.link_lost[machine - 1])
			return machine;
	return owner;
}

/* The room's START, as the host's list has it: this device's machine its place in the list */
static void start_race(void)
{
	const int index = member_index(net.id);

	if (index < 0)
		return;
	net.link_serial = net.start_serial;
	net.players = 0;
	for (int machine = 0; machine < MACHINES; machine++)
		net.link_ids[machine] = machine < net.member_count ? net.members[machine].id : 0u;
	for (int machine = 0; machine < net.member_count; machine++)
		net.players += net.members[machine].players;
	link_set_up(index + 1, net.member_count);
	net.start_pending = net.members[index].players;
}

/* True while a seat of this device plays a game */
static bool busy(void)
{
	return SCREEN(SCREEN_1)->game_mode == GAME_MODE_PLAY || SCREEN(SCREEN_2)->game_mode == GAME_MODE_PLAY;
}

/* ---- the datagrams ---- */

static void send_to(net_address to, const writer *out)
{
	if (out->size <= out->room)
		net_send(to, out->bytes, out->size);
}

/* The room's list, its START and this device's records (with those of a machine gone it carries) */
static void send_state(void)
{
	uint8_t bytes[PACKET_MOST];
	writer out = { bytes, 0, sizeof(bytes) };
	size_t slot_count_at;
	int slot_count = 0;

	put_header(&out, PACKET_STATE);
	put16(&out, net.list_version);
	put8(&out, net.start_serial);
	put8(&out, (uint32_t)net.member_count);
	for (int index = 0; index < net.member_count; index++)
	{
		put32(&out, net.members[index].id);
		put_name(&out, net.members[index].name);
		put8(&out, net.members[index].players);
		put32(&out, net.members[index].address.ip);
		put16(&out, net.members[index].address.port);
	}
	put8(&out, busy() ? BUSY : 0u);
	put8(&out, net.linked ? net.link_serial : 0u);
	put8(&out, net.linked ? (uint32_t)net.machine : 0u);
	slot_count_at = out.size;
	put8(&out, 0u);
	for (int slot = 0; net.linked && slot < net.machines * 2; slot++)
	{
		if (publisher(slot) != net.machine)
			continue;
		put8(&out, (uint32_t)slot);
		if (slot / 2 + 1 == net.machine)
			put_bytes(&out, RACE(slot & 1 ? RACE_2 : RACE_1), RECORD_BYTES);
		else
			put_bytes(&out, net.slots[slot], RECORD_BYTES);
		slot_count++;
	}
	if (slot_count_at < out.room)
		bytes[slot_count_at] = (uint8_t)slot_count;
	for (int index = 0; index < net.member_count; index++)
	{
		net_address to;

		if (net.members[index].id != net.id && member_address(&net.members[index], &to))
			send_to(to, &out);
	}
}

static void send_beacon(void)
{
	uint8_t bytes[64];
	writer out = { bytes, 0, sizeof(bytes) };
	const net_address everyone = { NET_BROADCAST, NET_BEACON_PORT }, here = { NET_LOOPBACK, NET_BEACON_PORT };

	put_header(&out, PACKET_BEACON);
	put_name(&out, net.members[0].name);
	put8(&out, (uint32_t)net.member_count);
	send_to(everyone, &out);
	send_to(here, &out);    /* (a second game on this computer) */
}

static void send_join(void)
{
	uint8_t bytes[64];
	writer out = { bytes, 0, sizeof(bytes) };

	put_header(&out, PACKET_JOIN);
	put_name(&out, console_window_settings.name);
	put8(&out, (uint32_t)console_network_players());
	send_to(net.joining, &out);
}

static void send_leave(void)
{
	uint8_t bytes[32];
	writer out = { bytes, 0, sizeof(bytes) };

	put_header(&out, PACKET_LEAVE);
	for (int index = 0; index < net.member_count; index++)
	{
		net_address to;

		if (net.members[index].id != net.id && member_address(&net.members[index], &to))
			send_to(to, &out);
	}
}

/* A beacon: the room into JOIN's list (where it comes from: its host's address; one from this computer's
   loopback only until the network's comes) */
static void take_beacon(reader *in, uint32_t room, net_address from)
{
	int index;

	for (index = 0; index < net.room_count && net.rooms[index].room != room; index++)
		;
	if (index == net.room_count)
	{
		if (net.room_count == ROOMS)
			return;
		net.room_count++;
		net.rooms[index].room = room;
		net.rooms[index].address = from;
	}
	else if (from.ip != NET_LOOPBACK)
		net.rooms[index].address = from;
	get_name(in, net.rooms[index].name);
	net.rooms[index].members = (int)get8(in);
	net.rooms[index].heard = net.frame;
}

/* A device's JOIN, at the room's host: into the list where there is room */
static void take_join(reader *in, uint32_t sender, net_address from)
{
	member *joined;

	if (!hosting() || member_index(sender) >= 0 || net.member_count == MACHINES)
		return;
	joined = &net.members[net.member_count++];
	memset(joined, 0, sizeof(*joined));
	joined->id = sender;
	get_name(in, joined->name);
	joined->players = (uint8_t)get8(in);
	if (joined->players < 1 || joined->players > 2)
		joined->players = 1;
	joined->address = from;
	net.list_version++;
}

/* A member's STATE: the room's list from its host (a newer one), START, and the records it carries */
static void take_state(reader *in, uint32_t sender)
{
	member list[MACHINES];
	uint16_t version;
	uint8_t start_serial, flags, link_serial, machine;
	int count, slot_count;
	peer *heard = peer_of(sender, false);

	version = (uint16_t)get16(in);
	start_serial = (uint8_t)get8(in);
	count = (int)get8(in);
	if (count > MACHINES)
		return;
	for (int index = 0; index < count; index++)
	{
		list[index].id = get32(in);
		get_name(in, list[index].name);
		list[index].players = (uint8_t)get8(in);
		list[index].address.ip = get32(in);
		list[index].address.port = (uint16_t)get16(in);
	}
	flags = (uint8_t)get8(in);
	link_serial = (uint8_t)get8(in);
	machine = (uint8_t)get8(in);
	if (in->short_of)
		return;
	if (heard)
		heard->flags = flags;

	/* the list from the room's host: JOINING ends once it lists this device */
	if (count > 0 && list[0].id == sender && (int16_t)(version - net.list_version) >= 0)
	{
		const bool new_start = net.role == ROLE_IN_ROOM && start_serial != net.start_serial;

		memcpy(net.members, list, sizeof(list[0]) * (size_t)count);
		net.member_count = count;
		net.list_version = version;
		net.start_serial = start_serial;
		if (net.role == ROLE_JOINING && member_index(net.id) >= 0)
		{
			net.role = ROLE_IN_ROOM;
			net_stop_beacons();
		}
		else if (new_start && member_index(net.id) >= 0 && !busy())
			start_race();
	}

	/* the records it carries, of the link this device has */
	slot_count = (int)get8(in);
	for (int index = 0; index < slot_count && !in->short_of; index++)
	{
		const int slot = (int)get8(in);
		uint8_t record[RECORD_BYTES];

		get_bytes(in, record, RECORD_BYTES);
		if (in->short_of || !net.linked || link_serial != net.link_serial || slot >= net.machines * 2)
			continue;
		if (publisher(slot) == machine && slot / 2 + 1 != net.machine)
			memcpy(net.slots[slot], record, RECORD_BYTES);
	}
}

static void take_datagrams(void)
{
	uint8_t bytes[PACKET_MOST];
	net_address from;
	size_t size;

	while ((size = net_receive_beacon(bytes, sizeof(bytes), &from)) > 0 || (size = net_receive(bytes, sizeof(bytes), &from)) > 0)
	{
		reader in = { bytes, size, 0, false };
		uint8_t start[4];
		uint32_t kind, sender, room;
		peer *heard;

		get_bytes(&in, start, sizeof(start));
		if (memcmp(start, magic, sizeof(magic)) != 0 || get8(&in) != VERSION)
			continue;
		kind = get8(&in);
		sender = get32(&in);
		room = get32(&in);
		if (in.short_of || sender == net.id || sender == 0u)
			continue;
		if (kind == PACKET_BEACON)
		{
			if (net.role == ROLE_SEARCHING)
				take_beacon(&in, room, from);
			continue;
		}
		if (room != net.room || net.role == ROLE_OFF || net.role == ROLE_SEARCHING)
			continue;
		heard = peer_of(sender, true);
		heard->heard = net.frame;
		heard->address = from;
		switch (kind)
		{
			case PACKET_JOIN:
				take_join(&in, sender, from);
				break;
			case PACKET_STATE:
				take_state(&in, sender);
				break;
			case PACKET_LEAVE:
				heard->heard = net.frame - LOST_FRAMES;    /* (gone at once) */
				break;
			default:
				break;
		}
	}
}

/* ---- a machine of the link gone ---- */

/* Its players go from the race as if out of it: another machine sends their records on (publisher), their
   cars out of every screen's sight, and the race's host takes them from its order (racer_count, race_order),
   so that nothing of the race waits for them (the passes, players_not_ready). A race whose host is gone pauses
   until the first of its players still heard (the lowest number) hosts it on with the host's last record: its
   time, the time left, the passes and the order */

#define HIDDEN_SEGMENT      0xFFFFu    /* past every screen's draw distance (race_car_draw) */
#define HOST_TAKEN          0xFFu      /* in a record's link_pad_5A (nothing of the game's reads it): the race
                                          hosted on from a host gone */

static bool player_lost(int player)
{
	return net.link_lost[player / 2];
}

static uint32_t copy_of(int player)
{
	return race_copy_offsets[player] + GAME_VARIABLES;
}

static uint8_t *race_order(uint32_t race)
{
	return (uint8_t *)RACE(race) + offsetof(struct race, race_order);
}

/* The race host's record without the players gone */
static void expel_lost(uint32_t race)
{
	uint8_t *order = race_order(race);
	const int count = RACE(race)->racer_count;
	int kept = 0;

	for (int index = 0; index < count && index < SLOTS; index++)
		if (!player_lost(order[index]))
			order[kept++] = order[index];
	if (kept > 0)
		RACE(race)->racer_count = (uint8_t)kept;
}

/* A screen's player hosts the race on from the host's last record (its copy): race_progress_update's own
   counts of the passes from the screen's (each pass counted there as the host published it) */
static void host_race_on(uint32_t screen_block, uint32_t old)
{
	const uint32_t race = SCREEN(screen_block)->race;

	RACE(race)->racer_count = RACE(old)->racer_count;
	memcpy(race_order(race), race_order(old), SLOTS);
	RACE(race)->race_time = RACE(old)->race_time;
	RACE(race)->time_left = RACE(old)->time_left;
	RACE(race)->pass_count = RACE(old)->pass_count;
	RACE(race)->pass_player = RACE(old)->pass_player;
	RACE(race)->pass_checkpoint = RACE(old)->pass_checkpoint;
	RACE(race)->pass_order = RACE(old)->pass_order;
	RACE(race)->pass_branch = RACE(old)->pass_branch;
	RACE(race)->pass_time = RACE(old)->pass_time;
	RACE(race)->link_pad_5A = HOST_TAKEN;
	memcpy(SCREEN(screen_block)->counted_passes, SCREEN(screen_block)->checkpoint_passes,
		sizeof(SCREEN(screen_block)->counted_passes));
	memcpy(SCREEN(screen_block)->counted_stages, SCREEN(screen_block)->checkpoints_passed,
		sizeof(SCREEN(screen_block)->counted_stages));
	SCREEN(screen_block)->is_host = 0xFFu;
}

/* A seat in a race: its host's players gone from it, or its host gone (another hosts on: true while it waits
   for the new host's record) */
static bool follow_host(uint32_t screen_block)
{
	const int host = SCREEN(screen_block)->host_player;
	const uint32_t old = copy_of(host);
	const uint8_t *order = race_order(old);
	int next = -1;

	if (!SCREEN(screen_block)->in_game || host >= net.machines * 2)
		return false;
	if (!player_lost(host))
	{
		if (SCREEN(screen_block)->is_host)
			expel_lost(SCREEN(screen_block)->race);
		return false;
	}
	for (int index = 0; index < RACE(old)->racer_count && index < SLOTS; index++)
		if (!player_lost(order[index]) && (next < 0 || order[index] < next))
			next = order[index];
	if (next < 0)
		return false;
	if (next == SCREEN(screen_block)->player)
		host_race_on(screen_block, old);
	else if (RACE(copy_of(next))->link_pad_5A != HOST_TAKEN)
		return true;    /* (the new host's record not here yet) */
	SCREEN(screen_block)->host_player = (uint8_t)next;
	SCREEN(screen_block)->host_record = copy_of(next);
	if (SCREEN(screen_block)->is_host)
		expel_lost(SCREEN(screen_block)->race);
	return false;
}

/* The records of a machine gone this one sends on: from their last, the car out of sight, ready, the
   entries seen (as race_entries_update waits for every record's), and player 0's entries closed as its
   countdown would close them */
static void carry_lost_records(void)
{
	const uint8_t entry_serial = RACE(RACE_COPY_1)->entry_serial;

	for (int slot = 0; slot < net.machines * 2; slot++)
	{
		struct race *record = (struct race *)net.slots[slot];

		if (!player_lost(slot) || publisher(slot) != net.machine)
			continue;
		record->segment = HIDDEN_SEGMENT;
		record->ready = 0xFFu;
		record->entry_request = 0u;
		record->entry_serial_seen = entry_serial;
		if (slot == 0 && record->entry_frames != 0u && (net.frame & 1u) == 0u && --record->entry_frames == 0u)
		{
			record->entry_serial++;
			record->entrant = 0xFFu;
		}
	}
}

/* The link's part of the frame: the machines gone, their records, the races' hosts; true while a race of this
   device waits for a new host */
static bool carry_link(void)
{
	bool waiting;

	if (!net.linked)
		return false;
	carry_lost_records();
	waiting = follow_host(SCREEN_1);
	waiting = follow_host(SCREEN_2) || waiting;
	return waiting && busy();
}

/* ---- the room's devices gone ---- */

/* A member unheard for LOST_FRAMES is gone: the host takes it off its list (and the others take the host
   off theirs: the next one hosts), and a machine of the link is lost (its records carried by another); true
   while a machine of the link is unheard for SILENT_FRAMES and not yet gone */
static bool check_members(void)
{
	bool silent = false;

	for (int machine = 1; net.linked && machine <= net.machines; machine++)
	{
		const uint32_t id = net.link_ids[machine - 1];
		const uint32_t frames = unheard(id);

		if (net.link_lost[machine - 1])
			continue;
		if (frames >= LOST_FRAMES)
			net.link_lost[machine - 1] = true;
		else if (frames >= SILENT_FRAMES)
			silent = true;
	}
	for (int index = net.member_count - 1; index >= 0; index--)
	{
		const uint32_t id = net.members[index].id;

		if (id == net.id || unheard(id) < LOST_FRAMES || (index != 0 && !hosting()))
			continue;
		memmove(&net.members[index], &net.members[index + 1], sizeof(member) * (size_t)(net.member_count - index - 1));
		net.member_count--;
		net.list_version++;
	}
	return silent;
}

/* ---- the port's frame ---- */

static uint32_t new_id(void)
{
	uint32_t id = (uint32_t)time(NULL) * 2654435761u ^ (uint32_t)clock() ^ (uint32_t)(uintptr_t)&id;

	id ^= id >> 15;
	id *= 0x2C1B3C6Du;
	id ^= id >> 12;
	return id ? id : 1u;
}

static void open_network(void)
{
	if (!net.id)
		net.id = new_id();
	net.problem[0] = '\0';
}

static void fail(const char *problem)
{
	snprintf(net.problem, sizeof(net.problem), "%s", problem);
	console_network_leave();
}

void console_network_frame(void)
{
	bool silent = false;

	if (net.role == ROLE_OFF && !net.linked)
		return;
	net.frame++;
	take_datagrams();
	switch (net.role)
	{
		case ROLE_SEARCHING:
			for (int index = net.room_count - 1; index >= 0; index--)
				if (net.frame - net.rooms[index].heard > ROOM_FORGOTTEN)
				{
					memmove(&net.rooms[index], &net.rooms[index + 1], sizeof(net.rooms[0]) * (size_t)(net.room_count - index - 1));
					net.room_count--;
				}
			break;
		case ROLE_JOINING:
			if (net.frame - net.role_frame > JOIN_GIVEN_UP)
				fail("THE ROOM DID NOT ANSWER");
			else if ((net.frame - net.role_frame) % JOIN_FRAMES == 0)
				send_join();
			break;
		case ROLE_IN_ROOM:
			silent = check_members();
			if (member_index(net.id) < 0)
			{
				fail("LEFT OUT OF THE ROOM");
				break;
			}
			if (hosting() && net.frame % BEACON_FRAMES == 0)
				send_beacon();
			send_state();
			/* everyone in: the entries closed soon after (machine 1, player 0's record) */
			if (net.linked && net.machine == 1 && RACE(RACE_1)->entry_frames > ENTRIES_CUT
				&& SCREEN(SCREEN_1)->entrant_count >= net.players)
				RACE(RACE_1)->entry_frames = ENTRIES_CUT;
			break;
		default:
			break;
	}
	net.waiting = carry_link() || (silent && busy());
	if (net.role == ROLE_OFF && net.linked && !busy())
		link_end();    /* (the room left during a race: once it is over) */
}

bool console_network_waiting(void)
{
	return net.waiting;
}

/* ---- the menu's ---- */

void console_network_host(void)
{
	open_network();
	if (!net_open())
	{
		fail("NO NETWORK");
		return;
	}
	net.room = net.id;
	net.role = ROLE_IN_ROOM;
	net.member_count = 1;
	memset(&net.members[0], 0, sizeof(net.members[0]));
	net.members[0].id = net.id;
	snprintf(net.members[0].name, sizeof(net.members[0].name), "%s", console_window_settings.name);
	net.members[0].players = (uint8_t)console_network_players();
	net.list_version = 1;
	net.start_serial = 0;
	net.link_serial = 0;
}

void console_network_search(void)
{
	open_network();
	if (!net_open() || !net_listen_beacons())
	{
		fail("NO NETWORK");
		return;
	}
	net.role = ROLE_SEARCHING;
	net.room_count = 0;
}

void console_network_join(int room)
{
	if (net.role != ROLE_SEARCHING || room < 0 || room >= net.room_count)
		return;
	net.room = net.rooms[room].room;
	net.joining = net.rooms[room].address;
	net.role = ROLE_JOINING;
	net.role_frame = net.frame;
	net.member_count = 0;
	net.list_version = 0;
	memset(net.peers, 0, sizeof(net.peers));
	send_join();
}

void console_network_leave(void)
{
	if (net.role == ROLE_IN_ROOM)
		send_leave();
	if (busy())
	{
		/* a race of this device goes on alone, the others gone from it; the link off once it is over */
		for (int machine = 1; net.linked && machine <= net.machines; machine++)
			net.link_lost[machine - 1] = machine != net.machine;
	}
	else
		link_end();
	net.role = ROLE_OFF;
	net.member_count = 0;
	net.room_count = 0;
	net.start_pending = 0;
	memset(net.peers, 0, sizeof(net.peers));
	net_close();
}

bool console_network_in_room(void)
{
	return net.role == ROLE_IN_ROOM || net.role == ROLE_JOINING;
}

bool console_network_joining(void)
{
	return net.role == ROLE_JOINING;
}

bool console_network_searching(void)
{
	return net.role == ROLE_SEARCHING;
}

bool console_network_hosting(void)
{
	return hosting();
}

bool console_network_linked(void)
{
	return net.linked;
}

int console_network_room_count(void)
{
	return net.room_count;
}

void console_network_room(int room, char *text, size_t size)
{
	snprintf(text, size, "%-8s %d/%d", net.rooms[room].name, net.rooms[room].members, MACHINES);
}

bool console_network_room_full(int room)
{
	return net.rooms[room].members >= MACHINES;
}

int console_network_member_count(void)
{
	return net.member_count;
}

void console_network_member(int index, char *text, size_t size)
{
	snprintf(text, size, "%d %-8s%s", index + 1, net.members[index].name, net.members[index].players == 2 ? " 2P" : "");
}

/* START: two devices or more, none of them in a race */
bool console_network_can_start(void)
{
	if (!hosting() || net.member_count < 2 || busy())
		return false;
	for (int index = 1; index < net.member_count; index++)
	{
		const peer *heard = peer_of(net.members[index].id, false);

		if (!heard || (heard->flags & BUSY) || unheard(net.members[index].id) >= SILENT_FRAMES)
			return false;
	}
	return true;
}

void console_network_start(void)
{
	if (!console_network_can_start())
		return;
	net.start_serial++;
	net.list_version++;
	start_race();
}

bool console_network_start_pending(void)
{
	return net.start_pending != 0;
}

int console_network_players(void)
{
	return players_wanted;
}

void console_network_set_players(int players)
{
	players_wanted = players == 2 ? 2 : 1;
}

int console_network_take_start(void)
{
	const int seats = net.start_pending;

	net.start_pending = 0;
	return seats;
}

const char *console_network_problem(void)
{
	return net.problem[0] ? net.problem : NULL;
}

void console_network_problem_seen(void)
{
	net.problem[0] = '\0';
}

/* ---- the game's link, in place of the board ---- */

/* Each frame, the race records of all the players into 20FC00 (link_receive_records): this device's own two,
   and the others' as their newest datagrams brought them */
void link_receive_records(void)
{
	if (!net.linked)
	{
		original_link_receive_records();
		return;
	}
	for (int slot = 0; slot < net.machines * 2; slot++)
	{
		struct race *copy = RACE(RACE_COPY_1 + (uint32_t)slot * RECORD_SIZE);

		if (slot / 2 + 1 == net.machine)
			memcpy(copy, RACE(slot & 1 ? RACE_2 : RACE_1), RECORD_BYTES);
		else
			memcpy(copy, net.slots[slot], RECORD_BYTES);
	}
}

/* The records go with the port's frame (console_network_frame), not to a link board */
void link_send_records(void)
{
	if (!net.linked)
		original_link_send_records();
}

/* ---- for scripted checks ---- */

void console_network_describe(char *text, size_t size)
{
	const char *const roles[] = { "off", "searching", "joining", "in room" };
	int at = snprintf(text, size, "%s%s, %d members", roles[net.role], hosting() ? " (host)" : "", net.member_count);

	if (net.linked && SCREEN(SCREEN_1)->host_record && at > 0 && (size_t)at < size)
	{
		const uint32_t host = SCREEN(SCREEN_1)->host_record;

		at += snprintf(text + at, size - (size_t)at, ", machine %d of %d, host player %u%s, racers %u, time %u, left %u,"
			" segments", net.machine, net.machines, SCREEN(SCREEN_1)->host_player, SCREEN(SCREEN_1)->is_host ? " (this)" : "",
			RACE(host)->racer_count, RACE(host)->race_time, RACE(host)->time_left);
		for (int slot = 0; slot < net.machines * 2 && at > 0 && (size_t)at < size; slot++)
			at += snprintf(text + at, size - (size_t)at, " %u", RACE(RACE_COPY_1 + (uint32_t)slot * RECORD_SIZE)->segment);
	}
}
