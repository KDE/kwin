/*
 * Copyright (c) 2009, Thomas Jaeger <ThJaeger@gmail.com>
 * Copyright (c) 2023, Daniel Kondor <kondor.dani@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE

#include "stroke.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>

const double stroke_infinity = 0.2;
#define EPS 0.000001

struct point {
	double x;
	double y;
	double t;
	double dt;
	double alpha;
};

struct _stroke_t {
	int n;
	int capacity;
	struct point *p;
};

stroke_t *stroke_alloc(int n) {
	assert(n > 0);
	stroke_t *s = malloc(sizeof(stroke_t));
	s->n = 0;
	s->capacity = n;
	s->p = calloc(n, sizeof(struct point));
	return s;
}

void stroke_add_point(stroke_t *s, double x, double y) {
	assert(s->capacity > s->n);
	s->p[s->n].x = x;
	s->p[s->n].y = y;
	s->n++;
}

static inline double angle_difference(double alpha, double beta) {
	double d = alpha - beta;
	if (d < -1.0)
		d += 2.0;
	else if (d > 1.0)
		d -= 2.0;
	return d;
}

void stroke_finish(stroke_t *s) {
	assert(s->capacity > 0);
	s->capacity = -1;

	int n = s->n - 1;
	double total = 0.0;
	s->p[0].t = 0.0;
	for (int i = 0; i < n; i++) {
		total += hypot(s->p[i+1].x - s->p[i].x, s->p[i+1].y - s->p[i].y);
		s->p[i+1].t = total;
	}
	for (int i = 0; i <= n; i++)
		s->p[i].t /= total;
	double minX = s->p[0].x, minY = s->p[0].y, maxX = minX, maxY = minY;
	for (int i = 1; i <= n; i++) {
		if (s->p[i].x < minX) minX = s->p[i].x;
		if (s->p[i].x > maxX) maxX = s->p[i].x;
		if (s->p[i].y < minY) minY = s->p[i].y;
		if (s->p[i].y > maxY) maxY = s->p[i].y;
	}
	double scaleX = maxX - minX;
	double scaleY = maxY - minY;
	double scale = (scaleX > scaleY) ? scaleX : scaleY;
	if (scale < 0.001) scale = 1;
	for (int i = 0; i <= n; i++) {
		s->p[i].x = (s->p[i].x-(minX+maxX)/2)/scale + 0.5;
		s->p[i].y = (s->p[i].y-(minY+maxY)/2)/scale + 0.5;
	}

	for (int i = 0; i < n; i++) {
		s->p[i].dt = s->p[i+1].t - s->p[i].t;
		s->p[i].alpha = atan2(s->p[i+1].y - s->p[i].y, s->p[i+1].x - s->p[i].x)/M_PI;
	}

}

void stroke_free(stroke_t *s) {
	if (s)
		free(s->p);
	free(s);
}

stroke_t *stroke_copy(const stroke_t *stroke) {
	if(!stroke) return NULL;
	stroke_t *s = malloc(sizeof(stroke_t));
	if(!s) return NULL;
	s->p = calloc(stroke->n, sizeof(struct point));
	if(!(s->p)) {
		free(s);
		return NULL;
	}
	s->n = stroke->n;
	s->capacity = s->n;
	memcpy(s->p, stroke->p, s->n * sizeof(struct point));
	return s;
}


int stroke_get_size(const stroke_t *s) { return s->n; }

void stroke_get_point(const stroke_t *s, int n, double *x, double *y) {
	assert(n < s->n);
	if (x)
		*x = s->p[n].x;
	if (y)
		*y = s->p[n].y;
}

double stroke_get_time(const stroke_t *s, int n) {
	assert(n < s->n);
	return s->p[n].t;
}

double stroke_get_angle(const stroke_t *s, int n) {
	assert(n+1 < s->n);
	return s->p[n].alpha;
}

inline static double sqr(double x) { return x*x; }

double stroke_angle_difference(const stroke_t *a, const stroke_t *b, int i, int j) {
	return fabs(angle_difference(stroke_get_angle(a, i), stroke_get_angle(b, j)));
}

static inline void step(const stroke_t *a,
			const stroke_t *b,
			const int N,
			double *dist,
			int *prev_x,
			int *prev_y,
			const int x,
			const int y,
			const double tx,
			const double ty,
			int *k,
			const int x2,
			const int y2)
{
	double dtx = a->p[x2].t - tx;
	double dty = b->p[y2].t - ty;
	if (dtx >= dty * 2.2 || dty >= dtx * 2.2 || dtx < EPS || dty < EPS)
		return;
	(*k)++;

	double d = 0.0;
	int i = x, j = y;
	double next_tx = (a->p[i+1].t - tx) / dtx;
	double next_ty = (b->p[j+1].t - ty) / dty;
	double cur_t = 0.0;

	for (;;) {
		double ad = sqr(angle_difference(a->p[i].alpha, b->p[j].alpha));
		double next_t = next_tx < next_ty ? next_tx : next_ty;
		bool done = next_t >= 1.0 - EPS;
		if (done)
			next_t = 1.0;
		d += (next_t - cur_t)*ad;
		if (done)
			break;
		cur_t = next_t;
		if (next_tx < next_ty)
			next_tx = (a->p[++i+1].t - tx) / dtx;
		else
			next_ty = (b->p[++j+1].t - ty) / dty;
	}
	double new_dist = dist[x*N+y] + d * (dtx + dty);
	if (new_dist != new_dist) abort();

	if (new_dist >= dist[x2*N+y2])
		return;

	prev_x[x2*N+y2] = x;
	prev_y[x2*N+y2] = y;
	dist[x2*N+y2] = new_dist;
}

/* To compare two gestures, we use dynamic programming to minimize (an
 * approximation) of the integral over square of the angle difference among
 * (roughly) all reparametrizations whose slope is always between 1/2 and 2.
 */
