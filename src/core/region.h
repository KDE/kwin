/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"

#include <QList>
#include <QRegion>
#include <QSpan>

namespace KWin
{

/**
 * The Region type represents a collection of rectangles to specify the area for drawing or clipping.
 *
 * The Region maintains rectangles so they are sorted first by the y coordinate then by the x coordinate.
 * In addition to that the rectangles are split to form bands. A band is a collection of rectangles
 * that share the same top and bottom coordinates. Rectangles in the same band cannot touch or overlap.
 *
 * For example, the following region with two rectangles
 *
 *     -----------------
 *     |               |
 *     |               |
 *     |               |   -----------------
 *     |               |   |               |
 *     |               |   |               |
 *     |               |   |               |
 *     -----------------   |               |
 *                         |               |
 *                         |               |
 *                         -----------------
 *
 * will be stored as follows
 *
 *     -----------------
 *     |               |
 *     |               |
 *     |---------------|
 *     |---------------|   -----------------
 *     |               |   |               |
 *     |               |   |               |
 *     |               |   |               |
 *     -----------------   |---------------|
 *                         |---------------|
 *                         |               |
 *                         |               |
 *                         -----------------
 */
class KWIN_EXPORT Region
{
public:
    Region();
    Region(int x, int y, int width, int height);
    Region(const Rect &rect);
    Region(const Region &other);
    Region(Region &&other);
    explicit Region(const QRegion &region);

    Rect boundingRect() const;
    bool isEmpty() const;

    bool contains(const Rect &rect) const;
    bool contains(const QPoint &point) const;

    bool intersects(const Rect &rect) const;
    bool intersects(const Region &other) const;

    Region united(const Region &other) const;
    Region united(const Rect &other) const;
    Region subtracted(const Region &other) const;
    Region subtracted(const Rect &other) const;
    Region xored(const Region &other) const;
    Region xored(const Rect &other) const;
    Region intersected(const Region &other) const;
    Region intersected(const Rect &other) const;

    void translate(int x, int y);
    void translate(const QPoint &offset);
    Region translated(int x, int y) const;
    Region translated(const QPoint &offset) const;

    Region scaledAndRoundedOut(qreal scale) const;
    Region scaledAndRoundedOut(qreal xScale, qreal yScale) const;

    QSpan<const Rect> rects() const;

    bool operator==(const Region &other) const;

    Region &operator=(const Region &other);
    Region &operator=(Region &&other);

    Region operator|(const Region &other) const;
    Region operator|(const Rect &other) const;
    Region operator+(const Region &other) const;
    Region operator+(const Rect &other) const;
    Region operator-(const Region &other) const;
    Region operator-(const Rect &other) const;
    Region operator&(const Region &other) const;
    Region operator&(const Rect &other) const;
    Region operator^(const Region &other) const;
    Region operator^(const Rect &other) const;

    Region &operator|=(const Region &other);
    Region &operator|=(const Rect &other);
    Region &operator+=(const Region &other);
    Region &operator+=(const Rect &other);
    Region &operator-=(const Region &other);
    Region &operator-=(const Rect &other);
    Region &operator&=(const Region &other);
    Region &operator&=(const Rect &other);
    Region &operator^=(const Region &other);
    Region &operator^=(const Rect &other);

    explicit operator QRegion() const;

    static Region fromSortedRects(const QList<Rect> &rects);
    static Region fromUnsortedRects(const QList<Rect> &rects);
    static Region fromRectsSortedByY(const QList<Rect> &rects);

    static Region infinite();

private:
    struct BandRef
    {
        qsizetype start = 0;
        qsizetype end = 0;
    };

    qsizetype bandByY(int y) const;

    void assignSortedRects(const QList<Rect> &rects);
    void assignRectsSortedByY(const QList<Rect> &rects);
    void computeBounds();

    void unite(QSpan<const Rect> left, QSpan<const Rect> right);
    void subtract(QSpan<const Rect> left, QSpan<const Rect> right);
    void exclusiveOr(QSpan<const Rect> left, QSpan<const Rect> right);
    void intersect(QSpan<const Rect> left, QSpan<const Rect> right);

