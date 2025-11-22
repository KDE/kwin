/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/region.h"

#include <QDebug>

namespace KWin
{

Region::Region()
{
}

Region::Region(int x, int y, int width, int height)
    : m_extents(x, y, width, height)
{
}

Region::Region(const Rect &rect)
    : m_extents(rect)
{
}

Rect Region::boundingRect() const
{
    return m_extents;
}

bool Region::isEmpty() const
{
    return m_extents.isEmpty();
}

qsizetype Region::bandByY(int y) const
{
    // TODO: Use binary search?
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
        return m_extents.contains(rect);
    } else if (!m_extents.intersects(rect)) {
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
        return m_extents.contains(point);
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
        return m_extents.intersects(rect);
    } else if (!m_extents.intersects(rect)) {
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

    const Rect intersectedExtents = m_extents.intersected(other.m_extents);
    if (intersectedExtents.isEmpty()) {
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
        return QSpan(&m_extents, 1);
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

Region::BandReference Region::appendBand(QSpan<const Rect> rects, int top, int bottom)
{
    const int head = m_rects.size();
    for (const Rect &rect : rects) {
        m_rects.append(slicedRect(rect, top, bottom));
    }
    const int tail = m_rects.size();
    return BandReference(head, tail);
}

Region::BandReference Region::mergeBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom)
{
    Rect *previousRect = nullptr;
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;

    const int head = m_rects.size();

    while (leftIndex < left.size() && rightIndex < right.size()) {
        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.left() < rightRect.left()) {
            if (previousRect && leftRect.left() <= previousRect->right()) {
                if (previousRect->right() < leftRect.right()) {
                    previousRect->setRight(leftRect.right());
                }
            } else {
                m_rects.append(slicedRect(leftRect, top, bottom));
                previousRect = &m_rects.back();
            }

            ++leftIndex;
        } else {
            if (previousRect && rightRect.left() <= previousRect->right()) {
                if (previousRect->right() < rightRect.right()) {
                    previousRect->setRight(rightRect.right());
                }
            } else {
                m_rects.append(slicedRect(rightRect, top, bottom));
                previousRect = &m_rects.back();
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
            m_rects.append(slicedRect(leftRect, top, bottom));
            previousRect = &m_rects.back();
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
            m_rects.append(slicedRect(rightRect, top, bottom));
            previousRect = &m_rects.back();
        }

        ++rightIndex;
    }

    const int tail = m_rects.size();

    return BandReference(head, tail);
}

Region::BandReference Region::sliceBand(QSpan<const Rect> rects, int top, int bottom)
{
    const int head = m_rects.size();

    Rect *previousRect = nullptr;
    for (const Rect &rect : rects) {
        if (previousRect && rect.left() <= previousRect->right()) {
            if (previousRect->right() < rect.right()) {
                previousRect->setRight(rect.right());
            }
        } else {
            m_rects.append(slicedRect(rect, top, bottom));
            previousRect = &m_rects.back();
        }
    }

    const int tail = m_rects.size();

    return BandReference(head, tail);
}

Region::BandReference Region::subtractBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom)
{
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;

    const int head = m_rects.size();

    int scanline = std::min(left[0].left(), right[0].left());
    while (leftIndex != left.size() && rightIndex != right.size()) {
        const Rect &leftRect = left[leftIndex];
        const Rect &rightRect = right[rightIndex];

        if (leftRect.left() <= rightRect.left()) {
            const int x = std::max(leftRect.left(), scanline);
            if (x < rightRect.left()) {
                scanline = std::min(leftRect.right(), rightRect.left());
                m_rects.append(Rect(QPoint(x, top), QPoint(scanline, bottom)));
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
        m_rects.append(Rect(QPoint(std::max(left[leftIndex].left(), scanline), top), QPoint(left[leftIndex].right(), bottom)));
        ++leftIndex;

        while (leftIndex != left.size()) {
            m_rects.append(Rect(QPoint(left[leftIndex].left(), top), QPoint(left[leftIndex].right(), bottom)));
            ++leftIndex;
        }
    }

    const int tail = m_rects.size();
    return BandReference(head, tail);
}

Region::BandReference Region::exclusiveOrBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom)
{
    Rect *previousRect = nullptr;
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;

    const int head = m_rects.size();

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
                    m_rects.append(Rect(QPoint(x, top), QPoint(scanline, bottom)));
                    previousRect = &m_rects.back();
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
                    m_rects.append(Rect(QPoint(x, top), QPoint(scanline, bottom)));
                    previousRect = &m_rects.back();
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
            m_rects.append(Rect(QPoint(y, top), QPoint(left[leftIndex].right(), bottom)));
        }

        ++leftIndex;

        while (leftIndex != left.size()) {
            m_rects.append(Rect(QPoint(left[leftIndex].left(), top), QPoint(left[leftIndex].right(), bottom)));
            ++leftIndex;
        }
    }

