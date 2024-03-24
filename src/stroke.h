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
#ifndef __STROKE_H__
#define __STROKE_H__

#ifdef  __cplusplus
extern "C" {
#endif

struct _stroke_t;

typedef struct _stroke_t stroke_t;

stroke_t *stroke_alloc(int n);
void stroke_add_point(stroke_t *stroke, double x, double y);
void stroke_finish(stroke_t *stroke);
void stroke_free(stroke_t *stroke);
stroke_t *stroke_copy(const stroke_t *stroke);

int stroke_get_size(const stroke_t *stroke);
void stroke_get_point(const stroke_t *stroke, int n, double *x, double *y);
double stroke_get_time(const stroke_t *stroke, int n);
double stroke_get_angle(const stroke_t *stroke, int n);
double stroke_angle_difference(const stroke_t *a, const stroke_t *b, int i, int j);

double stroke_compare(const stroke_t *a, const stroke_t *b, int *path_x, int *path_y);

extern const double stroke_infinity;

#ifdef  __cplusplus
}
#endif
#endif
