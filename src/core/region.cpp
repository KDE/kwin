/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/region.h"

#include <QDebug>

namespace KWin
{

Region::Region(const QRegion &region)
{
    const QSpan<const QRect> rects = region.rects();
    if (rects.size() > 1) {
        m_rects.reserve(rects.size());
        for (const QRect &rect : rects) {
            m_rects.append(Rect(rect));
        }
    }

    m_bounds = region.boundingRect();
}

Rect Region::boundingRect() const
{
    return m_bounds;
}

bool Region::isEmpty() const
{
    return m_bounds.isEmpty();
}

qsizetype Region::bandByY(int y) const
{
    // If it becomes a performance bottleneck, use the binary search algorithm.
    qsizetype index = 0;
    for (; index != m_rects.size(); ++index) {
        if (y < m_rects[index].bottom()) {
            break;
        }
    }
    return index;
}

bool Region::contains(const Rect &rect) const
{
    if (isEmpty()) {
        return false;
    } else if (rect.isEmpty()) {
        return false;
    }

    if (m_rects.isEmpty()) {
        return m_bounds.contains(rect);
    } else if (!m_bounds.intersects(rect)) {
        return false;
    }

    int scanline = rect.top();
    for (qsizetype index = bandByY(scanline); index != m_rects.size(); ++index) {
        if (m_rects[index].bottom() <= scanline) {
            continue;
        }

        // The given rect is either partially outside the region or there is a gap between bands.
        if (scanline < m_rects[index].top()) {
            return false;
        }

        // Skip a rectangle in the current band if it is far to the left.
        if (m_rects[index].right() <= rect.left()) {
            continue;
        }

        if (m_rects[index].left() <= rect.left() && rect.right() <= m_rects[index].right()) {
            scanline = m_rects[index].bottom();
            if (rect.bottom() <= scanline) {
                return true;
            }
        } else {
            // The band rect either partially covers the given rect or it is far to the right.
            return false;
        }
    }

    return false;
}

bool Region::contains(const QPoint &point) const
{
    if (isEmpty()) {
        return false;
    }

    if (m_rects.isEmpty()) {
        return m_bounds.contains(point);
    }

    for (qsizetype index = bandByY(point.y()); index != m_rects.size(); ++index) {
        if (point.y() < m_rects[index].top()) {
            return false;
        }

        if (m_rects[index].right() <= point.x()) {
            continue;
        }

        if (m_rects[index].left() <= point.x()) {
            return true;
        }

        if (m_rects[index].left() > point.x()) {
            return false;
        }
    }

    return false;
}

bool Region::intersects(const Rect &rect) const
{
    if (isEmpty()) {
        return false;
    } else if (rect.isEmpty()) {
        return false;
    }

    if (m_rects.isEmpty()) {
        return m_bounds.intersects(rect);
    } else if (!m_bounds.intersects(rect)) {
        return false;
    }

    int scanline = rect.top();
    for (qsizetype index = bandByY(scanline); index != m_rects.size(); ++index) {
        if (m_rects[index].bottom() <= scanline) {
            continue;
        }

        // The current band is below the specified rectangle, no intersection is possible.
        if (rect.bottom() <= m_rects[index].top()) {
            return false;
        }

        // Skip a rectangle in the current band if it is far to the left.
        if (m_rects[index].right() <= rect.left()) {
            continue;
        }

        // Jump to the next band if a rectangle in the current band is far to the right.
        if (rect.right() <= m_rects[index].left()) {
            scanline = m_rects[index].bottom();
            if (rect.bottom() <= scanline) {
                return false;
            }
        } else {
            // A rectangle in the current band partially overlaps with the specified rectangle.
            return true;
        }
    }

    return false;
}

bool Region::intersects(const Region &other) const
{
    if (isEmpty()) {
        return false;
    } else if (other.isEmpty()) {
        return false;
    }

    const Rect intersectedBounds = m_bounds.intersected(other.m_bounds);
    if (intersectedBounds.isEmpty()) {
        return false;
    } else if (m_rects.isEmpty() && other.m_rects.isEmpty()) {
        return true;
    }

    for (const Rect &rect : other.rects()) {
        if (intersects(rect)) {
            return true;
        }
    }

    return false;
}

QSpan<const Rect> Region::rects() const
{
    if (isEmpty()) {
        return QSpan<const Rect>();
    }

    if (m_rects.isEmpty()) {
        return QSpan(&m_bounds, 1);
    } else {
        return QSpan(m_rects);
    }
}

static Rect slicedRect(const Rect &rect, int top, int bottom)
{
    const int left = rect.left();
    const int right = rect.right();
    return Rect(QPoint(left, top), QPoint(right, bottom));
}

Region::BandRef Region::sliceBand(QSpan<const Rect> rects, int top, int bottom, const BandRef &previousBand)
{
    for (const Rect &rect : rects) {
        m_rects.emplaceBack(slicedRect(rect, top, bottom));
    }

    return coalesceBands(previousBand, BandRef(previousBand.end, m_rects.size()));
}

Region::BandRef Region::mergeBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom, const BandRef &previousBand)
{
    Rect *previousRect = nullptr;
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;

    while (leftIndex < left.size() && rightIndex < right.size()) {
        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.left() < rightRect.left()) {
            if (previousRect && leftRect.left() <= previousRect->right()) {
                if (previousRect->right() < leftRect.right()) {
                    previousRect->setRight(leftRect.right());
                }
            } else {
                previousRect = &m_rects.emplaceBack(slicedRect(leftRect, top, bottom));
            }

            ++leftIndex;
        } else {
            if (previousRect && rightRect.left() <= previousRect->right()) {
                if (previousRect->right() < rightRect.right()) {
                    previousRect->setRight(rightRect.right());
                }
            } else {
                previousRect = &m_rects.emplaceBack(slicedRect(rightRect, top, bottom));
            }

            ++rightIndex;
        }
    }

    while (leftIndex != left.size()) {
        const Rect &leftRect = left[leftIndex];

        if (leftRect.left() <= previousRect->right()) {
            if (previousRect->right() < leftRect.right()) {
                previousRect->setRight(leftRect.right());
            }
        } else {
            previousRect = &m_rects.emplaceBack(slicedRect(leftRect, top, bottom));
        }

        ++leftIndex;
    }

    while (rightIndex != right.size()) {
        const Rect &rightRect = right[rightIndex];

        if (rightRect.left() <= previousRect->right()) {
            if (previousRect->right() < rightRect.right()) {
                previousRect->setRight(rightRect.right());
            }
        } else {
            previousRect = &m_rects.emplaceBack(slicedRect(rightRect, top, bottom));
        }

        ++rightIndex;
    }

    return coalesceBands(previousBand, BandRef(previousBand.end, m_rects.size()));
}