    if (rightIndex != right.size()) {
        const int y = std::max(right[rightIndex].left(), scanline);
        if (previousRect && previousRect->right() == y) {
            previousRect->setRight(right[rightIndex].right());
        } else {
            m_rects.append(Rect(QPoint(y, top), QPoint(right[rightIndex].right(), bottom)));
        }

        ++rightIndex;

        while (rightIndex != right.size()) {
            m_rects.append(Rect(QPoint(right[rightIndex].left(), top), QPoint(right[rightIndex].right(), bottom)));
            ++rightIndex;
        }
    }

    const int tail = m_rects.size();
    return BandReference(head, tail);
}

Region::BandReference Region::intersectBands(QSpan<const Rect> left, QSpan<const Rect> right, int top, int bottom)
{
    qsizetype leftIndex = 0;
    qsizetype rightIndex = 0;

    const int head = m_rects.size();

    while (leftIndex != left.size() && rightIndex != right.size()) {
        const int x1 = std::max(left[leftIndex].left(), right[rightIndex].left());
        const int x2 = std::min(left[leftIndex].right(), right[rightIndex].right());
        if (x1 < x2) {
            m_rects.append(Rect(QPoint(x1, top), QPoint(x2, bottom)));
        }

        if (left[leftIndex].right() == x2) {
            ++leftIndex;
        }
        if (right[rightIndex].right() == x2) {
            ++rightIndex;
        }
    }

    const int tail = m_rects.size();
    return BandReference(head, tail);
}

Region::BandReference Region::coalesceBands(const BandReference &previous, const BandReference &current)
{
    if (!current.size() || previous.size() != current.size()) {
        return current;
    }

    const int previousBottom = m_rects[previous.start].bottom();
    const int currentTop = m_rects[current.start].top();
    if (previousBottom != currentTop) {
        return current;
    }

    const int n = current.size();
    for (int i = 0; i < n; ++i) {
        const Rect &a = m_rects[previous.start + i];
        const Rect &b = m_rects[current.start + i];
        if (a.left() != b.left() || a.right() != b.right()) {
            return current;
        }
    }

    const int currentBottom = m_rects[current.start].bottom();
    for (int i = 0; i < n; ++i) {
        m_rects[previous.start + i].setBottom(currentBottom);
    }

    m_rects.remove(current.start, current.size());
    return previous;
}

