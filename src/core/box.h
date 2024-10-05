/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QRect>

namespace KWin
{

class KWIN_EXPORT Box
{
    Q_GADGET
    Q_PROPERTY(int x MEMBER m_x)
    Q_PROPERTY(int y MEMBER m_y)
    Q_PROPERTY(int width MEMBER m_width)
    Q_PROPERTY(int height MEMBER m_height)

public:
    constexpr Box() noexcept;
    constexpr Box(int x, int y, int width, int height) noexcept;
    constexpr Box(const QPoint &topLeft, const QPoint &bottomRight) noexcept;
    constexpr Box(const QPoint &topLeft, const QSize &size) noexcept;
    constexpr Box(const QRect &rect) noexcept;

    constexpr inline bool isEmpty() const noexcept;

    constexpr inline bool contains(const Box &box) const noexcept;
    constexpr inline bool contains(const QPoint &point) const noexcept;
    constexpr inline bool contains(int x, int y) const noexcept;

    constexpr inline int x() const noexcept;
    constexpr inline void setX(int x) noexcept;
    constexpr inline int y() const noexcept;
    constexpr inline void setY(int y) noexcept;
    constexpr inline int width() const noexcept;
    constexpr inline void setWidth(int width) noexcept;
    constexpr inline int height() const noexcept;
    constexpr inline void setHeight(int height) noexcept;

    constexpr inline QSize size() const noexcept;
    constexpr inline void setSize(const QSize &size) noexcept;

    constexpr inline int left() const noexcept;
    constexpr inline void setLeft(int left) noexcept;
    constexpr inline int top() const noexcept;
    constexpr inline void setTop(int top) noexcept;
    constexpr inline int right() const noexcept;
    constexpr inline void setRight(int right) noexcept;
    constexpr inline int bottom() const noexcept;
    constexpr inline void setBottom(int bottom) noexcept;

    constexpr inline QPoint center() const noexcept;
    constexpr inline QPoint topLeft() const noexcept;
    constexpr inline QPoint bottomRight() const noexcept;
    constexpr inline QPoint topRight() const noexcept;
    constexpr inline QPoint bottomLeft() const noexcept;
    constexpr inline void setTopLeft(const QPoint &point) noexcept;
    constexpr inline void setBottomRight(const QPoint &point) noexcept;
    constexpr inline void setTopRight(const QPoint &point) noexcept;
    constexpr inline void setBottomLeft(const QPoint &point) noexcept;
    constexpr inline void moveCenter(const QPoint &point) noexcept;
    constexpr inline void moveLeft(int left) noexcept;
    constexpr inline void moveTop(int top) noexcept;
    constexpr inline void moveRight(int right) noexcept;
    constexpr inline void moveBottom(int bottom) noexcept;
    constexpr inline void moveTopLeft(const QPoint &point) noexcept;
    constexpr inline void moveBottomRight(const QPoint &point) noexcept;
    constexpr inline void moveTopRight(const QPoint &point) noexcept;
    constexpr inline void moveBottomLeft(const QPoint &point) noexcept;
    constexpr inline void moveTo(int x, int y) noexcept;
    constexpr inline void moveTo(const QPoint &point) noexcept;

    constexpr inline void translate(int dx, int dy) noexcept;
    constexpr inline void translate(const QPoint &point) noexcept;
    constexpr inline Box translated(int dx, int dy) const noexcept;
    constexpr inline Box translated(const QPoint &point) const noexcept;
    constexpr inline Box transposed() const noexcept;

    constexpr inline void setRect(int x, int y, int width, int height) noexcept;
    constexpr inline void getRect(int *x, int *y, int *width, int *height) const;
    constexpr inline void setCoords(int x1, int y1, int x2, int y2) noexcept;
    constexpr inline void getCoords(int *x1, int *y1, int *x2, int *y2) const;

    constexpr inline void adjust(int x1, int y1, int x2, int y2) noexcept;
    constexpr inline Box adjusted(int x1, int y1, int x2, int y2) const noexcept;
    constexpr inline Box marginsAdded(const QMargins &margins) const noexcept;
    constexpr inline Box marginsRemoved(const QMargins &margins) const noexcept;

    constexpr inline Box united(const Box &other) const noexcept;
    constexpr inline Box intersected(const Box &other) const noexcept;
    constexpr inline bool intersects(const Box &other) const noexcept;

