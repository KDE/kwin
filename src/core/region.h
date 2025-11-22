/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"

#include <QList>
#include <QSpan>

namespace KWin
{

class KWIN_EXPORT Region
{
public:
    Region();
    Region(int x, int y, int width, int height);
    explicit Region(const Rect &rect);

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

    Region scaledAndRounded(qreal scale) const;
    Region scaledAndRounded(qreal xScale, qreal yScale) const;
    Region scaledAndRoundedOut(qreal scale) const;
    Region scaledAndRoundedOut(qreal xScale, qreal yScale) const;

    QSpan<const Rect> rects() const;

    bool operator==(const Region &other) const;

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

    static Region fromSortedRects(const QList<Rect> &rects);
    static Region fromUnsortedRects(const QList<Rect> &rects);
    static Region fromRectsSortedByY(const QList<Rect> &rects);

private:
    struct BandReference
    {
        int size() const
        {
            return end - start;
        }

        int start = 0;
        int end = 0;
    };

    qsizetype bandByY(int y) const;

    void assignSortedRects(const QList<Rect> &rects);
    void assignRectsSortedByY(const QList<Rect> &rects);
    void computeExtents();

    void unite(QSpan<const Rect> left, QSpan<const Rect> right);
    void subtract(QSpan<const Rect> left, QSpan<const Rect> right);
    void exclusiveOr(QSpan<const Rect> left, QSpan<const Rect> right);
    void intersect(QSpan<const Rect> left, QSpan<const Rect> right);

    BandReference appendBand(QSpan<const Rect> rects, int top, int bottom);
    BandReference mergeBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom);
    BandReference sliceBand(QSpan<const Rect> rects, int top, int bottom);
    BandReference subtractBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom);
    BandReference exclusiveOrBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom);
    BandReference intersectBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom);
    BandReference coalesceBands(const BandReference &previous, const BandReference &current);

    QList<Rect> m_rects;
    Rect m_extents;
};

inline void Region::translate(int x, int y)
{
    translate(QPoint(x, y));
}

inline Region Region::translated(int x, int y) const
{
    return translated(QPoint(x, y));
}

inline Region Region::scaledAndRounded(qreal scale) const
{
    return scaledAndRounded(scale, scale);
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
    return m_extents == other.m_extents && m_rects == other.m_rects;
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