void Region::unite(QSpan<const Rect> left, QSpan<const Rect> right)
{
    qsizetype leftIndex = 0;
    qsizetype leftEnd = 0;
    qsizetype rightIndex = 0;
    qsizetype rightEnd = 0;

    int scanline = std::min(left[0].top(), right[0].top());
    BandReference currentBand;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const BandReference previousBand = currentBand;

        if (left[leftIndex].top() <= right[rightIndex].top()) {
            const int y = std::max(left[leftIndex].top(), scanline);
            if (y < right[rightIndex].top()) {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].top());
                currentBand = appendBand(left.subspan(leftIndex, leftEnd - leftIndex), y, scanline);
            } else {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].bottom());
                currentBand = mergeBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
            }
        } else {
            const int y = std::max(right[rightIndex].top(), scanline);
            if (y < left[leftIndex].top()) {
                scanline = std::min(left[leftIndex].top(), right[rightIndex].bottom());
                currentBand = appendBand(right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
            } else {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].bottom());
                currentBand = mergeBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
            }
        }

        currentBand = coalesceBands(previousBand, currentBand);

        if (left[leftIndex].bottom() == scanline) {
            leftIndex = leftEnd;
        }
        if (right[rightIndex].bottom() == scanline) {
            rightIndex = rightEnd;
        }
    }

    while (leftIndex != left.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        const BandReference previousBand = currentBand;
        currentBand = appendBand(left.subspan(leftIndex, leftEnd - leftIndex), std::max(left[leftIndex].top(), scanline), left[leftIndex].bottom());
        currentBand = coalesceBands(previousBand, currentBand);
        leftIndex = leftEnd;
    }

    while (rightIndex != right.size()) {
        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const BandReference previousBand = currentBand;
        currentBand = appendBand(right.subspan(rightIndex, rightEnd - rightIndex), std::max(right[rightIndex].top(), scanline), right[rightIndex].bottom());
        currentBand = coalesceBands(previousBand, currentBand);
        rightIndex = rightEnd;
    }
}

Region Region::united(const Region &other) const
{
    if (isEmpty()) {
        return other;
    } else if (other.isEmpty()) {
        return *this;
    }

    if (m_rects.isEmpty() && other.contains(m_extents)) {
        return other;
    } else if (other.m_rects.isEmpty() && contains(other.m_extents)) {
        return *this;
    }

    if (*this == other) {
        return *this;
    }

    Region result;
    result.unite(rects(), other.rects());

    if (result.m_rects.size() == 1) {
        result.m_extents = result.m_rects.constFirst();
        result.m_rects = {};
    } else {
        result.m_extents = m_extents.united(other.m_extents);
    }

    return result;
}

void Region::subtract(QSpan<const Rect> left, QSpan<const Rect> right)
{
    qsizetype leftIndex = 0;
    qsizetype leftEnd = 0;
    qsizetype rightIndex = 0;
    qsizetype rightEnd = 0;

    int scanline = std::min(left[0].top(), right[0].top());
    BandReference currentBand;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const BandReference previousBand = currentBand;

        if (left[leftIndex].top() <= right[rightIndex].top()) {
            const int y = std::max(left[leftIndex].top(), scanline);
            if (y < right[rightIndex].top()) {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].top());
                currentBand = appendBand(left.subspan(leftIndex, leftEnd - leftIndex), y, scanline);
                currentBand = coalesceBands(previousBand, currentBand);
            } else {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].bottom());
                currentBand = subtractBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
                currentBand = coalesceBands(previousBand, currentBand);
            }
        } else {
            const int y = std::max(right[rightIndex].top(), scanline);
            if (y < left[leftIndex].top()) {
                scanline = std::min(left[leftIndex].top(), right[rightIndex].bottom());
            } else {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].bottom());
                currentBand = subtractBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
                currentBand = coalesceBands(previousBand, currentBand);
            }
        }

        if (left[leftIndex].bottom() == scanline) {
            leftIndex = leftEnd;
        }
        if (right[rightIndex].bottom() == scanline) {
            rightIndex = rightEnd;
        }
    }

    while (leftIndex != left.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        const BandReference previousBand = currentBand;
        currentBand = appendBand(left.subspan(leftIndex, leftEnd - leftIndex), std::max(left[leftIndex].top(), scanline), left[leftIndex].bottom());
        currentBand = coalesceBands(previousBand, currentBand);
        leftIndex = leftEnd;
    }
}

Region Region::subtracted(const Region &other) const
{
    if (isEmpty()) {
        return Region();
    } else if (other.isEmpty()) {
        return *this;
    }

    if (!m_extents.intersects(other.m_extents)) {
        return *this;
    }

    if (*this == other) {
        return Region();
    }

    Region result;
    result.subtract(rects(), other.rects());
    result.computeExtents();

    return result;
}