    constexpr inline bool operator==(const Box &other) const noexcept;
    constexpr inline bool operator!=(const Box &other) const noexcept;
    constexpr inline Box operator|(const Box &other) const noexcept;
    constexpr inline Box operator&(const Box &other) const noexcept;
    constexpr inline Box &operator|=(const Box &other) noexcept;
    constexpr inline Box &operator&=(const Box &other) noexcept;
    constexpr inline Box &operator+=(const QMargins &margins) noexcept;
    constexpr inline Box &operator-=(const QMargins &margins) noexcept;
    constexpr inline operator QRect() const noexcept;

private:
    int m_x;
    int m_y;
    int m_width;
    int m_height;
};

class KWIN_EXPORT BoxF
{
    Q_GADGET
    Q_PROPERTY(qreal x MEMBER m_x)
    Q_PROPERTY(qreal y MEMBER m_y)
    Q_PROPERTY(qreal width MEMBER m_width)
    Q_PROPERTY(qreal height MEMBER m_height)

public:
    constexpr BoxF() noexcept;
    constexpr BoxF(qreal x, qreal y, qreal width, qreal height) noexcept;
    constexpr BoxF(const QPointF &topLeft, const QPointF &bottomRight) noexcept;
    constexpr BoxF(const QPointF &topLeft, const QSizeF &size) noexcept;
    constexpr BoxF(const Box &box) noexcept;
    constexpr BoxF(const QRect &rect) noexcept;
    constexpr BoxF(const QRectF &rect) noexcept;

    constexpr inline bool isEmpty() const noexcept;

    constexpr inline bool contains(const BoxF &box) const noexcept;
    constexpr inline bool contains(const QPointF &point) const noexcept;
    constexpr inline bool contains(qreal x, qreal y) const noexcept;

    constexpr inline qreal x() const noexcept;
    constexpr inline void setX(qreal x) noexcept;
    constexpr inline qreal y() const noexcept;
    constexpr inline void setY(qreal y) noexcept;
    constexpr inline qreal width() const noexcept;
    constexpr inline void setWidth(qreal width) noexcept;
    constexpr inline qreal height() const noexcept;
    constexpr inline void setHeight(qreal height) noexcept;

    constexpr inline QSizeF size() const noexcept;
    constexpr inline void setSize(const QSizeF &size) noexcept;

    constexpr inline qreal left() const noexcept;
    constexpr inline void setLeft(qreal left) noexcept;
    constexpr inline qreal top() const noexcept;
    constexpr inline void setTop(qreal top) noexcept;
    constexpr inline qreal right() const noexcept;
    constexpr inline void setRight(qreal right) noexcept;
    constexpr inline qreal bottom() const noexcept;
    constexpr inline void setBottom(qreal bottom) noexcept;

    constexpr inline QPointF center() const noexcept;
    constexpr inline QPointF topLeft() const noexcept;
    constexpr inline QPointF bottomRight() const noexcept;
    constexpr inline QPointF topRight() const noexcept;
    constexpr inline QPointF bottomLeft() const noexcept;
    constexpr inline void setTopLeft(const QPointF &point) noexcept;
    constexpr inline void setBottomRight(const QPointF &point) noexcept;
    constexpr inline void setTopRight(const QPointF &point) noexcept;
    constexpr inline void setBottomLeft(const QPointF &point) noexcept;
    constexpr inline void moveCenter(const QPointF &point) noexcept;
    constexpr inline void moveLeft(qreal left) noexcept;
    constexpr inline void moveTop(qreal top) noexcept;
    constexpr inline void moveRight(qreal right) noexcept;
    constexpr inline void moveBottom(qreal bottom) noexcept;
    constexpr inline void moveTopLeft(const QPointF &point) noexcept;
    constexpr inline void moveBottomRight(const QPointF &point) noexcept;
    constexpr inline void moveTopRight(const QPointF &point) noexcept;
    constexpr inline void moveBottomLeft(const QPointF &point) noexcept;
    constexpr inline void moveTo(qreal x, qreal y) noexcept;
    constexpr inline void moveTo(const QPointF &point) noexcept;

    constexpr inline void translate(qreal dx, qreal dy) noexcept;
    constexpr inline void translate(const QPointF &point) noexcept;
    constexpr inline BoxF translated(qreal dx, qreal dy) const noexcept;
    constexpr inline BoxF translated(const QPointF &point) const noexcept;
    constexpr inline BoxF transposed() const noexcept;