Region::BandRef Region::organizeBand(QSpan<const Rect> rects, int top, int bottom, const BandRef &previousBand)
{
    Rect *previousRect = nullptr;
    for (const Rect &rect : rects) {
        if (previousRect && rect.left() <= previousRect->right()) {
            if (previousRect->right() < rect.right()) {
                previousRect->setRight(rect.right());
            }
        } else {
            previousRect = &m_rects.emplaceBack(slicedRect(rect, top, bottom));
        }
    }

    return coalesceBands(previousBand, BandRef(previousBand.end, m_rects.size()));
}

Region::BandRef Region::subtractBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom, const BandRef &previousBand)
{
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;

    int scanline = std::min(left[0].left(), right[0].left());
    while (leftIndex != left.size() && rightIndex != right.size()) {
        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.left() <= rightRect.left()) {
            const int x = std::max(leftRect.left(), scanline);
            if (x < rightRect.left()) {
                scanline = std::min(leftRect.right(), rightRect.left());
                m_rects.emplaceBack(Rect(QPoint(x, top), QPoint(scanline, bottom)));
            } else {
                scanline = std::min(leftRect.right(), rightRect.right());
            }
        } else {
            scanline = std::min(leftRect.right(), rightRect.right());
        }

        if (leftRect.right() == scanline) {
            ++leftIndex;
        }
        if (rightRect.right() == scanline) {
            ++rightIndex;
        }
    }

    if (leftIndex != left.size()) {
        m_rects.emplaceBack(Rect(QPoint(std::max(left[leftIndex].left(), scanline), top), QPoint(left[leftIndex].right(), bottom)));
        ++leftIndex;

        while (leftIndex != left.size()) {
            m_rects.emplaceBack(Rect(QPoint(left[leftIndex].left(), top), QPoint(left[leftIndex].right(), bottom)));
            ++leftIndex;
        }
    }

    return coalesceBands(previousBand, BandRef(previousBand.end, m_rects.size()));
}

