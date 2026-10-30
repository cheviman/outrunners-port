/**
 *    Name:        net.h
 *    Description: the network's sockets (net_udp.c): UDP datagrams to and from the other devices, and the rooms'
 *                 beacons
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 */
#ifndef ORUNNERS_CONSOLE_NET_H
#define ORUNNERS_CONSOLE_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_BEACON_PORT   27720          /* where the rooms' beacons go, every device's on the network */
#define NET_BROADCAST     0xFFFFFFFFu    /* 255.255.255.255: every device on the local network */
#define NET_LOOPBACK      0x7F000001u    /* 127.0.0.1: this one (a second game on the same computer) */

/* A device's address: its IPv4 address (a.b.c.d as a << 24 | b << 16 | c << 8 | d) and UDP port */
typedef struct
{
	uint32_t ip;
	uint16_t port;
} net_address;

/* The game's socket, on a port of the system's choosing (every datagram but the beacons comes and goes
   there): false where the device has no network */
bool net_open(void);
void net_close(void);

/* A datagram to 'to' (its beacons to NET_BROADCAST too): false when it could not go */
bool net_send(net_address to, const void *data, size_t size);

/* The next datagram that came to the game's socket, into 'data' (at most 'room' bytes), and who sent it: its
   size, 0 when none waits */
size_t net_receive(void *data, size_t room, net_address *from);

/* The beacons' socket, on NET_BEACON_PORT, shared with other games on the same device: while a player looks
   for rooms */
bool net_listen_beacons(void);
void net_stop_beacons(void);
size_t net_receive_beacon(void *data, size_t room, net_address *from);

#endif