double stroke_compare(const stroke_t *a, const stroke_t *b, int *path_x, int *path_y) {
	const int M = a->n;
	const int N = b->n;
	const int m = M - 1;
	const int n = N - 1;

	double* dist = malloc(M * N * sizeof(double));
	int* prev_x  = malloc(M * N * sizeof(int));
	int* prev_y  = malloc(M * N * sizeof(int));
	for (int i = 0; i < m; i++)
		for (int j = 0; j < n; j++)
			dist[i*N+j] = stroke_infinity;
	dist[M*N-1] = stroke_infinity;
	dist[0] = 0.0;

	for (int x = 0; x < m; x++) {
		for (int y = 0; y < n; y++) {
			if (dist[x*N+y] >= stroke_infinity)
				continue;
			double tx  = a->p[x].t;
			double ty  = b->p[y].t;
			int max_x = x;
			int max_y = y;
			int k = 0;

			while (k < 4) {
				if (a->p[max_x+1].t - tx > b->p[max_y+1].t - ty) {
					max_y++;
					if (max_y == n) {
						step(a, b, N, dist, prev_x, prev_y, x, y, tx, ty, &k, m, n);
						break;
					}
					for (int x2 = x+1; x2 <= max_x; x2++)
						step(a, b, N, dist, prev_x, prev_y, x, y, tx, ty, &k, x2, max_y);
				} else {
					max_x++;
					if (max_x == m) {
						step(a, b, N, dist, prev_x, prev_y, x, y, tx, ty, &k, m, n);
						break;
					}
					for (int y2 = y+1; y2 <= max_y; y2++)
						step(a, b, N, dist, prev_x, prev_y, x, y, tx, ty, &k, max_x, y2);
				}
			}
		}
	}
	double cost = dist[M*N-1];
	if (path_x && path_y) {
		if (cost < stroke_infinity) {
			int x = m;
			int y = n;
			int k = 0;
			while (x || y) {
				int old_x = x;
				x = prev_x[x*N+y];
				y = prev_y[old_x*N+y];
				path_x[k] = x;
				path_y[k] = y;
				k++;
			}
		} else {
			path_x[0] = 0;
			path_y[0] = 0;
		}
	}

	free(prev_y);
	free(prev_x);
	free(dist);

	return cost;
}