Region::BandRef Region::xorBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom, const BandRef &previousBand)
{
    Rect *previousRect = nullptr;
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;

    int scanline = std::min(left[0].left(), right[0].left());
    while (leftIndex != left.size() && rightIndex != right.size()) {
        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.left() <= rightRect.left()) {
            const int x = std::max(leftRect.left(), scanline);
            if (x < rightRect.left()) {
                scanline = std::min(leftRect.right(), rightRect.left());
                if (previousRect && previousRect->right() == x) {
                    previousRect->setRight(scanline);
                } else {
                    previousRect = &m_rects.emplaceBack(Rect(QPoint(x, top), QPoint(scanline, bottom)));
                }
            } else {
                scanline = std::min(leftRect.right(), rightRect.right());
            }
        } else {
            const int x = std::max(rightRect.left(), scanline);
            if (x < leftRect.left()) {
                scanline = std::min(leftRect.left(), rightRect.right());
                if (previousRect && previousRect->right() == x) {
                    previousRect->setRight(scanline);
                } else {
                    previousRect = &m_rects.emplaceBack(Rect(QPoint(x, top), QPoint(scanline, bottom)));
                }
            } else {
                scanline = std::min(leftRect.right(), rightRect.right());
            }
        }

        if (leftRect.right() == scanline) {
            ++leftIndex;
        }
        if (rightRect.right() == scanline) {
            ++rightIndex;
        }
    }

    if (leftIndex != left.size()) {
        const int y = std::max(left[leftIndex].left(), scanline);
        if (previousRect && previousRect->right() == y) {
            previousRect->setRight(left[leftIndex].right());
        } else {
            m_rects.emplaceBack(Rect(QPoint(y, top), QPoint(left[leftIndex].right(), bottom)));
        }

        ++leftIndex;

        while (leftIndex != left.size()) {
            m_rects.emplaceBack(Rect(QPoint(left[leftIndex].left(), top), QPoint(left[leftIndex].right(), bottom)));
            ++leftIndex;
        }
    }

    if (rightIndex != right.size()) {
        const int y = std::max(right[rightIndex].left(), scanline);
        if (previousRect && previousRect->right() == y) {
            previousRect->setRight(right[rightIndex].right());
        } else {
            m_rects.emplaceBack(Rect(QPoint(y, top), QPoint(right[rightIndex].right(), bottom)));
        }

        ++rightIndex;

        while (rightIndex != right.size()) {
            m_rects.emplaceBack(Rect(QPoint(right[rightIndex].left(), top), QPoint(right[rightIndex].right(), bottom)));
            ++rightIndex;
        }
    }

    return coalesceBands(previousBand, BandRef(previousBand.end, m_rects.size()));
}

Region::BandRef Region::intersectBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom, const BandRef &previousBand)
{
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        const int x1 = std::max(left[leftIndex].left(), right[rightIndex].left());
        const int x2 = std::min(left[leftIndex].right(), right[rightIndex].right());
        if (x1 < x2) {
            m_rects.emplaceBack(Rect(QPoint(x1, top), QPoint(x2, bottom)));
        }

        if (left[leftIndex].right() == x2) {
            ++leftIndex;
        }
        if (right[rightIndex].right() == x2) {
            ++rightIndex;
        }
    }

    return coalesceBands(previousBand, BandRef(previousBand.end, m_rects.size()));
}

Region::BandRef Region::coalesceBands(const BandRef &previous, const BandRef &current)
{
    const qsizetype currentCount = current.end - current.start;
    const qsizetype previousCount = previous.end - previous.start;
    if (!currentCount || previousCount != currentCount) {
        return current;
    }

    const int previousBottom = m_rects[previous.start].bottom();
    const int currentTop = m_rects[current.start].top();
    if (previousBottom != currentTop) {
        return current;
    }

    for (qsizetype i = 0; i < currentCount; ++i) {
        const Rect &a = m_rects[previous.start + i];
        const Rect &b = m_rects[current.start + i];
        if (a.left() != b.left() || a.right() != b.right()) {
            return current;
        }
    }

    const int currentBottom = m_rects[current.start].bottom();
    for (qsizetype i = 0; i < currentCount; ++i) {
        m_rects[previous.start + i].setBottom(currentBottom);
    }

    m_rects.remove(current.start, currentCount);
    return previous;
}

void Region::appendRects(QSpan<const Rect> rects)
{
    if (rects.isEmpty()) {
        return;
    }

    const qsizetype start = m_rects.size();
    m_rects.resizeForOverwrite(start + rects.size());
    std::memcpy(m_rects.data() + start, rects.data(), rects.size_bytes());
}

