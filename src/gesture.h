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
#include <vector>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/split_member.hpp>

#define STROKE_SIZE 64

class Stroke {
	friend class boost::serialization::access;
    public:
	struct Point {
		double x;
		double y;
		Point operator+(const Point &p) {
			Point sum = { x + p.x, y + p.y };
			return sum;
		}
		Point operator-(const Point &p) {
			Point sum = { x - p.x, y - p.y };
			return sum;
		}
		Point operator*(const double a) {
			Point product = { x * a, y * a };
			return product;
		}
		template<class Archive> void serialize(Archive & ar, const unsigned int version) {
			ar & x; ar & y;
			if (version == 0) {
				double time;
				ar & time;
			}
		}
	};
	
	using PreStroke = std::vector<Point>;

private:
	BOOST_SERIALIZATION_SPLIT_MEMBER()
	template<class Archive> void load(Archive & ar, const unsigned int version) {
		if(version >= 6) {
			unsigned int n;
			ar & n;
			if(n) {
				stroke_t* s = stroke_alloc(n);
				for(unsigned int i = 0; i < n; i++) {
					double x, y;
					ar & x;
					ar & y;
					stroke_add_point(s, x, y);
				}
				stroke_finish(s);
				stroke.reset(s);
			}
			return;
		}
		
		std::vector<Point> ps;
		ar & ps;
		if (ps.size()) {
			stroke_t *s = stroke_alloc(ps.size());
			for (std::vector<Point>::iterator i = ps.begin(); i != ps.end(); ++i)
				stroke_add_point(s, i->x, i->y);
			stroke_finish(s);
			stroke.reset(s);
		}
		if (version == 0) return;
		
		int trigger;
		int button;
		unsigned int modifiers;
		bool timeout;
		
		ar & button;
		if (version >= 2) ar & trigger;
		if (version < 3) return;
		ar & timeout;
		if (version < 5) return;
		ar & modifiers;

	}
	template<class Archive> void save(Archive & ar, __attribute__((unused)) unsigned int version) const {
		unsigned int n = size();
		ar & n;
		for(unsigned int i = 0; i < n; i++) {
			Point p = points(i);
			ar & p.x;
			ar & p.y;
		}
	}
	
	struct stroke_deleter {
		void operator()(stroke_t* s) const { stroke_free(s); }
	};
	
public:
	std::unique_ptr<stroke_t, stroke_deleter> stroke;

	Stroke() : stroke(nullptr, stroke_deleter()) { }
	Stroke(const PreStroke &s);
	Stroke clone() const { Stroke s; if(stroke) s.stroke.reset(stroke_copy(stroke.get())); return s; }

	static Stroke trefoil();
	static int compare(const Stroke&, const Stroke&, double &);
	
	unsigned int size() const { return stroke ? stroke_get_size(stroke.get()) : 0; }
	bool trivial() const { return size() == 0 ; }
	Point points(int n) const { Point p; stroke_get_point(stroke.get(), n, &p.x, &p.y); return p; }
	double time(int n) const { return stroke_get_time(stroke.get(), n); }
};
BOOST_CLASS_VERSION(Stroke, 6)
BOOST_CLASS_VERSION(Stroke::Point, 1)

#endif