    BandRef sliceBand(QSpan<const Rect> rects, int top, int bottom, const BandRef &previousBand);
    BandRef mergeBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom, const BandRef &previousBand);
    BandRef subtractBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom, const BandRef &previousBand);
    BandRef xorBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom, const BandRef &previousBand);
    BandRef intersectBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom, const BandRef &previousBand);
    BandRef organizeBand(QSpan<const Rect> rects, int top, int bottom, const BandRef &previousBand);
    BandRef coalesceBands(const BandRef &previous, const BandRef &current);
    void appendRects(QSpan<const Rect> rects);

    QList<Rect> m_rects;
    Rect m_bounds;
};

inline Region::Region()
{
}

inline Region::Region(int x, int y, int width, int height)
    : m_bounds(x, y, width, height)
{
}

inline Region::Region(const Rect &rect)
    : m_bounds(rect)
{
}

inline Region::Region(const Region &other)
    : m_rects(other.m_rects)
    , m_bounds(other.m_bounds)
{
}

inline Region::Region(Region &&other)
    : m_rects(std::exchange(other.m_rects, {}))
    , m_bounds(std::exchange(other.m_bounds, {}))
{
}

inline Region Region::infinite()
{
    return Region(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
}

inline void Region::translate(int x, int y)
{
    translate(QPoint(x, y));
}

inline Region Region::translated(int x, int y) const
{
    return translated(QPoint(x, y));
}

inline Region Region::scaledAndRoundedOut(qreal scale) const
{
    return scaledAndRoundedOut(scale, scale);
}

inline Region Region::united(const Rect &other) const
{
    return united(Region(other));
}

inline Region Region::subtracted(const Rect &other) const
{
    return subtracted(Region(other));
}

inline Region Region::xored(const Rect &other) const
{
    return xored(Region(other));
}

inline Region Region::intersected(const Rect &other) const
{
    return intersected(Region(other));
}

inline bool Region::operator==(const Region &other) const
{
    return m_bounds == other.m_bounds && m_rects == other.m_rects;
}

inline Region &Region::operator=(const Region &other)
{
    m_rects = other.m_rects;
    m_bounds = other.m_bounds;
    return *this;
}

inline Region &Region::operator=(Region &&other)
{
    m_rects = std::exchange(other.m_rects, {});
    m_bounds = std::exchange(other.m_bounds, {});
    return *this;
}

inline Region Region::operator|(const Region &other) const
{
    return united(other);
}

inline Region Region::operator|(const Rect &other) const
{
    return united(other);
}

inline Region Region::operator+(const Region &other) const
{
    return united(other);
}

inline Region Region::operator+(const Rect &other) const
{
    return united(other);
}

inline Region Region::operator-(const Region &other) const
{
    return subtracted(other);
}

inline Region Region::operator-(const Rect &other) const
{
    return subtracted(other);
}

inline Region Region::operator&(const Region &other) const
{
    return intersected(other);
}

inline Region Region::operator&(const Rect &other) const
{
    return intersected(other);
}

inline Region Region::operator^(const Region &other) const
{
    return xored(other);
}

inline Region Region::operator^(const Rect &other) const
{
    return xored(other);
}

inline Region &Region::operator|=(const Region &other)
{
    *this = united(other);
    return *this;
}

inline Region &Region::operator|=(const Rect &other)
{
    *this = united(other);
    return *this;
}

inline Region &Region::operator+=(const Region &other)
{
    *this = united(other);
    return *this;
}

inline Region &Region::operator+=(const Rect &other)
{
    *this = united(other);
    return *this;
}

inline Region &Region::operator-=(const Region &other)
{
    *this = subtracted(other);
    return *this;
}

inline Region &Region::operator-=(const Rect &other)
{
    *this = subtracted(other);
    return *this;
}

inline Region &Region::operator&=(const Region &other)
{
    *this = intersected(other);
    return *this;
}

inline Region &Region::operator&=(const Rect &other)
{
    *this = intersected(other);
    return *this;
}

inline Region &Region::operator^=(const Region &other)
{
    *this = xored(other);
    return *this;
}

inline Region &Region::operator^=(const Rect &other)
{
    *this = xored(other);
    return *this;
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const Region &region);

KWIN_EXPORT QDataStream &operator>>(QDataStream &stream, Region &region);
KWIN_EXPORT QDataStream &operator<<(QDataStream &stream, const Region &region);

} // namespace KWin