/**
 * @internal
 *
 * In order to unite two regions, a scanline runs from top to bottom. The scanline stops at the boundaries
 * of bands. For example, consider that we want to unite the following two regions
 *
 *   a |---------------|
 *     |               |
 *     |               |
 *   b |            |---------------|
 *     |            |  |            |
 *     |            |  |            |
 *     |            |  |            |
 *   c |------------|--|            |
 *                  |               |
 *                  |               |
 *   d              |---------------|
 *
 * The scanline will start scanning rects starting from position "a". Then, it will move to the
 * next position "b" (the top edge of the right rectangle) and make a new band by slicing all rectangles
 * that are between "a" and "b", in our example only the left rectangle will be sliced. Then, the
 * scanline moves to "c" (the bottom edge of the left rectangle) and creates a new band by slicing
 * and merging all rectangles that are between "b" and "c", there can be overlaps between two regions
 * so the band union algorithm has to account for that. Finally, the scanline moves to position "d" and
 * creates the last band c-d.
 *
 * After every stop, we also try to merge the current band with the previous band.
 *
 * The final region will look as follows
 *
 *   a |---------------|
 *     |               |
 *     |               |
 *   b |---------------|
 *   b |----------------------------|
 *     |                            |
 *     |                            |
 *     |                            |
 *   c |----------------------------|
 *   c              |---------------|
 *                  |               |
 *                  |               |
 *   d              |---------------|
 */
void Region::unite(QSpan<const Rect> left, QSpan<const Rect> right)
{
    m_rects.reserve(left.size() + right.size());

    qsizetype leftIndex = 0;
    qsizetype leftEnd = 0;
    qsizetype rightIndex = 0;
    qsizetype rightEnd = 0;

    int scanline = std::min(left[0].top(), right[0].top());
    BandRef currentBand;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.top() <= rightRect.top()) {
            const int y = std::max(leftRect.top(), scanline);
            if (y < rightRect.top()) {
                scanline = std::min(leftRect.bottom(), rightRect.top());
                currentBand = sliceBand(left.subspan(leftIndex, leftEnd - leftIndex), y, scanline, currentBand);
            } else {
                scanline = std::min(leftRect.bottom(), rightRect.bottom());
                currentBand = mergeBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            }
        } else {
            const int y = std::max(rightRect.top(), scanline);
            if (y < leftRect.top()) {
                scanline = std::min(leftRect.top(), rightRect.bottom());
                currentBand = sliceBand(right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            } else {
                scanline = std::min(leftRect.bottom(), rightRect.bottom());
                currentBand = mergeBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            }
        }

        if (leftRect.bottom() == scanline) {
            leftIndex = leftEnd;
        }
        if (rightRect.bottom() == scanline) {
            rightIndex = rightEnd;
        }
    }

    if (leftIndex != left.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        sliceBand(left.subspan(leftIndex, leftEnd - leftIndex), std::max(left[leftIndex].top(), scanline), left[leftIndex].bottom(), currentBand);
        appendRects(left.subspan(leftEnd));
    } else if (rightIndex != right.size()) {
        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        sliceBand(right.subspan(rightIndex, rightEnd - rightIndex), std::max(right[rightIndex].top(), scanline), right[rightIndex].bottom(), currentBand);
        appendRects(right.subspan(rightEnd));
    }
}

Region Region::united(const Region &other) const
{
    if (isEmpty()) {
        return other;
    } else if (other.isEmpty()) {
        return *this;
    }

    if (m_rects.isEmpty() && other.contains(m_bounds)) {
        return other;
    } else if (other.m_rects.isEmpty() && contains(other.m_bounds)) {
        return *this;
    }

    if (*this == other) {
        return *this;
    }

    Region result;
    result.unite(rects(), other.rects());

    if (result.m_rects.size() == 1) {
        result.m_bounds = result.m_rects.constFirst();
        result.m_rects = {};
    } else {
        result.m_bounds = m_bounds.united(other.m_bounds);
    }

    return result;
}

/**
 * @internal
 *
 * In order to subtract two regions, a scanline runs from top to bottom. The scanline stops at the boundaries
 * of bands. For example, consider that we want to subtract the following two regions
 *
 *   a |---------------|
 *     |               |
 *     |               |
 *   b |            |---------------|
 *     |            |  |            |
 *     |            |  |            |
 *     |            |  |            |
 *   c |------------|--|            |
 *                  |               |
 *                  |               |
 *   d              |---------------|
 *
 * The scanline will start scanning rects starting from position "a". Then, it will move to the
 * next position "b" (the top edge of the right rectangle) and make a new a-b band by slicing the
 * left rectangle. Then, the scanline moves to "c" (the bottom edge of the left rectangle) and
 * creates a new band by slicing and removing parts of the left rectangle that overlap with the
 * right rectangle. Finally, the scanline moves to position "d", since only the right rectangle
 * intersects with c-d, no rectangle will be added to the final region.
 *
 * After every stop, we also try to merge the current band with the previous band.
 *
 * The final region will look as follows
 *
 *   a |---------------|
 *     |               |
 *     |               |
 *   b |---------------|
 *   b |------------|
 *     |            |
 *     |            |
 *     |            |
 *   c |------------|
 */
