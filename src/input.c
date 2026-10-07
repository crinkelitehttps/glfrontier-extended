/*
 * input.c - keyboard queue and mouse state, read by the game through host
 * calls.
 */
#include <SDL.h>

#include "input.h"
#include "m68000.h"
#include "main.h"
#include "renderer.h"

CINPUT input;

/* Host call: fills the game's mouse structures (big endian words)
 *   A7+2: motion x, motion y, buttons
 *   A7+6: absolute x, y in 320x200 screen coordinates */
void Call_GetMouseInput(void)
{
	unsigned long params = GetReg(REG_A7) - SIZE_WORD;
	short *mouse_mov = (short *)(STRam + STMemory_ReadLong(params + SIZE_WORD));
	short *mouse_abs = (short *)(STRam + STMemory_ReadLong(params + SIZE_WORD + SIZE_LONG));

	/* SDL occasionally reports a huge jump (e.g. when grabbing the mouse) */
	if (abs(input.motion_x) > 100 || abs(input.motion_y) > 100)
		input.motion_x = input.motion_y = 0;

	mouse_mov[0] = SDL_SwapBE16(SDL_SwapBE16(mouse_mov[0]) + input.motion_x);
	mouse_mov[1] = SDL_SwapBE16(SDL_SwapBE16(mouse_mov[1]) + input.motion_y);
	mouse_mov[2] = SDL_SwapBE16(0xf8 | input.cur_mousebut_state);

	/* window pixels -> game screen (letterboxed game area, or the cockpit) */
	int gx, gy;
	if (Screen_WindowToGame(input.abs_x, input.abs_y, &gx, &gy))
	{
		mouse_abs[0] = SDL_SwapBE16(gx);
		mouse_abs[1] = SDL_SwapBE16(gy);
	}

	input.motion_x = input.motion_y = 0;
}

/* Host call: next queued ST scancode in D0, or 0 */
void Call_GetKeyboardEvent(void)
{
	if (input.buf_head != input.buf_tail)
	{
		SetReg(REG_D0, input.key_buf[input.buf_head++]);
		input.buf_head %= SIZE_KEYBUF;
	}
	else
	{
		SetReg(REG_D0, 0);
	}
}

void Input_PressSTKey(unsigned char scancode, BOOL press)
{
	if (!press)
		scancode |= 0x80;
	input.key_buf[input.buf_tail++] = scancode;
	input.buf_tail %= SIZE_KEYBUF;
}

/* While the right button is held the game steers with the mouse, so
 * capture it */
static void update_mouse_grab(void)
{
	SDL_SetRelativeMouseMode((input.cur_mousebut_state & 0x1) ? SDL_TRUE : SDL_FALSE);
}

void Input_MousePress(int sdl_button)
{
	if (sdl_button == SDL_BUTTON_RIGHT)
		input.cur_mousebut_state |= 0x1;
	else if (sdl_button == SDL_BUTTON_LEFT)
		input.cur_mousebut_state |= 0x2;
	else
		return;
	update_mouse_grab();
}

void Input_MouseRelease(int sdl_button)
{
	if (sdl_button == SDL_BUTTON_RIGHT)
		input.cur_mousebut_state &= ~0x1;
	else if (sdl_button == SDL_BUTTON_LEFT)
		input.cur_mousebut_state &= ~0x2;
	else
		return;
	update_mouse_grab();
}
