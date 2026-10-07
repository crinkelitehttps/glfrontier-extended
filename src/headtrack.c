/*
 * headtrack.c - see headtrack.h. Ported from the F-19 project's
 * native/src/host/headtrack.cpp.
 */
#include <math.h>
#include <string.h>

#include <SDL.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET Socket;
#define close_socket closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int Socket;
#define INVALID_SOCKET (-1)
#define close_socket   close
#endif

#include "headtrack.h"
#include "main.h"

static Socket sock = INVALID_SOCKET;
static HeadPose raw, centre;
static Uint64 last_ticks;
static bool receiving;

bool headtrack_open(int port)
{
	if (sock != INVALID_SOCKET)
		return true;
#ifdef _WIN32
	static bool wsa;
	if (!wsa)
	{
		WSADATA wd;
		if (WSAStartup(MAKEWORD(2, 2), &wd) != 0)
			return false;
		wsa = true;
	}
#endif
	Socket s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET)
		return false;
#ifdef _WIN32
	u_long nonblocking = 1;
	bool ok = ioctlsocket(s, FIONBIO, &nonblocking) == 0;
#else
	bool ok = fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK) == 0;
#endif
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short)port);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (!ok || bind(s, (struct sockaddr *)&sa, sizeof sa) < 0)
	{
		log_printf("Head tracking: can't listen on UDP port %d\n", port);
		close_socket(s);
		return false;
	}
	sock = s;
	log_printf("Head tracking: listening for OpenTrack on UDP port %d\n", port);
	return true;
}

void headtrack_close(void)
{
	if (sock != INVALID_SOCKET)
		close_socket(sock);
	sock = INVALID_SOCKET;
	receiving = false;
	memset(&raw, 0, sizeof raw);
}

HeadPose headtrack_poll(void)
{
	HeadPose zero = {0};
	if (sock == INVALID_SOCKET)
		return zero;

	double d[6];
	int n;
	while ((n = (int)recv(sock, (char *)d, sizeof d, 0)) >= 0)
	{
		if (n != (int)sizeof d)
			continue;
		bool ok = true;
		for (int i = 0; i < 6; i++)
			ok = ok && isfinite(d[i]);
		if (!ok)
			continue;
		const float deg = 3.14159265f / 180.0f;
		raw.yaw = (float)d[3] * deg;
		raw.pitch = (float)d[4] * deg;
		raw.roll = (float)d[5] * deg;
		raw.x = (float)d[0];
		raw.y = (float)d[1];
		raw.z = (float)d[2];
		last_ticks = SDL_GetTicks64();
		if (!receiving)
			log_printf("Head tracking: receiving\n");
		receiving = true;
	}
	/* Back to straight ahead when the tracker stops sending */
	if (receiving && SDL_GetTicks64() - last_ticks > 1000)
	{
		memset(&raw, 0, sizeof raw);
		receiving = false;
		log_printf("Head tracking: no data\n");
	}
	if (!receiving)
		return zero;

	HeadPose p = raw;
	p.yaw -= centre.yaw;
	p.pitch -= centre.pitch;
	p.roll -= centre.roll;
	p.x -= centre.x;
	p.y -= centre.y;
	p.z -= centre.z;
	return p;
}

void headtrack_recenter(void)
{
	centre = receiving ? raw : (HeadPose){0};
}

bool headtrack_receiving(void)
{
	return receiving;
}