void Region::subtract(QSpan<const Rect> left, QSpan<const Rect> right)
{
    m_rects.reserve(left.size());

    qsizetype leftIndex = 0;
    qsizetype leftEnd = 0;
    qsizetype rightIndex = 0;
    qsizetype rightEnd = 0;

    int scanline = std::min(left[0].top(), right[0].top());
    BandRef currentBand;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.top() <= rightRect.top()) {
            const int y = std::max(leftRect.top(), scanline);
            if (y < rightRect.top()) {
                scanline = std::min(leftRect.bottom(), rightRect.top());
                currentBand = sliceBand(left.subspan(leftIndex, leftEnd - leftIndex), y, scanline, currentBand);
            } else {
                scanline = std::min(leftRect.bottom(), rightRect.bottom());
                currentBand = subtractBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            }
        } else {
            const int y = std::max(rightRect.top(), scanline);
            if (y < leftRect.top()) {
                scanline = std::min(leftRect.top(), rightRect.bottom());
            } else {
                scanline = std::min(leftRect.bottom(), rightRect.bottom());
                currentBand = subtractBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            }
        }

        if (leftRect.bottom() == scanline) {
            leftIndex = leftEnd;
        }
        if (rightRect.bottom() == scanline) {
            rightIndex = rightEnd;
        }
    }

    if (leftIndex != left.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        sliceBand(left.subspan(leftIndex, leftEnd - leftIndex), std::max(left[leftIndex].top(), scanline), left[leftIndex].bottom(), currentBand);
        appendRects(left.subspan(leftEnd));
    }
}

Region Region::subtracted(const Region &other) const
{
    if (isEmpty()) {
        return Region();
    } else if (other.isEmpty()) {
        return *this;
    }

    if (!m_bounds.intersects(other.m_bounds)) {
        return *this;
    }

    if (*this == other) {
        return Region();
    }

    Region result;
    result.subtract(rects(), other.rects());
    result.computeBounds();

    return result;
}

/**
 * @internal
 *
 * In order to xor two regions, a scanline runs from top to bottom. The scanline stops at the boundaries
 * of bands. For example, consider that we want to xor the following two regions
 *
 *   a |---------------|
 *     |               |
 *     |               |
 *   b |            |---------------|
 *     |            |  |            |
 *     |            |  |            |
 *     |            |  |            |
 *   c |------------|--|            |
 *                  |               |
 *                  |               |
 *   d              |---------------|
 *
 * The scanline will start scanning rects starting from position "a". Then, it will move to the
 * next position "b" (the top edge of the right rectangle) and make a new a-b band by slicing the
 * left rectangle. Then, the scanline moves to "c" (the bottom edge of the left rectangle) and
 * creates a new band by slicing and removing parts of the left rectangle that overlap with the
 * right rectangle and vice versa. Finally, the scanline moves to position "d" and the last band
 * is created by slicing the right rectangle.
 *
 * After every stop, we also try to merge the current band with the previous band.
 *
 * The final region will look as follows
 *
 *   a |---------------|
 *     |               |
 *     |               |
 *   b |---------------|
 *   b |------------|  |------------|
 *     |            |  |            |
 *     |            |  |            |
 *     |            |  |            |
 *   c |------------|  |------------|
 *   c              |---------------|
 *                  |               |
 *                  |               |
 *   d              |---------------|
 *
 * Note there are three bands: a-b (includes one rectangle), b-c (includes two rectangles), and
 * c-d (includes one rectangle).
 */
