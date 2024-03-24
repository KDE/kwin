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

#include <algorithm>
#include <math.h>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/export.hpp>

BOOST_CLASS_EXPORT(Stroke)

Stroke::Stroke(const PreStroke &ps) : stroke(nullptr, stroke_deleter()) {
	if (ps.size() >= 2) {
		stroke_t *s = stroke_alloc(ps.size());
		for (const auto& t : ps)
			stroke_add_point(s, t.x, t.y);
		stroke_finish(s);
		stroke.reset(s);
	}
}

int Stroke::compare(const Stroke& a, const Stroke& b, double &score) {
	score = 0.0;
	if (!a.stroke || !b.stroke) {
		if (!a.stroke && !b.stroke) {
			score = 1.0;
			return 1;
		}
		return -1;
	}
	double cost = stroke_compare(a.stroke.get(), b.stroke.get(), nullptr, nullptr);
	if (cost >= stroke_infinity)
		return -1;
	score = std::max(1.0 - 2.5*cost, 0.0);
	return score > 0.7;
}

/*
Stroke Stroke::trefoil() {
	PreStroke s;
	const unsigned int n = 40;
	s.reserve(n);
	for (unsigned int i = 0; i<=n; i++) {
		double phi = M_PI*(-4.0*i/n)-2.7;
		double r = exp(1.0 + sin(6.0*M_PI*i/n)) + 2.0;
		s.add(Point{(float)(r*cos(phi)), (float)(r*sin(phi))});
	}
	return Stroke(s);
}
*/

