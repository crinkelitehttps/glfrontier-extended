/*
 * gl_fill2d.c - coded scanline fill, see gl_fill2d.h.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gl_cockpit.h"
#include "gl_draw.h"
#include "gl_fill2d.h"

#define BEZIER_SEGS   24
/* Every bezier becomes BEZIER_SEGS edges; detailed planets (Earth) have
 * hundreds of beziers */
#define MAX_EDGES     65536
#define MAX_CROSSINGS 1024
/* Cockpit view: how far (view pixels, 89.9 degrees) haze bands that
 * reach the classic view's edges are carried on beyond them */
#define EXTEND        100000.0f

typedef struct
{
	float x0, y0, x1, y1;
	int code;
} Edge;

typedef struct
{
	float x;
	int code;
} Crossing;

static Edge edges[MAX_EDGES];
static int n_edges;
static int row_delta[FILL2D_ROWS];

void fill2d_begin(void)
{
	n_edges = 0;
	memset(row_delta, 0, sizeof(row_delta));
}

void fill2d_line(float x0, float y0, float x1, float y1, int code)
{
	if (n_edges < MAX_EDGES && y0 != y1)
	{
		Edge e = {x0, y0, x1, y1, code};
		edges[n_edges++] = e;
	}
}

void fill2d_bezier(const int16_t w[8], int code)
{
	float p[4][2] = {{w[0], w[1]}, {w[2], w[3]}, {w[4], w[5]}, {w[6], w[7]}};
	float px = p[0][0], py = p[0][1];
	for (int i = 1; i <= BEZIER_SEGS; i++)
	{
		float t = i / (float)BEZIER_SEGS, c = 1.0f - t;
		float a = c * c * c, b = 3 * c * c * t, d = 3 * c * t * t, e = t * t * t;
		float x = a * p[0][0] + b * p[1][0] + d * p[2][0] + e * p[3][0];
		float y = a * p[0][1] + b * p[1][1] + d * p[2][1] + e * p[3][1];
		fill2d_line(px, py, x, y, code);
		px = x;
		py = y;
	}
}

void fill2d_row_code(int row, int code)
{
	if (row < 0)
		row = 0;
	if (row >= FILL2D_ROWS)
		row = FILL2D_ROWS - 1;
	row_delta[row] ^= code;
}

int fill2d_solid(int code, const void *ctx)
{
	(void)code;
	return *(const int *)ctx;
}

static int cmp_crossing(const void *a, const void *b)
{
	float d = ((const Crossing *)a)->x - ((const Crossing *)b)->x;
	return (d > 0) - (d < 0);
}

void fill2d_draw(enum Fill2dRule rule, fill2d_colour_fn colour, const void *ctx)
{
	float ymin = FILL2D_VIEW_H, ymax = 0;
	for (int i = 0; i < n_edges; i++)
	{
		ymin = fminf(ymin, fminf(edges[i].y0, edges[i].y1));
		ymax = fmaxf(ymax, fmaxf(edges[i].y0, edges[i].y1));
	}
	ymin = fmaxf(ymin, 0.0f);
	ymax = fminf(ymax, FILL2D_VIEW_H);
	if (ymin >= ymax)
		return;

	/* base region code of each row: accumulated from the bottom upwards */
	int row_code[FILL2D_ROWS];
	int acc = 0;
	for (int r = FILL2D_ROWS - 1; r >= 0; r--)
	{
		acc ^= row_delta[r];
		row_code[r] = acc;
	}

	mat4 saved_proj = *gd_projection();
	mat4 view = cockpit_view_pixel_projection();
	gd_set_viewport(GD_VP_VIEW3D);
	gd_set_projection(&view);
	gd_push();
	gd_identity();

	/* one span row per screen pixel row */
	int rows = gd_viewport_rect(GD_VP_VIEW3D)[3];
	if (rows < 1)
		rows = 1;
	float step = FILL2D_VIEW_H / (float)rows;
	Crossing xs[MAX_CROSSINGS];
	int last_rgb = -2;
	/* The game clips its haze bands and the ground to the classic view; in
	 * the cockpit, where the view is wider, spans touching an edge carry on
	 * past it. Planets only when they span the whole width (the ground, or
	 * a planet filling the view), not a disc cut by one edge. */
	bool extend = false;
	if (cockpit_active())
	{
		bool left = false, right = false;
		for (int i = 0; i < n_edges && !(left && right); i++)
		{
			const Edge *e = &edges[i];
			left = left || fminf(e->x0, e->x1) <= 0.5f;
			right = right || fmaxf(e->x0, e->x1) >= FILL2D_VIEW_W - 0.5f;
		}
		extend = rule == FILL2D_EVEN_ODD || (left && right);
	}
	for (float y = floorf(ymin / step) * step; y < ymax; y += step)
	{
		float yc = y + step * 0.5f;
		int n = 0;
		for (int i = 0; i < n_edges && n < MAX_CROSSINGS; i++)
		{
			const Edge *e = &edges[i];
			if ((yc >= e->y0 && yc < e->y1) || (yc >= e->y1 && yc < e->y0))
			{
				xs[n].x = e->x0 + (yc - e->y0) * (e->x1 - e->x0) / (e->y1 - e->y0);
				xs[n].code = e->code;
				n++;
			}
		}
		if (n < 2)
			continue;
		qsort(xs, (size_t)n, sizeof(Crossing), cmp_crossing);

		int r = (int)yc;
		int code = row_code[r < 0 ? 0 : (r >= FILL2D_ROWS ? FILL2D_ROWS - 1 : r)];
		int i = 0;
		if (rule == FILL2D_PLANET)
		{
			/* feature edges before the outline still change the code */
			while (i < n && xs[i].code != 0)
				code ^= xs[i++].code;
		}
		for (; i + 1 < n; i++)
		{
			if (rule == FILL2D_EVEN_ODD)
			{
				if (i & 1)
					continue; /* outside, between pairs */
			}
			else
			{
				code ^= xs[i].code;
			}
			float x0 = fmaxf(xs[i].x, 0.0f), x1 = fminf(xs[i + 1].x, FILL2D_VIEW_W);
			int rgb = (x1 > x0) ? colour(code, ctx) : -1;
			if (rgb >= 0)
			{
				if (rgb != last_rgb)
				{
					gd_color3ub((rgb & 0xf00) >> 4, rgb & 0xf0, (rgb & 0xf) << 4);
					last_rgb = rgb;
				}
				float y0 = y, y1 = y + step;
				if (extend)
				{
					if (x0 <= 0.5f)
						x0 = -EXTEND;
					if (x1 >= FILL2D_VIEW_W - 0.5f)
						x1 = FILL2D_VIEW_W + EXTEND;
					if (y0 <= 0.0f)
						y0 = -EXTEND;
					if (y1 >= FILL2D_VIEW_H)
						y1 = FILL2D_VIEW_H + EXTEND;
				}
				gd_rect2(x0, y0, x1, y1);
			}
			/* planet rule: the next outline crossing ends the row */
			if (rule == FILL2D_PLANET && xs[i + 1].code == 0)
				break;
		}
	}

	gd_pop();
	gd_set_projection(&saved_proj);
}