void Region::exclusiveOr(QSpan<const Rect> left, QSpan<const Rect> right)
{
    m_rects.reserve(left.size() + right.size());

    qsizetype leftIndex = 0;
    qsizetype leftEnd = 0;
    qsizetype rightIndex = 0;
    qsizetype rightEnd = 0;

    int scanline = std::min(left[0].top(), right[0].top());
    BandRef currentBand;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.top() <= rightRect.top()) {
            const int y = std::max(leftRect.top(), scanline);
            if (y < rightRect.top()) {
                scanline = std::min(leftRect.bottom(), rightRect.top());
                currentBand = sliceBand(left.subspan(leftIndex, leftEnd - leftIndex), y, scanline, currentBand);
            } else {
                scanline = std::min(leftRect.bottom(), rightRect.bottom());
                currentBand = xorBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            }
        } else {
            const int y = std::max(rightRect.top(), scanline);
            if (y < leftRect.top()) {
                scanline = std::min(leftRect.top(), rightRect.bottom());
                currentBand = sliceBand(right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            } else {
                scanline = std::min(leftRect.bottom(), rightRect.bottom());
                currentBand = xorBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            }
        }

        if (leftRect.bottom() == scanline) {
            leftIndex = leftEnd;
        }
        if (rightRect.bottom() == scanline) {
            rightIndex = rightEnd;
        }
    }

    if (leftIndex != left.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        sliceBand(left.subspan(leftIndex, leftEnd - leftIndex), std::max(left[leftIndex].top(), scanline), left[leftIndex].bottom(), currentBand);
        appendRects(left.subspan(leftEnd));
    } else if (rightIndex != right.size()) {
        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        sliceBand(right.subspan(rightIndex, rightEnd - rightIndex), std::max(right[rightIndex].top(), scanline), right[rightIndex].bottom(), currentBand);
        appendRects(right.subspan(rightEnd));
    }
}

Region Region::xored(const Region &other) const
{
    if (isEmpty()) {
        return other;
    } else if (other.isEmpty()) {
        return *this;
    }

    if (*this == other) {
        return Region();
    }

    Region result;
    result.exclusiveOr(rects(), other.rects());
    result.computeBounds();

    return result;
}

/**
 * @internal
 *
 * In order to intersect two regions, a scanline runs from top to bottom. The scanline stops at the boundaries
 * of bands. For example, consider that we want to intersect the following two regions
 *
 *   a |---------------|
 *     |               |
 *     |               |
 *   b |            |---------------|
 *     |            |  |            |
 *     |            |  |            |
 *     |            |  |            |
 *   c |------------|--|            |
 *                  |               |
 *                  |               |
 *   d              |---------------|
 *
 * The scanline will start scanning rects starting from position "a". Then, it will move to the
 * next position "b" (the top edge of the right rectangle), no band will created in the final region
 * because the two rectangles don't intersect. Then, the scanline moves to "c" (the bottom edge of
 * the left rectangle) and creates a new band by slicing and intersecting parts of the left rectangle
 * that overlap with the right rectangle. Finally, the scanline moves to position "d" and the
 * algorithm stops.
 *
 * After every stop, we also try to merge the current band with the previous band.
 *
 * The final region will look as follows
 *
 *   b              |--|
 *                  |  |
 *                  |  |
 *                  |  |
 *   c              |--|
 */
void Region::intersect(QSpan<const Rect> left, QSpan<const Rect> right)
{
    m_rects.reserve(std::max(left.size(), right.size()));

    qsizetype leftIndex = 0;
    qsizetype leftEnd = 0;
    qsizetype rightIndex = 0;
    qsizetype rightEnd = 0;

    int scanline = std::min(left[0].top(), right[0].top());
    BandRef currentBand;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.top() <= rightRect.top()) {
            const int y = std::max(leftRect.top(), scanline);
            if (y < rightRect.top()) {
                scanline = std::min(leftRect.bottom(), rightRect.top());
            } else {
                scanline = std::min(leftRect.bottom(), rightRect.bottom());
                currentBand = intersectBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            }
        } else {
            const int y = std::max(rightRect.top(), scanline);
            if (y < leftRect.top()) {
                scanline = std::min(leftRect.top(), rightRect.bottom());
            } else {
                scanline = std::min(leftRect.bottom(), rightRect.bottom());
                currentBand = intersectBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline, currentBand);
            }
        }

        if (leftRect.bottom() == scanline) {
            leftIndex = leftEnd;
        }
        if (rightRect.bottom() == scanline) {
            rightIndex = rightEnd;
        }
    }
}

Region Region::intersected(const Region &other) const
{
    if (isEmpty()) {
        return Region();
    } else if (other.isEmpty()) {
        return Region();
    }

    const Rect intersectedBounds = m_bounds.intersected(other.m_bounds);
    if (intersectedBounds.isEmpty()) {
        return Region();
    } else if (m_rects.isEmpty() && other.m_rects.isEmpty()) {
        return Region(intersectedBounds);
    }

    if (m_rects.isEmpty() && m_bounds.contains(other.m_bounds)) {
        return other;
    } else if (other.m_rects.isEmpty() && other.m_bounds.contains(m_bounds)) {
        return *this;
    }

    Region result;
    result.intersect(rects(), other.rects());
    result.computeBounds();

    return result;
}