    constexpr inline void setRect(qreal x, qreal y, qreal width, qreal height) noexcept;
    constexpr inline void getRect(qreal *x, qreal *y, qreal *width, qreal *height) const;
    constexpr inline void setCoords(qreal x1, qreal y1, qreal x2, qreal y2) noexcept;
    constexpr inline void getCoords(qreal *x1, qreal *y1, qreal *x2, qreal *y2) const;

    constexpr inline void adjust(qreal x1, qreal y1, qreal x2, qreal y2) noexcept;
    constexpr inline BoxF adjusted(qreal x1, qreal y1, qreal x2, qreal y2) const noexcept;
    constexpr inline BoxF marginsAdded(const QMarginsF &margins) const noexcept;
    constexpr inline BoxF marginsRemoved(const QMarginsF &margins) const noexcept;

    constexpr inline BoxF united(const BoxF &other) const noexcept;
    constexpr inline BoxF intersected(const BoxF &other) const noexcept;
    constexpr inline bool intersects(const BoxF &other) const noexcept;

    constexpr inline Box rounded() const noexcept;
    constexpr inline Box roundedOut() const noexcept;

    constexpr inline bool operator==(const BoxF &other) const noexcept;
    constexpr inline bool operator!=(const BoxF &other) const noexcept;
    constexpr inline BoxF operator|(const BoxF &other) const noexcept;
    constexpr inline BoxF operator&(const BoxF &other) const noexcept;
    constexpr inline BoxF &operator|=(const BoxF &other) noexcept;
    constexpr inline BoxF &operator&=(const BoxF &other) noexcept;
    constexpr inline BoxF &operator+=(const QMarginsF &margins) noexcept;
    constexpr inline BoxF &operator-=(const QMarginsF &margins) noexcept;
    constexpr inline operator QRectF() const noexcept;

private:
    qreal m_x;
    qreal m_y;
    qreal m_width;
    qreal m_height;
};

constexpr inline Box::Box() noexcept
    : m_x(0)
    , m_y(0)
    , m_width(0)
    , m_height(0)
{
}

constexpr inline Box::Box(int x, int y, int width, int height) noexcept
    : m_x(x)
    , m_y(y)
    , m_width(width)
    , m_height(height)
{
}

constexpr inline Box::Box(const QPoint &topLeft, const QPoint &bottomRight) noexcept
    : m_x(topLeft.x())
    , m_y(topLeft.y())
    , m_width(bottomRight.x() - topLeft.x())
    , m_height(bottomRight.y() - topLeft.y())
{
}

constexpr inline Box::Box(const QPoint &topLeft, const QSize &size) noexcept
    : m_x(topLeft.x())
    , m_y(topLeft.y())
    , m_width(size.width())
    , m_height(size.height())
{
}

constexpr inline Box::Box(const QRect &rect) noexcept
    : m_x(rect.x())
    , m_y(rect.y())
    , m_width(rect.width())
    , m_height(rect.height())
{
}

constexpr inline bool Box::isEmpty() const noexcept
{
    return m_width <= 0 || m_height <= 0;
}

constexpr inline bool Box::contains(const Box &box) const noexcept
{
    return (left() <= box.left() && box.right() <= right()) && (top() <= box.top() && box.bottom() <= bottom());
}

constexpr inline bool Box::contains(const QPoint &point) const noexcept
{
    return (left() <= point.x() && point.x() < right()) && (top() <= point.y() && point.y() < bottom());
}

constexpr inline bool Box::contains(int x, int y) const noexcept
{
    return contains(QPoint(x, y));
}

constexpr inline int Box::x() const noexcept
{
    return m_x;
}

constexpr inline void Box::setX(int x) noexcept
{
    m_x = x;
}

constexpr inline int Box::y() const noexcept
{
    return m_y;
}

constexpr inline void Box::setY(int y) noexcept
{
    m_y = y;
}

constexpr inline int Box::width() const noexcept
{
    return m_width;
}

constexpr inline void Box::setWidth(int width) noexcept
{
    m_width = width;
}

constexpr inline int Box::height() const noexcept
{
    return m_height;
}

constexpr inline void Box::setHeight(int height) noexcept
{
    m_height = height;
}

constexpr inline QSize Box::size() const noexcept
{
    return QSize(m_width, m_height);
}

constexpr inline void Box::setSize(const QSize &size) noexcept
{
    m_width = size.width();
    m_height = size.height();
}

constexpr inline int Box::left() const noexcept
{
    return m_x;
}

constexpr inline void Box::setLeft(int left) noexcept
{
    m_x = left;
}

constexpr inline int Box::top() const noexcept
{
    return m_y;
}

constexpr inline void Box::setTop(int top) noexcept
{
    m_y = top;
}

constexpr inline int Box::right() const noexcept
{
    return m_x + m_width;
}

constexpr inline void Box::setRight(int right) noexcept
{
    m_width = right - m_x;
}

constexpr inline int Box::bottom() const noexcept
{
    return m_y + m_height;
}

constexpr inline void Box::setBottom(int bottom) noexcept
{
    m_height = bottom - m_y;
}

constexpr inline QPoint Box::center() const noexcept
{
    return QPoint(m_x + m_width / 2, m_y + m_height / 2);
}

constexpr inline QPoint Box::topLeft() const noexcept
{
    return QPoint(left(), top());
}

constexpr inline QPoint Box::bottomRight() const noexcept
{
    return QPoint(right(), bottom());
}

constexpr inline QPoint Box::topRight() const noexcept
{
    return QPoint(right(), top());
}

constexpr inline QPoint Box::bottomLeft() const noexcept
{
    return QPoint(left(), bottom());
}

constexpr inline void Box::setTopLeft(const QPoint &point) noexcept
{
    setLeft(point.x());
    setTop(point.y());
}

constexpr inline void Box::setBottomRight(const QPoint &point) noexcept
{
    setRight(point.x());
    setBottom(point.y());
}

constexpr inline void Box::setTopRight(const QPoint &point) noexcept
{
    setRight(point.x());
    setTop(point.y());
}

constexpr inline void Box::setBottomLeft(const QPoint &point) noexcept
{
    setLeft(point.x());
    setBottom(point.y());
}

constexpr inline void Box::moveCenter(const QPoint &point) noexcept
{
    m_x = point.x() - m_width / 2;
    m_y = point.y() - m_height / 2;
}

constexpr inline void Box::moveLeft(int left) noexcept
{
    m_x = left;
}

constexpr inline void Box::moveTop(int top) noexcept
{
    m_y = top;
}

constexpr inline void Box::moveRight(int right) noexcept
{
    m_x = right - m_width;
}

constexpr inline void Box::moveBottom(int bottom) noexcept
{
    m_y = bottom - m_height;
}

constexpr inline void Box::moveTopLeft(const QPoint &point) noexcept
{
    moveLeft(point.x());
    moveTop(point.y());
}

constexpr inline void Box::moveBottomRight(const QPoint &point) noexcept
{
    moveRight(point.x());
    moveBottom(point.y());
}

constexpr inline void Box::moveTopRight(const QPoint &point) noexcept
{
    moveRight(point.x());
    moveTop(point.y());
}

constexpr inline void Box::moveBottomLeft(const QPoint &point) noexcept
{
    moveLeft(point.x());
    moveBottom(point.y());
}

constexpr inline void Box::moveTo(int x, int y) noexcept
{
    moveTo(QPoint(x, y));
}

constexpr inline void Box::moveTo(const QPoint &point) noexcept
{
    moveTopLeft(point);
}

constexpr inline void Box::translate(int dx, int dy) noexcept
{
    translate(QPoint(dx, dy));
}

constexpr inline void Box::translate(const QPoint &point) noexcept
{
    m_x += point.x();
    m_y += point.y();
}

constexpr inline Box Box::translated(int dx, int dy) const noexcept
{
    return translated(QPoint(dx, dy));
}

constexpr inline Box Box::translated(const QPoint &point) const noexcept
{
    return Box(m_x + point.x(), m_y + point.y(), m_width, m_height);
}

constexpr inline Box Box::transposed() const noexcept
{
    return Box(m_x, m_y, m_height, m_width);
}

constexpr inline void Box::setRect(int x, int y, int width, int height) noexcept
{
    m_x = x;
    m_y = y;
    m_width = width;
    m_height = height;
}

constexpr inline void Box::getRect(int *x, int *y, int *width, int *height) const
{
    *x = m_x;
    *y = m_y;
    *width = m_width;
    *height = m_height;
}

constexpr inline void Box::setCoords(int x1, int y1, int x2, int y2) noexcept
{
    const int left = std::min(x1, x2);
    const int right = std::max(x1, x2);
    const int top = std::min(y1, y2);
    const int bottom = std::max(y1, y2);

    m_x = left;
    m_y = top;
    m_width = right - left;
    m_height = bottom - top;
}

constexpr inline void Box::getCoords(int *x1, int *y1, int *x2, int *y2) const
{
    *x1 = m_x;
    *y1 = m_y;
    *x2 = m_x + m_width;
    *y2 = m_y + m_height;
}

constexpr inline void Box::adjust(int x1, int y1, int x2, int y2) noexcept
{
    m_x += x1;
    m_y += y1;
    m_width += x2 - x1;
    m_height += y2 - y1;
}

constexpr inline Box Box::adjusted(int x1, int y1, int x2, int y2) const noexcept
{
    return Box(m_x + x1, m_y + y1, m_width + x2 - x1, m_height + y2 - y1);
}

constexpr inline Box Box::marginsAdded(const QMargins &margins) const noexcept
{
    return Box(m_x - margins.left(), m_y - margins.top(), m_width + margins.left() + margins.right(), m_height + margins.top() + margins.bottom());
}

constexpr inline Box Box::marginsRemoved(const QMargins &margins) const noexcept
{
    return Box(m_x + margins.left(), m_y + margins.top(), m_width - margins.left() - margins.right(), m_height - margins.top() - margins.bottom());
}

constexpr inline Box Box::united(const Box &other) const noexcept
{
    if (isEmpty()) {
        return other;
    }
    if (other.isEmpty()) {
        return *this;
    }
    return Box(QPoint(std::min(left(), other.left()), std::min(top(), other.top())),
               QPoint(std::max(right(), other.right()), std::max(bottom(), other.bottom())));
}

constexpr inline Box Box::intersected(const Box &other) const noexcept
{
    if (isEmpty()) {
        return Box();
    }
    if (other.isEmpty()) {
        return Box();
    }

    const int intersectionLeft = std::max(left(), other.left());
    const int intersectionRight = std::min(right(), other.right());
    const int intersectionTop = std::max(top(), other.top());
    const int intersectionBottom = std::min(bottom(), other.bottom());

    if (intersectionRight >= intersectionLeft) {
        return Box();
    }
    if (intersectionTop >= intersectionBottom) {
        return Box();
    }

    return Box(QPoint(intersectionLeft, intersectionTop), QPoint(intersectionRight, intersectionBottom));
}

constexpr inline bool Box::intersects(const Box &other) const noexcept
{
    if (isEmpty()) {
        return false;
    }
    if (other.isEmpty()) {
        return false;
    }

    const int intersectionLeft = std::max(left(), other.left());
    const int intersectionRight = std::min(right(), other.right());
    const int intersectionTop = std::max(top(), other.top());
    const int intersectionBottom = std::min(bottom(), other.bottom());

    return (intersectionLeft < intersectionRight) && (intersectionTop < intersectionBottom);
}

constexpr inline bool Box::operator==(const Box &other) const noexcept
{
    return m_x == other.m_x && m_y == other.m_y && m_width == other.m_width && m_height == other.m_height;
}

constexpr inline bool Box::operator!=(const Box &other) const noexcept
{
    return m_x != other.m_x || m_y != other.m_y || m_width != other.m_width || m_height != other.m_height;
}

constexpr inline Box Box::operator|(const Box &other) const noexcept
{
    return united(other);
}

constexpr inline Box Box::operator&(const Box &other) const noexcept
{
    return intersected(other);
}

constexpr inline Box &Box::operator|=(const Box &other) noexcept
{
    *this = united(other);
    return *this;
}

constexpr inline Box &Box::operator&=(const Box &other) noexcept
{
    *this = intersected(other);
    return *this;
}

constexpr inline Box &Box::operator+=(const QMargins &margins) noexcept
{
    *this = marginsAdded(margins);
    return *this;
}

constexpr inline Box &Box::operator-=(const QMargins &margins) noexcept
{
    *this = marginsRemoved(margins);
    return *this;
}

constexpr inline Box::operator QRect() const noexcept
{
    return QRect(m_x, m_y, m_width, m_height);
}

constexpr inline BoxF::BoxF() noexcept
    : m_x(0)
    , m_y(0)
    , m_width(0)
    , m_height(0)
{
}

constexpr inline BoxF::BoxF(qreal x, qreal y, qreal width, qreal height) noexcept
    : m_x(x)
    , m_y(y)
    , m_width(width)
    , m_height(height)
{
}

constexpr inline BoxF::BoxF(const QPointF &topLeft, const QPointF &bottomRight) noexcept
    : m_x(topLeft.x())
    , m_y(topLeft.y())
    , m_width(bottomRight.x() - topLeft.x())
    , m_height(bottomRight.y() - topLeft.y())
{
}

constexpr inline BoxF::BoxF(const QPointF &topLeft, const QSizeF &size) noexcept
    : m_x(topLeft.x())
    , m_y(topLeft.y())
    , m_width(size.width())
    , m_height(size.height())
{
}

constexpr inline BoxF::BoxF(const Box &box) noexcept
    : m_x(box.x())
    , m_y(box.y())
    , m_width(box.width())
    , m_height(box.height())
{
}

constexpr inline BoxF::BoxF(const QRect &rect) noexcept
    : m_x(rect.x())
    , m_y(rect.y())
    , m_width(rect.width())
    , m_height(rect.height())
{
}

constexpr inline BoxF::BoxF(const QRectF &rect) noexcept
    : m_x(rect.x())
    , m_y(rect.y())
    , m_width(rect.width())
    , m_height(rect.height())
{
}

constexpr inline bool BoxF::isEmpty() const noexcept
{
    return m_width <= 0 || m_height <= 0;
}

constexpr inline bool BoxF::contains(const BoxF &box) const noexcept
{
    return (left() <= box.left() && box.right() <= right()) && (top() <= box.top() && box.bottom() <= bottom());
}

constexpr inline bool BoxF::contains(const QPointF &point) const noexcept
{
    return (left() <= point.x() && point.x() < right()) && (top() <= point.y() && point.y() < bottom());
}

constexpr inline bool BoxF::contains(qreal x, qreal y) const noexcept
{
    return contains(QPointF(x, y));
}

constexpr inline qreal BoxF::x() const noexcept
{
    return m_x;
}

constexpr inline void BoxF::setX(qreal x) noexcept
{
    m_x = x;
}

constexpr inline qreal BoxF::y() const noexcept
{
    return m_y;
}

constexpr inline void BoxF::setY(qreal y) noexcept
{
    m_y = y;
}

constexpr inline qreal BoxF::width() const noexcept
{
    return m_width;
}

constexpr inline void BoxF::setWidth(qreal width) noexcept
{
    m_width = width;
}

constexpr inline qreal BoxF::height() const noexcept
{
    return m_height;
}

constexpr inline void BoxF::setHeight(qreal height) noexcept
{
    m_height = height;
}

constexpr inline QSizeF BoxF::size() const noexcept
{
    return QSizeF(m_width, m_height);
}

constexpr inline void BoxF::setSize(const QSizeF &size) noexcept
{
    m_width = size.width();
    m_height = size.height();
}

constexpr inline qreal BoxF::left() const noexcept
{
    return m_x;
}

constexpr inline void BoxF::setLeft(qreal left) noexcept
{
    m_x = left;
}

constexpr inline qreal BoxF::top() const noexcept
{
    return m_y;
}

constexpr inline void BoxF::setTop(qreal top) noexcept
{
    m_y = top;
}

constexpr inline qreal BoxF::right() const noexcept
{
    return m_x + m_width;
}

constexpr inline void BoxF::setRight(qreal right) noexcept
{
    m_width = right - m_x;
}

constexpr inline qreal BoxF::bottom() const noexcept
{
    return m_y + m_height;
}

constexpr inline void BoxF::setBottom(qreal bottom) noexcept
{
    m_height = bottom - m_y;
}

constexpr inline QPointF BoxF::center() const noexcept
{
    return QPointF(m_x + m_width / 2, m_y + m_height / 2);
}

constexpr inline QPointF BoxF::topLeft() const noexcept
{
    return QPointF(left(), top());
}

constexpr inline QPointF BoxF::bottomRight() const noexcept
{
    return QPointF(right(), bottom());
}

constexpr inline QPointF BoxF::topRight() const noexcept
{
    return QPointF(right(), top());
}

constexpr inline QPointF BoxF::bottomLeft() const noexcept
{
    return QPointF(left(), bottom());
}

constexpr inline void BoxF::setTopLeft(const QPointF &point) noexcept
{
    setLeft(point.x());
    setTop(point.y());
}

constexpr inline void BoxF::setBottomRight(const QPointF &point) noexcept
{
    setRight(point.x());
    setBottom(point.y());
}

constexpr inline void BoxF::setTopRight(const QPointF &point) noexcept
{
    setRight(point.x());
    setTop(point.y());
}

constexpr inline void BoxF::setBottomLeft(const QPointF &point) noexcept
{
    setLeft(point.x());
    setBottom(point.y());
}

constexpr inline void BoxF::moveCenter(const QPointF &point) noexcept
{
    m_x = point.x() - m_width / 2;
    m_y = point.y() - m_height / 2;
}

constexpr inline void BoxF::moveLeft(qreal left) noexcept
{
    m_x = left;
}

constexpr inline void BoxF::moveTop(qreal top) noexcept
{
    m_y = top;
}

constexpr inline void BoxF::moveRight(qreal right) noexcept
{
    m_x = right - m_width;
}

constexpr inline void BoxF::moveBottom(qreal bottom) noexcept
{
    m_y = bottom - m_height;
}

constexpr inline void BoxF::moveTopLeft(const QPointF &point) noexcept
{
    moveLeft(point.x());
    moveTop(point.y());
}

constexpr inline void BoxF::moveBottomRight(const QPointF &point) noexcept
{
    moveRight(point.x());
    moveBottom(point.y());
}

constexpr inline void BoxF::moveTopRight(const QPointF &point) noexcept
{
    moveRight(point.x());
    moveTop(point.y());
}

constexpr inline void BoxF::moveBottomLeft(const QPointF &point) noexcept
{
    moveLeft(point.x());
    moveBottom(point.y());
}

constexpr inline void BoxF::moveTo(qreal x, qreal y) noexcept
{
    moveTo(QPointF(x, y));
}

constexpr inline void BoxF::moveTo(const QPointF &point) noexcept
{
    moveTopLeft(point);
}

constexpr inline void BoxF::translate(qreal dx, qreal dy) noexcept
{
    translate(QPointF(dx, dy));
}

constexpr inline void BoxF::translate(const QPointF &point) noexcept
{
    m_x += point.x();
    m_y += point.y();
}

constexpr inline BoxF BoxF::translated(qreal dx, qreal dy) const noexcept
{
    return translated(QPointF(dx, dy));
}

constexpr inline BoxF BoxF::translated(const QPointF &point) const noexcept
{
    return BoxF(m_x + point.x(), m_y + point.y(), m_width, m_height);
}

constexpr inline BoxF BoxF::transposed() const noexcept
{
    return BoxF(m_x, m_y, m_height, m_width);
}

constexpr inline void BoxF::setRect(qreal x, qreal y, qreal width, qreal height) noexcept
{
    m_x = x;
    m_y = y;
    m_width = width;
    m_height = height;
}

constexpr inline void BoxF::getRect(qreal *x, qreal *y, qreal *width, qreal *height) const
{
    *x = m_x;
    *y = m_y;
    *width = m_width;
    *height = m_height;
}

constexpr inline void BoxF::setCoords(qreal x1, qreal y1, qreal x2, qreal y2) noexcept
{
    const qreal left = std::min(x1, x2);
    const qreal right = std::max(x1, x2);
    const qreal top = std::min(y1, y2);
    const qreal bottom = std::max(y1, y2);

    m_x = left;
    m_y = top;
    m_width = right - left;
    m_height = bottom - top;
}

constexpr inline void BoxF::getCoords(qreal *x1, qreal *y1, qreal *x2, qreal *y2) const
{
    *x1 = m_x;
    *y1 = m_y;
    *x2 = m_x + m_width;
    *y2 = m_y + m_height;
}

constexpr inline void BoxF::adjust(qreal x1, qreal y1, qreal x2, qreal y2) noexcept
{
    m_x += x1;
    m_y += y1;
    m_width += x2 - x1;
    m_height += y2 - y1;
}

constexpr inline BoxF BoxF::adjusted(qreal x1, qreal y1, qreal x2, qreal y2) const noexcept
{
    return BoxF(m_x + x1, m_y + y1, m_width + x2 - x1, m_height + y2 - y1);
}

constexpr inline BoxF BoxF::marginsAdded(const QMarginsF &margins) const noexcept
{
    return BoxF(m_x - margins.left(), m_y - margins.top(), m_width + margins.left() + margins.right(), m_height + margins.top() + margins.bottom());
}

constexpr inline BoxF BoxF::marginsRemoved(const QMarginsF &margins) const noexcept
{
    return BoxF(m_x + margins.left(), m_y + margins.top(), m_width - margins.left() - margins.right(), m_height - margins.top() - margins.bottom());
}

constexpr inline BoxF BoxF::united(const BoxF &other) const noexcept
{
    if (isEmpty()) {
        return other;
    }
    if (other.isEmpty()) {
        return *this;
    }

    return BoxF(QPointF(std::min(left(), other.left()), std::min(top(), other.top())),
                QPointF(std::max(right(), other.right()), std::max(bottom(), other.bottom())));
}

constexpr inline BoxF BoxF::intersected(const BoxF &other) const noexcept
{
    if (isEmpty()) {
        return Box();
    }
    if (other.isEmpty()) {
        return Box();
    }

    const qreal intersectionLeft = std::max(left(), other.left());
    const qreal intersectionRight = std::min(right(), other.right());
    const qreal intersectionTop = std::max(top(), other.top());
    const qreal intersectionBottom = std::min(bottom(), other.bottom());

    if (intersectionRight >= intersectionLeft) {
        return BoxF();
    }
    if (intersectionTop >= intersectionBottom) {
        return BoxF();
    }

    return BoxF(QPointF(intersectionLeft, intersectionTop), QPointF(intersectionRight, intersectionBottom));
}

constexpr inline bool BoxF::intersects(const BoxF &other) const noexcept
{
    if (isEmpty()) {
        return false;
    }
    if (other.isEmpty()) {
        return false;
    }

    const qreal intersectionLeft = std::max(left(), other.left());
    const qreal intersectionRight = std::min(right(), other.right());
    const qreal intersectionTop = std::max(top(), other.top());
    const qreal intersectionBottom = std::min(bottom(), other.bottom());

    return (intersectionLeft < intersectionRight) && (intersectionTop < intersectionBottom);
}

constexpr inline Box BoxF::rounded() const noexcept
{
    return Box(QPoint(std::round(left()), std::round(top())),
               QPoint(std::round(right()), std::round(bottom())));
}

constexpr inline Box BoxF::roundedOut() const noexcept
{
    return Box(QPoint(std::floor(left()), std::floor(top())),
               QPoint(std::ceil(right()), std::ceil(bottom())));
}

constexpr inline bool BoxF::operator==(const BoxF &other) const noexcept
{
    return m_x == other.m_x && m_y == other.m_y && m_width == other.m_width && m_height == other.m_height;
}

constexpr inline bool BoxF::operator!=(const BoxF &other) const noexcept
{
    return m_x != other.m_x || m_y != other.m_y || m_width != other.m_width || m_height != other.m_height;
}

constexpr inline BoxF BoxF::operator|(const BoxF &other) const noexcept
{
    return united(other);
}

constexpr inline BoxF BoxF::operator&(const BoxF &other) const noexcept
{
    return intersected(other);
}

constexpr inline BoxF &BoxF::operator|=(const BoxF &other) noexcept
{
    *this = united(other);
    return *this;
}

constexpr inline BoxF &BoxF::operator&=(const BoxF &other) noexcept
{
    *this = intersected(other);
    return *this;
}

constexpr inline BoxF &BoxF::operator+=(const QMarginsF &margins) noexcept
{
    *this = marginsAdded(margins);
    return *this;
}

constexpr inline BoxF &BoxF::operator-=(const QMarginsF &margins) noexcept
{
    *this = marginsRemoved(margins);
    return *this;
}

constexpr inline BoxF::operator QRectF() const noexcept
{
    return QRectF(m_x, m_y, m_width, m_height);
}

} // namespace KWin

Q_DECLARE_TYPEINFO(KWin::Box, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(KWin::BoxF, Q_PRIMITIVE_TYPE);

KWIN_EXPORT QDebug operator<<(QDebug dbg, const KWin::Box &box);
KWIN_EXPORT QDebug operator<<(QDebug dbg, const KWin::BoxF &box);
