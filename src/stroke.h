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

#include <QList>
#include <QPointF>

#include <memory> // std::unique_ptr

struct stroke_t;

struct stroke_deleter
{
    void operator()(stroke_t *s) const;
};

class Stroke
{
public:
    std::unique_ptr<stroke_t, stroke_deleter> stroke;

    Stroke()
        : stroke(nullptr, stroke_deleter())
    {
    }
    Stroke(const QList<QPointF> &s);
    Stroke clone() const;

    static bool compare(const Stroke &, const Stroke &, double &score_out);
    static double min_matching_score();

    unsigned int size() const;
    bool trivial() const
    {
        return size() == 0;
    }
    QPointF pointAt(int n) const;
    double time(int n) const;
};

#endif