void Region::translate(const QPoint &offset)
{
    if (*this == infinite()) {
        return;
    }

    if (isEmpty()) {
        return;
    }

    m_bounds.translate(offset);
    for (Rect &rect : m_rects) {
        rect.translate(offset);
    }
}

Region Region::translated(const QPoint &offset) const
{
    if (*this == infinite()) {
        return *this;
    }

    Region result = *this;
    result.translate(offset);
    return result;
}

Region Region::scaledAndRoundedOut(qreal xScale, qreal yScale) const
{
    if (*this == infinite()) {
        return *this;
    }

    if (isEmpty()) {
        return Region();
    } else if (m_rects.isEmpty()) {
        return Region(m_bounds.scaled(xScale, yScale).roundedOut());
    }

    if (xScale == 1 && yScale == 1) {
        return *this;
    }

    QList<Rect> scaledRects;
    scaledRects.reserve(m_rects.size());
    for (const Rect &rect : m_rects) {
        scaledRects.append(rect.scaled(xScale, yScale).roundedOut());
    }

    Region result;
    result.assignRectsSortedByY(scaledRects);
    result.m_bounds = m_bounds.scaled(xScale, yScale).roundedOut();
    return result;
}

void Region::assignSortedRects(const QList<Rect> &rects)
{
    m_rects = rects;
}

/**
 * @internal
 *
 * A region is constructed from a list of rectangles sorted by the top edge coordinate by running
 * a sweep line from top to bottom. There is a buffer that contains rectangles that intersect with
 * the scan line. A rectangle will be added to the buffer when the sweep line is at its top edge, and
 * a rectangle will be removed from the buffer when the sweep line is at its bottom edge. Rectangles
 * in the buffer are sorted by the left coordinate. Every time the sweep line stops (either at the
 * top or the bottom edge of a rectangle), the buffer is sliced to produce a new band in the final
 * region.
 *
 * Note that the only requirement is that the rectangles are sorted by the top coordinate. The
 * rectangles may overlap vertically. The rectangles also don't need to be sorted by the x coordinate.
 *
 * For example, consider the following example
 *
 *                               -----------------
 *                               |               |
 *                               |               |
 *            -------------      |               |
 *            |           |      |       0       |
 *            |           |      |               |      --------------
 *            |           |      |               |      |            |
 *            |     1     |      |               |      |            |
 *            |           |      -----------------      |            |
 *            |           |                             |     2      |
 *            |           |                             |            |
 *            -------------                             |            |
 *                                                      |            |
 *                                                      |            |
 *                                                      --------------
 *
 * The sweep line will move as follows
 *
 *   a                           -----------------
 *                               |               |
 *                               |               |
 *   b        -------------      |               |
 *            |           |      |       0       |
 *   c        |           |      |               |      --------------
 *            |           |      |               |      |            |
 *            |     1     |      |               |      |            |
 *   d        |           |      -----------------      |            |
 *            |           |                             |     2      |
 *            |           |                             |            |
 *   e        -------------                             |            |
 *                                                      |            |
 *                                                      |            |
 *   f                                                  --------------
 *
 * The sweep line will start at position "a". The rectangle "0" will be added to the buffer, and the
 * sweep line will be advanced to position "b". As the last step, a new band will be created by slicing
 * the buffer between positions "a" and "b". Then the cycle repeats, the rectangle "1" is added to
 * the buffer, the sweep line is advanced to position "c" and the b-c band is created. In the next
 * cycle, the rectangle "2" is added to the buffer, the sweep line is advanced to position "d",
 * the c-d band is created, and the rectangle "0" is removed from the buffer. Next, the sweep line
 * is advanced to position "e", the d-e band gets created, and rectangle "1" is removed from the buffer.
 * Finally, the sweep line moves to position "f", the e-f band is created, and rectangle "2" is removed
 * from the buffer.
 */
