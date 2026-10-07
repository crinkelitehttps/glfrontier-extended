/*
 * gl_atmos.c - atmosphere haze.
 *
 * Near a planet the game builds its atmosphere (sky haze on the ground,
 * the glowing rim seen from space) as 2D shapes, bands between two bezier
 * curves, and puts them at the back of its software depth tree. No GL hook
 * draws them; the GL depth node made for each one remembers the address of
 * the game's node (PRIM_SOFT_NODE), and once the game has filled it in
 * the band is copied out (the cockpit reuses that memory for its next
 * pass, and the colour ids are only valid now). The band is filled when
 * that node is drawn, so it lands in exactly the game's draw order.
 *
 * Band node layout (words, after the 12 byte tree header):
 *   42                            start shape
 *   90  x0 y0 x1 y1 x2 y2 x3 y3   bezier edge
 *   56  x0 y0 x1 y1               straight edge
 *   104 colour_id                 fill (even-odd) and end
 * Coordinates are 3D view pixels: x 0..320, y 0..168 downwards. The copy
 * is the same words from the first edge on, with the fill's colour id
 * replaced by its rgb444 value.
 */
#include "gl_fill2d.h"
#include "gl_scene.h"
#include "m68000.h"
#include "host.h"

#define ADDR_DYN_COLS 0x56 /* dynamic colour table */
#define TREE_HEADER   12   /* z, two child pointers */

#define OP_SHAPE  42
#define OP_LINE   56
#define OP_BEZIER 90
#define OP_FILL   104

#define BAND_OPS_MAX 64
#define ARENA_WORDS  (1 << 16)

static int16_t arena[ARENA_WORDS];
static int arena_used;

void soft_nodes_reset(void)
{
	arena_used = 0;
}

/* Copies the band at p (just past its start word); false if it is not one */
static bool copy_band(uint32_t p, SoftNode *s)
{
	int start = arena_used, n = arena_used;
	for (int guard = 0; guard < BAND_OPS_MAX; guard++)
	{
		int op = STMemory_ReadWord(p);
		int args = op == OP_BEZIER ? 8 : op == OP_LINE ? 4 : op == OP_FILL ? 1 : -1;
		if (args < 0 || n + 1 + args > ARENA_WORDS)
			return false;
		arena[n++] = (int16_t)op;
		for (int i = 0; i < args; i++)
			arena[n++] = (int16_t)STMemory_ReadWord(p + 2 + i * 2);
		p += 2 + args * 2;
		if (op == OP_FILL)
		{
			arena[n - 1] = (int16_t)(STMemory_ReadWord(ADDR_DYN_COLS + arena[n - 1] + 2) & 0xfff);
			arena_used = n;
			s->band = start;
			return true;
		}
	}
	return false;
}

void soft_node_capture(SoftNode *s)
{
	uint32_t node = s->node;
	s->band = -1;
	if (node == 0 || node + TREE_HEADER + 2 >= MEM_SIZE)
		return;
	if (STMemory_ReadWord(node + TREE_HEADER) == OP_SHAPE)
		copy_band(node + TREE_HEADER + 2, s);
}

void draw_soft_node(const void *payload)
{
	const SoftNode *s = payload;
	if (s->band < 0)
		return;

	const int16_t *w = &arena[s->band];
	fill2d_begin();
	for (;;)
	{
		switch (*w++)
		{
		case OP_BEZIER:
			fill2d_bezier(w, 0);
			w += 8;
			break;
		case OP_LINE:
			fill2d_line(w[0], w[1], w[2], w[3], 0);
			w += 4;
			break;
		case OP_FILL:
		{
			int rgb = w[0];
			fill2d_draw(FILL2D_EVEN_ODD, fill2d_solid, &rgb);
			return;
		}
		default:
			return;
		}
	}
}
