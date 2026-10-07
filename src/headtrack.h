/*
 * headtrack.h - head tracking from OpenTrack's "UDP over network" output.
 *
 * Each datagram is six little-endian doubles: x, y, z (cm), yaw, pitch,
 * roll (degrees). Signs as OpenTrack sends them by default: yaw right +,
 * pitch up +, roll right ear down +, x right +, y up +, z back + (flip axes
 * in OpenTrack's Options > Output if one feels reversed).
 *
 * The pose moves the cockpit view (cockpit.c): rotation turns the camera,
 * position moves the eye inside the cockpit only.
 */
#ifndef HEADTRACK_H
#define HEADTRACK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HEADTRACK_DEFAULT_PORT 4242

typedef struct
{
	float yaw, pitch, roll; /* radians */
	float x, y, z;          /* cm */
} HeadPose;

/* Listens on 127.0.0.1:port; false if the socket can't be bound */
bool headtrack_open(int port);
void headtrack_close(void);
/* Drains pending datagrams; returns the latest pose, minus the recentred
 * one. Zero pose when nothing has arrived for a second (tracker stopped). */
HeadPose headtrack_poll(void);
/* Makes the current pose straight ahead */
void headtrack_recenter(void);
/* Data arrived within the last second */
bool headtrack_receiving(void);

#ifdef __cplusplus
}
#endif

#endif /* HEADTRACK_H */