void Region::exclusiveOr(QSpan<const Rect> left, QSpan<const Rect> right)
{
    qsizetype leftIndex = 0;
    qsizetype leftEnd = 0;
    qsizetype rightIndex = 0;
    qsizetype rightEnd = 0;

    int scanline = std::min(left[0].top(), right[0].top());
    BandReference currentBand;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const BandReference previousBand = currentBand;

        if (left[leftIndex].top() <= right[rightIndex].top()) {
            const int y = std::max(left[leftIndex].top(), scanline);
            if (y < right[rightIndex].top()) {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].top());
                currentBand = appendBand(left.subspan(leftIndex, leftEnd - leftIndex), y, scanline);
            } else {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].bottom());
                currentBand = exclusiveOrBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
            }
        } else {
            const int y = std::max(right[rightIndex].top(), scanline);
            if (y < left[leftIndex].top()) {
                scanline = std::min(left[leftIndex].top(), right[rightIndex].bottom());
                currentBand = appendBand(right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
            } else {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].bottom());
                currentBand = exclusiveOrBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
            }
        }

        currentBand = coalesceBands(previousBand, currentBand);

        if (left[leftIndex].bottom() == scanline) {
            leftIndex = leftEnd;
        }
        if (right[rightIndex].bottom() == scanline) {
            rightIndex = rightEnd;
        }
    }

    while (leftIndex != left.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        const BandReference previousBand = currentBand;
        currentBand = appendBand(left.subspan(leftIndex, leftEnd - leftIndex), std::max(left[leftIndex].top(), scanline), left[leftIndex].bottom());
        currentBand = coalesceBands(previousBand, currentBand);
        leftIndex = leftEnd;
    }

    while (rightIndex != right.size()) {
        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        const BandReference previousBand = currentBand;
        currentBand = appendBand(right.subspan(rightIndex, rightEnd - rightIndex), std::max(right[rightIndex].top(), scanline), right[rightIndex].bottom());
        currentBand = coalesceBands(previousBand, currentBand);
        rightIndex = rightEnd;
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
    result.computeExtents();

    return result;
}

void Region::intersect(QSpan<const Rect> left, QSpan<const Rect> right)
{
    qsizetype leftIndex = 0;
    qsizetype leftEnd = 0;
    qsizetype rightIndex = 0;
    qsizetype rightEnd = 0;

    int scanline = std::min(left[0].top(), right[0].top());
    BandReference currentBand;

    while (leftIndex != left.size() && rightIndex != right.size()) {
        while (leftEnd != left.size() && left[leftIndex].top() == left[leftEnd].top()) {
            ++leftEnd;
        }

        while (rightEnd != right.size() && right[rightIndex].top() == right[rightEnd].top()) {
            ++rightEnd;
        }

        if (left[leftIndex].top() <= right[rightIndex].top()) {
            const int y = std::max(left[leftIndex].top(), scanline);
            if (y < right[rightIndex].top()) {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].top());
            } else {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].bottom());

                const BandReference previousBand = currentBand;
                currentBand = intersectBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
                currentBand = coalesceBands(previousBand, currentBand);
            }
        } else {
            const int y = std::max(right[rightIndex].top(), scanline);
            if (y < left[leftIndex].top()) {
                scanline = std::min(left[leftIndex].top(), right[rightIndex].bottom());
            } else {
                scanline = std::min(left[leftIndex].bottom(), right[rightIndex].bottom());

                const BandReference previousBand = currentBand;
                currentBand = intersectBands(left.subspan(leftIndex, leftEnd - leftIndex), right.subspan(rightIndex, rightEnd - rightIndex), y, scanline);
                currentBand = coalesceBands(previousBand, currentBand);
            }
        }

        if (left[leftIndex].bottom() == scanline) {
            leftIndex = leftEnd;
        }
        if (right[rightIndex].bottom() == scanline) {
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

    const Rect intersectedExtents = m_extents.intersected(other.m_extents);
    if (intersectedExtents.isEmpty()) {
        return Region();
    } else if (m_rects.isEmpty() && other.m_rects.isEmpty()) {
        return Region(intersectedExtents);
    }

    if (m_rects.isEmpty() && m_extents.contains(other.m_extents)) {
        return other;
    } else if (other.m_rects.isEmpty() && other.m_extents.contains(m_extents)) {
        return *this;
    }

    Region result;
    result.intersect(rects(), other.rects());
    result.computeExtents();

    return result;
}

