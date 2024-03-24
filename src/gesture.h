/*
 * Copyright (c) 2008-2009, Thomas Jaeger <ThJaeger@gmail.com>
 * Copyright (c) 2020-2023, Daniel Kondor <kondor.dani@gmail.com>
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
#ifndef __GESTURE_H__
#define __GESTURE_H__

#include "stroke.h"

#include <memory>
#include <vector>

#define STROKE_SIZE 64

class Stroke
{
public:
    struct Point {
        double x;
        double y;
        Point operator+(const Point &p)
        {
            Point sum = {x + p.x, y + p.y};
            return sum;
        }
        Point operator-(const Point &p)
        {
            Point sum = {x - p.x, y - p.y};
            return sum;
        }
        Point operator*(const double a)
        {
            Point product = {x * a, y * a};
            return product;
        }
    };

    using PreStroke = std::vector<Point>;

private:
    struct stroke_deleter {
        void operator()(stroke_t *s) const
        {
            stroke_free(s);
        }
    };

public:
    std::unique_ptr<stroke_t, stroke_deleter> stroke;

    Stroke()
        : stroke(nullptr, stroke_deleter())
    {
    }
    Stroke(const PreStroke &s);
    Stroke clone() const
    {
        Stroke s;
        if (stroke)
            s.stroke.reset(stroke_copy(stroke.get()));
        return s;
    }

    static int compare(const Stroke &, const Stroke &, double &);

    unsigned int size() const
    {
        return stroke ? stroke_get_size(stroke.get()) : 0;
    }
    bool trivial() const
    {
        return size() == 0;
    }
    Point points(int n) const
    {
        Point p;
        stroke_get_point(stroke.get(), n, &p.x, &p.y);
        return p;
    }
    double time(int n) const
    {
        return stroke_get_time(stroke.get(), n);
    }
};

#endif
