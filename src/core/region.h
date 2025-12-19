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

/*!
 * The Region type represents a collection of rectangles to specify the area for drawing or clipping.
 *
 * The Region stores rectangles in the y-x lexicographical order. In other words, the rectangles
 * are sorted by the top coordinate first, from top to bottom. If two rectangles have the same top
 * coordinate, then they are also sorted by the left coordinate, from left to right.
 *
 * The rectangles are split to form bands. A band is a collection of rectangles that share the same
 * top and bottom coordinates. Rectangles in the same band cannot touch or overlap. For example, the
 * following region with two rectangles
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
    /*!
     * Constructs an empty region.
     */
    Region();

    /*!
     * Constructs a region with the given (\a x, \a y, \a width, and \a height) rectangle.
     */
    Region(int x, int y, int width, int height);

    /*!
     * Constructs a region with the given \a rect rectangle.
     */
    Region(const Rect &rect);

    /*!
     * Constructs a copy of the given \a other region.
     */
    Region(const Region &other);

    /*!
     * Move-constructs a region from the given \a other region. After the call, the \a other is empty.
     */
    Region(Region &&other);

    /*!
     * Constructs a region from the given QRegion \a region.
     */
    explicit Region(const QRegion &region);

    /*!
     * Returns the bounding rectangle for this region. If the region is empty, this will return an empty
     * rectangle.
     */
    Rect boundingRect() const;

    /*!
     * Returns \c true if the region is empty; otherwise returns \c false.
     */
    bool isEmpty() const;

    /*!
     * Returns \c true if the \a rect is completely inside this region; otherwise returns \c false.
     */
    bool contains(const Rect &rect) const;

    /*!
     * Returns \c true if the specified \a point is inside this region; otherwise returns \c false.
     */
    bool contains(const QPoint &point) const;

    /*!
     * \overload
     *
     * Returns \c true if the given \a rect and this region overlap; otherwise returns \c false.
     */
    bool intersects(const Rect &rect) const;

    /*!
     * Returns \c true if the given \a other region and this region overlap; otherwise returns \c false.
     */
    bool intersects(const Region &other) const;

    /*!
     * Returns a region that is the union of this region and the given \a other region.
     */
    Region united(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the union of this region and the given \a other rectangle.
     */
    Region united(const Rect &other) const;

    /*!
     * Returns a region that is the \a other region subtracted from this region.
     */
    Region subtracted(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the \a other rectangle subtracted from this region.
     */
    Region subtracted(const Rect &other) const;

    /*!
     * Returns a region that is the exclusive or of this region and the given \a other region.
     */
    Region xored(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the exclusive or of this region and the given \a other rectangle.
     */
    Region xored(const Rect &other) const;

    /*!
     * Returns a region that is the intersection of this region and the given \a other region.
     */
    Region intersected(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the intersection of this region and the given \a other rectangle.
     */
    Region intersected(const Rect &other) const;

    /*!
     * \overload
     *
     * Shifts the region by the \a x amount along the X axis, and the \a y amount along the Y axis.
     */
    void translate(int x, int y);

    /*!
     * Shifts the region by the \a{offset}\e{.x()} amount along the X axis, and the \a{offset}\e{.y()}
     * amount along the Y axis.
     */
    void translate(const QPoint &offset);

    /*!
     * \overload
     *
     * Returns a copy of this region that is shifted by the \a x amount along the X axis, and the
     * \a y amount along the Y axis.
     */
    Region translated(int x, int y) const;

    /*!
     * Returns a copy of this region that is shifted by the \a{offset}\e{.x()} amount along the X
     * axis, and the \a{offset}\e{.y()} amount along the Y axis.
     */
    Region translated(const QPoint &offset) const;

    /*!
     * \overload
     *
     * Returns a copy of this region that is scaled by the \a scale factor along both the X and Y
     * axis and then rounded out. Equivalent to scaledAndRoundedOut(scale, scale).
     */
    Region scaledAndRoundedOut(qreal scale) const;

    /*!
     * Returns a copy of this region that is scaled by the \a xScale factor along the X axis and
     * the \a yScale along the Y axis and then rounded out.
     */
    Region scaledAndRoundedOut(qreal xScale, qreal yScale) const;

    /*!
     * Returns the rectangles that this region is made of.
     */
    QSpan<const Rect> rects() const;

    /*!
     * Returns \c true if this region is equal to the \a other region; otherwise returns \c false.
     */
    bool operator==(const Region &other) const;

    /*!
     * Copy-assigns the \a other region to this region.
     */
    Region &operator=(const Region &other);

    /*!
     * Move-assigns the \a other region to this region.
     */
    Region &operator=(Region &&other);

    /*!
     * Returns a region that is the union of this region and the given \a other region. Equivalent
     * to united() and operator+().
     *
     * \sa united(), operator|=()
     */
    Region operator|(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the union of this region and the given \a other rectangle. Equivalent
     * to united() and operator+().
     *
     * \sa united(), operator|=()
     */
    Region operator|(const Rect &other) const;

    /*!
     * Returns a region that is the union of this region and the given \a other region. Equivalent
     * to united() and operator|().
     *
     * \sa united(), operator+=()
     */
    Region operator+(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the union of this region and the given \a other rectangle. Equivalent
     * to united() and operator|().
     *
     * \sa united(), operator+=()
     */
    Region operator+(const Rect &other) const;

    /*!
     * Returns a region that is the \a other region subtracted from this region. Equivalent to subtracted().
     *
     * \sa subtracted(), operator-=()
     */
    Region operator-(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the \a other rectangle subtracted from this region. Equivalent to subtracted().
     *
     * \sa subtracted(), operator-=()
     */
    Region operator-(const Rect &other) const;

    /*!
     * Returns a region that is the intersection of this region and the given \a other region. Equivalent
     * to intersected().
     *
     * \sa intersected(), operator&=()
     */
    Region operator&(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the intersection of this region and the given \a other rectangle. Equivalent
     * to intersected().
     *
     * \sa intersected(), operator&=()
     */
    Region operator&(const Rect &other) const;

    /*!
     * Returns a region that is the exclusive or of this region and the given \a other region. Equivalent
     * to xored().
     *
     * \sa xored(), operator^=()
     */
    Region operator^(const Region &other) const;

    /*!
     * \overload
     *
     * Returns a region that is the exclusive or of this region and the given \a other rectangle. Equivalent
     * to xored().
     *
     * \sa xored(), operator^=()
     */
    Region operator^(const Rect &other) const;

    /*!
     * Unites the \a other region with this region and assigns the result to this region. Equivalent
     * to \c{region = region.united(other)}.
     *
     * \sa united(), operator|()
     */
    Region &operator|=(const Region &other);

    /*!
     * \overload
     *
     * Unites the \a other rectangle with this region and assigns the result to this region. Equivalent
     * to \c{region = region.united(other)}.
     *
     * \sa united(), operator|()
     */
    Region &operator|=(const Rect &other);

    /*!
     * Unites the \a other region with this region and assigns the result to this region. Equivalent
     * to \c{region = region.united(other)}.
     *
     * \sa united(), operator+()
     */
    Region &operator+=(const Region &other);

    /*!
     * \overload
     *
     * Unites the \a other rectangle with this region and assigns the result to this region. Equivalent
     * to \c{region = region.united(other)}.
     *
     * \sa united(), operator+()
     */
    Region &operator+=(const Rect &other);

    /*!
     * Subtracts the \a other region from this region and assigns the result to this region. Equivalent
     * to \c{region = region.subtracted(other)}.
     *
     * \sa subtracted(), operator-()
     */
    Region &operator-=(const Region &other);

    /*!
     * \overload
     *
     * Subtracts the \a other rectangle from this region and assigns the result to this region. Equivalent
     * to \c{region = region.subtracted(other)}.
     *
     * \sa subtracted(), operator-()
     */
    Region &operator-=(const Rect &other);

    /*!
     * Intersects the \a other region with this region and assigns the result to this region. Equivalent
     * to \c{region = region.intersected(other)}.
     *
     * \sa intersected(), operator&()
     */
    Region &operator&=(const Region &other);

    /*!
     * \overload
     *
     * Intersects the \a other rectangle with this region and assigns the result to this region. Equivalent
     * to \c{region = region.intersected(other)}.
     *
     * \sa intersected(), operator&()
     */
    Region &operator&=(const Rect &other);

    /*!
     * Applies the exclusive OR operation to the \a other region and this region and assigns the result
     * to this region. Equivalent to \c{region = region.xored(other)}.
     *
     * \sa xored(), operator^()
     */
    Region &operator^=(const Region &other);

    /*!
     * \overload
     *
     * Applies the exclusive OR operation to the \a other rectangle and this region and assigns the result
     * to this region. Equivalent to \c{region = region.xored(other)}.
     *
     * \sa xored(), operator^()
     */
    Region &operator^=(const Rect &other);

    /*!
     * Converts this region to a QRegion.
     */
    explicit operator QRegion() const;

    /*!
     * Constructs a region from the given \a rects. The rectangles must be sorted in the y-x
     * lexicographical order. In other words, the rectangles must be sorted by the top edge coordinate,
     * from top to bottom. If two rectangles have the same top coordinate, they should be sorted by
     * the left coordinate, from left to right. Rectangles with the same top edge coordinate must
     * have the same bottom coordinate.
     *
     * No rectangles are allowed to overlap. Furthermore, two rectangles cannot touch each other
     * horizontally, a rectangle must occupy the horizontal space as much as possible.
     *
     * rects() can be safely passed to this function.
     *
     * \a rects should not have any empty rectangles.
     *
     * \sa rects(), fromUnsortedRects(), fromRectsSortedByY()
     */
    static Region fromSortedRects(const QList<Rect> &rects);

    /*!
     * Constructs a region from the given list of unsorted rectangles \a rects. There are no concrete
     * requirements for rectangles. They can overlap, there can be duplicate rectangles, they can be
     * stored in any order in the list. However, the \a rects should not have any empty rectangles.
     *
     * \sa fromSortedRects(), fromRectsSortedByY()
     */
    static Region fromUnsortedRects(const QList<Rect> &rects);

    /*!
     * Constructs a region from the given list of partially sorted rectangles \a rects. The rectangles
     * must be sorted by the top edge position, from top to bottom. The rectangles are allowed to overlap.
     * The rectangles with the same top edge coordinate are not required to be sorted by the left
     * edge coordinate and share the same bottom edge coordinate.
     *
     * \a rects should not have any empty rectangles.
     *
     * This can be more efficient than constructing a region by calling the united() function in a loop.
     *
     * \sa fromSortedRects(), fromUnsortedRects()
     */
    static Region fromRectsSortedByY(const QList<Rect> &rects);

    /*!
     * Returns the infinite region.
     */
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