void Region::assignRectsSortedByY(const QList<Rect> &rects)
{
    m_rects.reserve(rects.size());

    QList<Rect> buffer;
    buffer.reserve(rects.size());

    int scanline = rects[0].top();
    qsizetype index = 0;
    BandRef currentBand;

    while (index != rects.size()) {
        if (rects[index].top() == scanline) {
            for (; index != rects.size(); ++index) {
                if (rects[index].top() == scanline) {
                    auto insertionPoint = std::ranges::lower_bound(buffer, rects[index], [](const Rect &a, const Rect &b) {
                        return a.left() < b.left();
                    });
                    buffer.insert(insertionPoint, rects[index]);
                } else {
                    break;
                }
            }

            if (index == rects.size()) {
                break;
            }
        }

        // If it becomes a performance bottleneck, use a binary heap.
        int bottom = buffer[0].bottom();
        for (qsizetype j = 1; j < buffer.size(); ++j) {
            bottom = std::min(bottom, buffer[j].bottom());
        }

        const int previousScanline = scanline;
        scanline = std::min(rects[index].top(), bottom);

        currentBand = organizeBand(buffer, previousScanline, scanline, currentBand);

        if (scanline == bottom) {
            const auto removed = std::ranges::remove_if(buffer, [bottom](const Rect &rect) {
                return rect.bottom() == bottom;
            });
            buffer.erase(removed.begin(), removed.end());

            if (buffer.isEmpty()) {
                scanline = rects[index].top();
            }
        }
    }

    while (!buffer.isEmpty()) {
        int bottom = buffer[0].bottom();
        for (qsizetype j = 1; j < buffer.size(); ++j) {
            bottom = std::min(bottom, buffer[j].bottom());
        }

        const int previousScanline = scanline;
        scanline = bottom;

        currentBand = organizeBand(buffer, previousScanline, scanline, currentBand);

        const auto removed = std::ranges::remove_if(buffer, [bottom](const Rect &rect) {
            return rect.bottom() == bottom;
        });
        buffer.erase(removed.begin(), removed.end());
    }
}

void Region::computeBounds()
{
    if (m_rects.size() == 1) {
        m_bounds = m_rects.constFirst();
        m_rects = {};
    } else {
        for (const Rect &rect : std::as_const(m_rects)) {
            m_bounds |= rect;
        }
    }
}

Region::operator QRegion() const
{
    if (isEmpty()) {
        return QRegion();
    }

    if (m_rects.isEmpty()) {
        return QRegion(m_bounds);
    }

    QList<QRect> rects;
    rects.reserve(m_rects.size());
    for (const Rect &rect : m_rects) {
        rects.append(rect);
    }

    QRegion region;
    region.setRects(rects);
    return region;
}

Region Region::fromSortedRects(const QList<Rect> &rects)
{
    if (rects.isEmpty()) {
        return Region();
    } else if (rects.size() == 1) {
        return Region(rects[0]);
    }

    Region result;
    result.assignSortedRects(rects);
    result.computeBounds();
    return result;
}

Region Region::fromUnsortedRects(const QList<Rect> &rects)
{
    if (rects.isEmpty()) {
        return Region();
    } else if (rects.size() == 1) {
        return Region(rects[0]);
    }

    QList<Rect> sortedRects = rects;
    std::ranges::sort(sortedRects, [](const Rect &a, const Rect &b) {
        return a.top() < b.top();
    });

    return fromRectsSortedByY(sortedRects);
}

Region Region::fromRectsSortedByY(const QList<Rect> &rects)
{
    if (rects.isEmpty()) {
        return Region();
    } else if (rects.size() == 1) {
        return Region(rects.first());
    }

    Region result;
    result.assignRectsSortedByY(rects);
    result.computeBounds();
    return result;
}

QDebug operator<<(QDebug debug, const Region &region)
{
    QDebugStateSaver saver(debug);
    debug.nospace();

    debug << "KWin::Region(";
    if (region.isEmpty()) {
        debug << "empty";
    } else {
        const QSpan<const Rect> rects = region.rects();
        debug << "size=" << rects.size();
        debug << ", bounds=" << region.boundingRect();
        if (rects.size() > 1) {
            debug << " - [";
            for (qsizetype i = 0; i < rects.size(); ++i) {
                if (i != 0) {
                    debug << ", ";
                }
                debug << rects[i];
            }
            debug << "]";
        }
    }
    debug << ")";

    return debug;
}

QDataStream &operator>>(QDataStream &stream, Region &region)
{
    int boxCount = 0;
    stream >> boxCount;

    QList<Rect> rects;
    rects.reserve(boxCount);
    for (int i = 0; i < boxCount; ++i) {
        Rect box;
        stream >> box;
        rects.append(box);
    }

    region = Region::fromSortedRects(rects);
    return stream;
}

QDataStream &operator<<(QDataStream &stream, const Region &region)
{
    const QSpan<const Rect> rects = region.rects();

    stream << int(rects.size());
    for (const Rect &rect : rects) {
        stream << rect;
    }

    return stream;
}

} // namespace KWin
