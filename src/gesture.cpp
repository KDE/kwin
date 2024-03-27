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
#include "gesture.h"

#include <cmath>

Stroke::Stroke(const Points &ps)
    : stroke(nullptr, stroke_deleter())
{
    if (ps.size() >= 2) {
        stroke_t *s = stroke_alloc(ps.size());
        for (const auto &t : ps)
            stroke_add_point(s, t.x(), t.y());
        stroke_finish(s);
        stroke.reset(s);
    }
}

bool Stroke::compare(const Stroke &a, const Stroke &b, double &score)
{
    score = 0.0;
    if (!a.stroke || !b.stroke) {
        if (!a.stroke && !b.stroke) {
            score = 1.0;
            return true;
        }
        return false;
    }
    double cost = stroke_compare(a.stroke.get(), b.stroke.get(), nullptr, nullptr);
    if (cost >= stroke_infinity)
        return false;
    score = std::max(1.0 - 2.5 * cost, 0.0);
    return true;
}

double Stroke::min_matching_score()
{
    return 0.7;
}
