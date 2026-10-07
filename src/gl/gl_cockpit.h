/*
 * gl_cockpit.h - the 3D cockpit view: the flight view seen from a seat in
 * the ship, looking around with head tracking (headtrack.c).
 *
 * HOW IT WORKS
 *   The world fills the window with a real perspective and the head's
 *   rotation. The game's own 2D drawing over its 3D view (HUD, target
 *   brackets, planets and atmosphere, which the game draws as 2D shapes)
 *   lives in "view pixels": x 0..320, y 0..168 downwards, each pixel a
 *   direction through the game's original projection. In the cockpit those
 *   pixels are put back on that direction, far away and turning with the
 *   world, so they stay on what they mark (a collimated display). The
 *   control panel (screen rows 168..200) is a texture on the dashboard,
 *   which also moves with the head's position. Mouse positions are mapped
 *   back through the same surfaces.
 *
 *   Only in the front flight view, when the cockpit is enabled and the free
 *   camera is off; everything else keeps the classic letterboxed layout.
 */
#ifndef GL_COCKPIT_H
#define GL_COCKPIT_H

#include <stdbool.h>

#include "gl_api.h"
#include "gl_math.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* The game's own projection of its 3D view (gl_screen.c's classic layout) */
#define CLASSIC_FOV    36.5f /* vertical, degrees */
#define CLASSIC_ASPECT 1.9f

/* The emulated screen and its texture (gl_screen.c) */
#define GL_SCREEN_W     320
#define GL_SCREEN_H     200
#define GL_SCREEN_TEX_W 512
#define GL_SCREEN_TEX_H 256
void gl_screen_upload(void);
GLuint gl_screen_texture(void);

/* Settings */
extern bool cockpit_enabled;
extern int cockpit_fov;      /* vertical field of view, degrees */
extern bool headtrack_enabled;
extern int headtrack_port;

/* With the GL context (Screen_Init / Screen_UnInit) */
void cockpit_init(void);
void cockpit_shutdown(void);

/* Once per frame, before drawing: polls the head tracker and decides
 * whether this frame is drawn as the cockpit. */
bool cockpit_begin_frame(int window_w, int window_h);
/* What the last cockpit_begin_frame decided */
bool cockpit_active(void);

/* Projection for the 3D scene: lens * head rotation */
mat4 cockpit_world_projection(void);
/* The same, split: the lens alone and the head rotation (view matrix) */
void cockpit_world_split(mat4 *lens, mat4 *head_rotation);

/* Projection for 2D drawing in view pixels (x 0..320, y 0..168 down, z 0),
 * cockpit or not, to be used with the GD_VP_VIEW3D viewport */
mat4 cockpit_view_pixel_projection(void);

/* Queues the HUD plane, the cockpit frame and the panel */
void cockpit_draw(void);

/* Window pixel (drawable pixels, y down) -> emulated screen pixel
 * (320x200). False if it points at nothing the game draws on. */
bool cockpit_window_to_screen(int wx, int wy, int *sx, int *sy);

#ifdef __cplusplus
}
#endif

#endif /* GL_COCKPIT_H */
