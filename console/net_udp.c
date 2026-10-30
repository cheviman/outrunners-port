/**
 *    Name:        net_udp.c
 *    Description: the network's sockets: UDP, non-blocking, on Windows (Winsock), the Switch (libnx's BSD
 *                 sockets) and the rest (POSIX: Linux, macOS, Android)
 *    Version:     0.1.0
 *    Created:     30-10-2026
 *    Modified:    30-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 */
#include "net.h"

#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET net_socket;
#define NO_SOCKET       INVALID_SOCKET
#define close_socket    closesocket
#else
#ifdef __SWITCH__
#include <switch.h>
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int net_socket;
#define NO_SOCKET       (-1)
#define close_socket    close
#endif

static net_socket game_socket = NO_SOCKET, beacon_socket = NO_SOCKET;
static bool started;

/* The system's sockets, once (Winsock, libnx's) */
static bool start(void)
{
	if (started)
		return true;
#ifdef _WIN32
	{
		WSADATA data;

		if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
			return false;
	}
#elif defined(__SWITCH__)
	if (R_FAILED(socketInitializeDefault()))
		return false;
#endif
	started = true;
	return true;
}

/* A UDP socket on 'port' (0: the system's choice), able to broadcast, non-blocking; 'shared' with other games
   on the device (the beacons') */
static net_socket open_socket(uint16_t port, bool shared)
{
	struct sockaddr_in address;
	const int on = 1;
	net_socket opened;

	if (!start())
		return NO_SOCKET;
	opened = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (opened == NO_SOCKET)
		return NO_SOCKET;
	setsockopt(opened, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof(on));
	if (shared)
	{
		setsockopt(opened, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
#if defined(SO_REUSEPORT) && !defined(__SWITCH__)
		setsockopt(opened, SOL_SOCKET, SO_REUSEPORT, (const char *)&on, sizeof(on));    /* (macOS, Linux) */
#endif
	}
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(opened, (const struct sockaddr *)&address, sizeof(address)) != 0)
	{
		close_socket(opened);
		return NO_SOCKET;
	}
#ifdef _WIN32
	{
		u_long non_blocking = 1;

		ioctlsocket(opened, FIONBIO, &non_blocking);
	}
#else
	fcntl(opened, F_SETFL, fcntl(opened, F_GETFL, 0) | O_NONBLOCK);
#endif
	return opened;
}

/* The next datagram on 'from_socket': its size, 0 when none waits */
static size_t receive_on(net_socket from_socket, void *data, size_t room, net_address *from)
{
	for (;;)
	{
		struct sockaddr_in sender;
		socklen_t sender_size = sizeof(sender);
		const int got = (int)recvfrom(from_socket, (char *)data, (int)room, 0, (struct sockaddr *)&sender,
			&sender_size);

		if (got > 0)
		{
			from->ip = ntohl(sender.sin_addr.s_addr);
			from->port = ntohs(sender.sin_port);
			return (size_t)got;
		}
#ifdef _WIN32
		/* (a datagram sent earlier to a port no longer open, or one too large: the next) */
		if (got < 0 && (WSAGetLastError() == WSAECONNRESET || WSAGetLastError() == WSAEMSGSIZE))
			continue;
#else
		if (got < 0 && errno == EINTR)
			continue;
#endif
		return 0;
	}
}

bool net_open(void)
{
	if (game_socket == NO_SOCKET)
		game_socket = open_socket(0, false);
	return game_socket != NO_SOCKET;
}

void net_close(void)
{
	net_stop_beacons();
	if (game_socket != NO_SOCKET)
		close_socket(game_socket);
	game_socket = NO_SOCKET;
}

bool net_send(net_address to, const void *data, size_t size)
{
	struct sockaddr_in address;

	if (game_socket == NO_SOCKET)
		return false;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(to.port);
	address.sin_addr.s_addr = htonl(to.ip);
	return sendto(game_socket, (const char *)data, (int)size, 0, (const struct sockaddr *)&address, sizeof(address))
		== (int)size;
}

size_t net_receive(void *data, size_t room, net_address *from)
{
	return game_socket == NO_SOCKET ? 0 : receive_on(game_socket, data, room, from);
}

bool net_listen_beacons(void)
{
	if (beacon_socket == NO_SOCKET)
		beacon_socket = open_socket(NET_BEACON_PORT, true);
	return beacon_socket != NO_SOCKET;
}

void net_stop_beacons(void)
{
	if (beacon_socket != NO_SOCKET)
		close_socket(beacon_socket);
	beacon_socket = NO_SOCKET;
}

size_t net_receive_beacon(void *data, size_t room, net_address *from)
{
	return beacon_socket == NO_SOCKET ? 0 : receive_on(beacon_socket, data, room, from);
}