void Region::translate(const QPoint &offset)
{
    if (isEmpty()) {
        return;
    }

    m_extents.translate(offset);
    for (Rect &rect : m_rects) {
        rect.translate(offset);
    }
}

Region Region::translated(const QPoint &offset) const
{
    Region result = *this;
    result.translate(offset);
    return result;
}

Region Region::scaledAndRounded(qreal xScale, qreal yScale) const
{
    if (isEmpty()) {
        return Region();
    } else if (m_rects.isEmpty()) {
        return Region(m_extents.scaled(xScale, yScale).rounded());
    }

    if (xScale == 1 && yScale == 1) {
        return *this;
    }

    QList<Rect> scaledRects;
    scaledRects.reserve(m_rects.size());
    for (const Rect &rect : m_rects) {
        scaledRects.append(rect.scaled(xScale, yScale).rounded());
    }

    Region result;
    result.assignSortedRects(scaledRects);
    result.m_extents = m_extents.scaled(xScale, yScale).rounded();
    return result;
}

Region Region::scaledAndRoundedOut(qreal xScale, qreal yScale) const
{
    if (isEmpty()) {
        return Region();
    } else if (m_rects.isEmpty()) {
        return Region(m_extents.scaled(xScale, yScale).roundedOut());
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
    result.m_extents = m_extents.scaled(xScale, yScale).roundedOut();
    return result;
}

void Region::assignSortedRects(const QList<Rect> &rects)
{
    m_rects = rects;
}

void Region::assignRectsSortedByY(const QList<Rect> &rects)
{
    int scanline = rects[0].top();
    QList<Rect> buffer;

    qsizetype index = 0;
    BandReference currentBand;

    while (index != rects.size()) {
        if (rects[index].top() == scanline) {
            for (; index != rects.size(); ++index) {
                if (rects[index].top() == scanline) {
                    auto insertionPoint = std::lower_bound(buffer.begin(), buffer.end(), rects[index], [](const Rect &a, const Rect &b) {
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

        // TODO: Use a binary heap?
        int bottom = buffer[0].bottom();
        for (int j = 1; j < buffer.size(); ++j) {
            bottom = std::min(bottom, buffer[j].bottom());
        }

        const int previousScanline = scanline;
        scanline = std::min(rects[index].top(), bottom);

        const BandReference previousBand = currentBand;
        currentBand = sliceBand(buffer, previousScanline, scanline);
        currentBand = coalesceBands(previousBand, currentBand);

        if (scanline == bottom) {
            auto it = std::remove_if(buffer.begin(), buffer.end(), [bottom](const Rect &rect) {
                return rect.bottom() == bottom;
            });
            buffer.erase(it, buffer.end());

            if (buffer.isEmpty()) {
                scanline = rects[index].top();
            }
        }
    }

    while (!buffer.isEmpty()) {
        int bottom = buffer[0].bottom();
        for (int j = 1; j < buffer.size(); ++j) {
            bottom = std::min(bottom, buffer[j].bottom());
        }

        const int previousScanline = scanline;
        scanline = bottom;

        const BandReference previousBand = currentBand;
        currentBand = sliceBand(buffer, previousScanline, scanline);
        currentBand = coalesceBands(previousBand, currentBand);

        auto it = std::remove_if(buffer.begin(), buffer.end(), [bottom](const Rect &rect) {
            return rect.bottom() == bottom;
        });
        buffer.erase(it, buffer.end());
    }
}

void Region::computeExtents()
{
    if (m_rects.size() == 1) {
        m_extents = m_rects.constFirst();
        m_rects = {};
    } else {
        for (const Rect &rect : std::as_const(m_rects)) {
            m_extents |= rect;
        }
    }
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
    result.computeExtents();
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
    std::sort(sortedRects.begin(), sortedRects.end(), [](const Rect &a, const Rect &b) {
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
    result.computeExtents();
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
            for (int i = 0; i < rects.size(); ++i) {
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
