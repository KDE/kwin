/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef KRECT_H
#define KRECT_H

#include <QMargins>
#include <QSize>
#include <QPoint>
#include <QRect>

#ifdef topLeft
#error krect.h must be included before any header file that defines topLeft
#endif

#if defined(Q_OS_DARWIN) || defined(Q_QDOC)
struct CGRect;
#endif

class Q_CORE_EXPORT KRect
{
public:
    Q_DECL_CONSTEXPR KRect() noexcept : x1(0), y1(0), x2(-1), y2(-1) {}
    Q_DECL_CONSTEXPR KRect(const QPoint &topleft, const QPoint &bottomright) noexcept;
    Q_DECL_CONSTEXPR KRect(const QPoint &topleft, const QSize &size) noexcept;
    Q_DECL_CONSTEXPR KRect(int left, int top, int width, int height) noexcept;
    Q_DECL_CONSTEXPR KRect(const QRect &rect) noexcept;

    Q_DECL_CONSTEXPR inline bool isNull() const noexcept;
    Q_DECL_CONSTEXPR inline bool isEmpty() const noexcept;
    Q_DECL_CONSTEXPR inline bool isValid() const noexcept;

    Q_DECL_CONSTEXPR inline int left() const noexcept;
    Q_DECL_CONSTEXPR inline int top() const noexcept;
    Q_DECL_CONSTEXPR inline int right() const noexcept;
    Q_DECL_CONSTEXPR inline int bottom() const noexcept;
    Q_REQUIRED_RESULT KRect normalized() const noexcept;

    Q_DECL_CONSTEXPR inline int x() const noexcept;
    Q_DECL_CONSTEXPR inline int y() const noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setLeft(int pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setTop(int pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setRight(int pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setBottom(int pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setX(int x) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setY(int y) noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void setTopLeft(const QPoint &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setBottomRight(const QPoint &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setTopRight(const QPoint &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setBottomLeft(const QPoint &p) noexcept;

    Q_DECL_CONSTEXPR inline QPoint topLeft() const noexcept;
    Q_DECL_CONSTEXPR inline QPoint bottomRight() const noexcept;
    Q_DECL_CONSTEXPR inline QPoint topRight() const noexcept;
    Q_DECL_CONSTEXPR inline QPoint bottomLeft() const noexcept;
    Q_DECL_CONSTEXPR inline QPoint center() const noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void moveLeft(int pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveTop(int pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveRight(int pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveBottom(int pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveTopLeft(const QPoint &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveBottomRight(const QPoint &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveTopRight(const QPoint &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveBottomLeft(const QPoint &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveCenter(const QPoint &p) noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void translate(int dx, int dy) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void translate(const QPoint &p) noexcept;
    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRect translated(int dx, int dy) const noexcept;
    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRect translated(const QPoint &p) const noexcept;
    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRect transposed() const noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void moveTo(int x, int t) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveTo(const QPoint &p) noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void setRect(int x, int y, int w, int h) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void getRect(int *x, int *y, int *w, int *h) const;

    Q_DECL_RELAXED_CONSTEXPR inline void setCoords(int x1, int y1, int x2, int y2) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void getCoords(int *x1, int *y1, int *x2, int *y2) const;

    Q_DECL_RELAXED_CONSTEXPR inline void adjust(int x1, int y1, int x2, int y2) noexcept;
    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRect adjusted(int x1, int y1, int x2, int y2) const noexcept;

    Q_DECL_CONSTEXPR inline QSize size() const noexcept;
    Q_DECL_CONSTEXPR inline int width() const noexcept;
    Q_DECL_CONSTEXPR inline int height() const noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setWidth(int w) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setHeight(int h) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setSize(const QSize &s) noexcept;

    KRect operator|(const KRect &r) const noexcept;
    KRect operator&(const KRect &r) const noexcept;
    inline KRect& operator|=(const KRect &r) noexcept;
    inline KRect& operator&=(const KRect &r) noexcept;

    bool contains(const KRect &r, bool proper = false) const noexcept;
    bool contains(const QPoint &p, bool proper=false) const noexcept;
    inline bool contains(int x, int y) const noexcept;
    inline bool contains(int x, int y, bool proper) const noexcept;
    Q_REQUIRED_RESULT inline KRect united(const KRect &other) const noexcept;
    Q_REQUIRED_RESULT inline KRect intersected(const KRect &other) const noexcept;
    bool intersects(const KRect &r) const noexcept;

    Q_DECL_CONSTEXPR inline KRect marginsAdded(const QMargins &margins) const noexcept;
    Q_DECL_CONSTEXPR inline KRect marginsRemoved(const QMargins &margins) const noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline KRect &operator+=(const QMargins &margins) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline KRect &operator-=(const QMargins &margins) noexcept;

    friend Q_DECL_CONSTEXPR inline bool operator==(const KRect &, const KRect &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator!=(const KRect &, const KRect &) noexcept;

    friend Q_DECL_CONSTEXPR inline bool operator==(const KRect &, const QRect &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator!=(const KRect &, const QRect &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator==(const QRect &, const KRect &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator!=(const QRect &, const KRect &) noexcept;

#if defined(Q_OS_DARWIN) || defined(Q_QDOC)
    Q_REQUIRED_RESULT CGRect toCGRect() const noexcept;
#endif

    Q_DECL_CONSTEXPR inline operator QRect() const noexcept;

private:
    int x1;
    int y1;
    int x2;
    int y2;
};
Q_DECLARE_TYPEINFO(KRect, Q_MOVABLE_TYPE);

Q_DECL_CONSTEXPR inline bool operator==(const KRect &, const KRect &) noexcept;
Q_DECL_CONSTEXPR inline bool operator!=(const KRect &, const KRect &) noexcept;

Q_DECL_CONSTEXPR inline bool operator==(const KRect &, const QRect &) noexcept;
Q_DECL_CONSTEXPR inline bool operator!=(const KRect &, const QRect &) noexcept;
Q_DECL_CONSTEXPR inline bool operator==(const QRect &, const KRect &) noexcept;
Q_DECL_CONSTEXPR inline bool operator!=(const QRect &, const KRect &) noexcept;

/*****************************************************************************
  KRect stream functions
 *****************************************************************************/
#ifndef QT_NO_DATASTREAM
Q_CORE_EXPORT QDataStream &operator<<(QDataStream &, const KRect &);
Q_CORE_EXPORT QDataStream &operator>>(QDataStream &, KRect &);
#endif

/*****************************************************************************
  KRect inline member functions
 *****************************************************************************/

Q_DECL_CONSTEXPR inline KRect::KRect(int aleft, int atop, int awidth, int aheight) noexcept
    : x1(aleft), y1(atop), x2(aleft + awidth - 1), y2(atop + aheight - 1) {}

Q_DECL_CONSTEXPR inline KRect::KRect(const QPoint &atopLeft, const QPoint &abottomRight) noexcept
    : x1(atopLeft.x()), y1(atopLeft.y()), x2(abottomRight.x()), y2(abottomRight.y()) {}

Q_DECL_CONSTEXPR inline KRect::KRect(const QPoint &atopLeft, const QSize &asize) noexcept
    : x1(atopLeft.x()), y1(atopLeft.y()), x2(atopLeft.x()+asize.width() - 1), y2(atopLeft.y()+asize.height() - 1) {}

Q_DECL_CONSTEXPR inline KRect::KRect(const QRect &arect) noexcept
    : x1(arect.left()), y1(arect.top()), x2(arect.right()), y2(arect.bottom()) {}

Q_DECL_CONSTEXPR inline bool KRect::isNull() const noexcept
{ return x2 == x1 - 1 && y2 == y1 - 1; }

Q_DECL_CONSTEXPR inline bool KRect::isEmpty() const noexcept
{ return x1 > x2 || y1 > y2; }

Q_DECL_CONSTEXPR inline bool KRect::isValid() const noexcept
{ return x1 <= x2 && y1 <= y2; }

Q_DECL_CONSTEXPR inline int KRect::left() const noexcept
{ return x1; }

Q_DECL_CONSTEXPR inline int KRect::top() const noexcept
{ return y1; }

Q_DECL_CONSTEXPR inline int KRect::right() const noexcept
{ return x2; }

Q_DECL_CONSTEXPR inline int KRect::bottom() const noexcept
{ return y2; }

Q_DECL_CONSTEXPR inline int KRect::x() const noexcept
{ return x1; }

Q_DECL_CONSTEXPR inline int KRect::y() const noexcept
{ return y1; }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setLeft(int pos) noexcept
{ x1 = pos; }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setTop(int pos) noexcept
{ y1 = pos; }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setRight(int pos) noexcept
{ x2 = pos; }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setBottom(int pos) noexcept
{ y2 = pos; }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setTopLeft(const QPoint &p) noexcept
{ x1 = p.x(); y1 = p.y(); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setBottomRight(const QPoint &p) noexcept
{ x2 = p.x(); y2 = p.y(); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setTopRight(const QPoint &p) noexcept
{ x2 = p.x(); y1 = p.y(); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setBottomLeft(const QPoint &p) noexcept
{ x1 = p.x(); y2 = p.y(); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setX(int ax) noexcept
{ x1 = ax; }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setY(int ay) noexcept
{ y1 = ay; }

Q_DECL_CONSTEXPR inline QPoint KRect::topLeft() const noexcept
{ return QPoint(x1, y1); }

Q_DECL_CONSTEXPR inline QPoint KRect::bottomRight() const noexcept
{ return QPoint(x2, y2); }

Q_DECL_CONSTEXPR inline QPoint KRect::topRight() const noexcept
{ return QPoint(x2, y1); }

Q_DECL_CONSTEXPR inline QPoint KRect::bottomLeft() const noexcept
{ return QPoint(x1, y2); }

Q_DECL_CONSTEXPR inline QPoint KRect::center() const noexcept
{ return QPoint(x1 + (x2 - x1) / 2, y1 + (y2 - y1) / 2); }

Q_DECL_CONSTEXPR inline int KRect::width() const noexcept
{ return  x2 - x1 + 1; }

Q_DECL_CONSTEXPR inline int KRect::height() const noexcept
{ return  y2 - y1 + 1; }

Q_DECL_CONSTEXPR inline QSize KRect::size() const noexcept
{ return QSize(width(), height()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::translate(int dx, int dy) noexcept
{
    x1 += dx;
    y1 += dy;
    x2 += dx;
    y2 += dy;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::translate(const QPoint &p) noexcept
{
    x1 += p.x();
    y1 += p.y();
    x2 += p.x();
    y2 += p.y();
}

Q_DECL_CONSTEXPR inline KRect KRect::translated(int dx, int dy) const noexcept
{ return KRect(QPoint(x1 + dx, y1 + dy), QPoint(x2 + dx, y2 + dy)); }

Q_DECL_CONSTEXPR inline KRect KRect::translated(const QPoint &p) const noexcept
{ return KRect(QPoint(x1 + p.x(), y1 + p.y()), QPoint(x2 + p.x(), y2 + p.y())); }

Q_DECL_CONSTEXPR inline KRect KRect::transposed() const noexcept
{ return KRect(topLeft(), size().transposed()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveTo(int ax, int ay) noexcept
{
    x2 += ax - x1;
    y2 += ay - y1;
    x1 = ax;
    y1 = ay;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveTo(const QPoint &p) noexcept
{
    x2 += p.x() - x1;
    y2 += p.y() - y1;
    x1 = p.x();
    y1 = p.y();
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveLeft(int pos) noexcept
{ x2 += (pos - x1); x1 = pos; }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveTop(int pos) noexcept
{ y2 += (pos - y1); y1 = pos; }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveRight(int pos) noexcept
{
    x1 += (pos - x2);
    x2 = pos;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveBottom(int pos) noexcept
{
    y1 += (pos - y2);
    y2 = pos;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveTopLeft(const QPoint &p) noexcept
{
    moveLeft(p.x());
    moveTop(p.y());
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveBottomRight(const QPoint &p) noexcept
{
    moveRight(p.x());
    moveBottom(p.y());
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveTopRight(const QPoint &p) noexcept
{
    moveRight(p.x());
    moveTop(p.y());
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveBottomLeft(const QPoint &p) noexcept
{
    moveLeft(p.x());
    moveBottom(p.y());
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::moveCenter(const QPoint &p) noexcept
{
    int w = x2 - x1;
    int h = y2 - y1;
    x1 = p.x() - w/2;
    y1 = p.y() - h/2;
    x2 = x1 + w;
    y2 = y1 + h;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::getRect(int *ax, int *ay, int *aw, int *ah) const
{
    *ax = x1;
    *ay = y1;
    *aw = x2 - x1 + 1;
    *ah = y2 - y1 + 1;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setRect(int ax, int ay, int aw, int ah) noexcept
{
    x1 = ax;
    y1 = ay;
    x2 = (ax + aw - 1);
    y2 = (ay + ah - 1);
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::getCoords(int *xp1, int *yp1, int *xp2, int *yp2) const
{
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setCoords(int xp1, int yp1, int xp2, int yp2) noexcept
{
    x1 = xp1;
    y1 = yp1;
    x2 = xp2;
    y2 = yp2;
}

Q_DECL_CONSTEXPR inline KRect KRect::adjusted(int xp1, int yp1, int xp2, int yp2) const noexcept
{ return KRect(QPoint(x1 + xp1, y1 + yp1), QPoint(x2 + xp2, y2 + yp2)); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::adjust(int dx1, int dy1, int dx2, int dy2) noexcept
{
    x1 += dx1;
    y1 += dy1;
    x2 += dx2;
    y2 += dy2;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setWidth(int w) noexcept
{ x2 = (x1 + w - 1); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setHeight(int h) noexcept
{ y2 = (y1 + h - 1); }

Q_DECL_RELAXED_CONSTEXPR inline void KRect::setSize(const QSize &s) noexcept
{
    x2 = (s.width()  + x1 - 1);
    y2 = (s.height() + y1 - 1);
}

inline bool KRect::contains(int ax, int ay, bool aproper) const noexcept
{
    return contains(QPoint(ax, ay), aproper);
}

inline bool KRect::contains(int ax, int ay) const noexcept
{
    return contains(QPoint(ax, ay), false);
}

inline KRect& KRect::operator|=(const KRect &r) noexcept
{
    *this = *this | r;
    return *this;
}

inline KRect& KRect::operator&=(const KRect &r) noexcept
{
    *this = *this & r;
    return *this;
}

inline KRect KRect::intersected(const KRect &other) const noexcept
{
    return *this & other;
}

inline KRect KRect::united(const KRect &r) const noexcept
{
    return *this | r;
}

Q_DECL_CONSTEXPR inline bool operator==(const KRect &r1, const KRect &r2) noexcept
{
    return r1.x1==r2.x1 && r1.x2==r2.x2 && r1.y1==r2.y1 && r1.y2==r2.y2;
}

Q_DECL_CONSTEXPR inline bool operator!=(const KRect &r1, const KRect &r2) noexcept
{
    return r1.x1!=r2.x1 || r1.x2!=r2.x2 || r1.y1!=r2.y1 || r1.y2!=r2.y2;
}

Q_DECL_CONSTEXPR inline bool operator==(const KRect &r1, const QRect &r2) noexcept
{
    return r1.x1==r2.left() && r1.x2==r2.right() && r1.y1==r2.top() && r1.y2==r2.bottom();
}

Q_DECL_CONSTEXPR inline bool operator!=(const KRect &r1, const QRect &r2) noexcept
{
    return r1.x1!=r2.left() || r1.x2!=r2.right() || r1.y1!=r2.top() || r1.y2!=r2.bottom();
}

Q_DECL_CONSTEXPR inline bool operator==(const QRect &r1, const KRect &r2) noexcept
{
    return r1.left()==r2.x1 && r1.right()==r2.x2 && r1.top()==r2.y1 && r1.bottom()==r2.y2;
}

Q_DECL_CONSTEXPR inline bool operator!=(const QRect &r1, const KRect &r2) noexcept
{
    return r1.left()!=r2.x1 || r1.right()!=r2.x2 || r1.top()!=r2.y1 || r1.bottom()!=r2.y2;
}

Q_DECL_CONSTEXPR inline KRect operator+(const KRect &rectangle, const QMargins &margins) noexcept
{
    return KRect(QPoint(rectangle.left() - margins.left(), rectangle.top() - margins.top()),
                 QPoint(rectangle.right() + margins.right(), rectangle.bottom() + margins.bottom()));
}

Q_DECL_CONSTEXPR inline KRect operator+(const QMargins &margins, const KRect &rectangle) noexcept
{
    return KRect(QPoint(rectangle.left() - margins.left(), rectangle.top() - margins.top()),
                 QPoint(rectangle.right() + margins.right(), rectangle.bottom() + margins.bottom()));
}

Q_DECL_CONSTEXPR inline KRect operator-(const KRect &lhs, const QMargins &rhs) noexcept
{
    return KRect(QPoint(lhs.left() + rhs.left(), lhs.top() + rhs.top()),
                 QPoint(lhs.right() - rhs.right(), lhs.bottom() - rhs.bottom()));
}

Q_DECL_CONSTEXPR inline KRect KRect::marginsAdded(const QMargins &margins) const noexcept
{
    return KRect(QPoint(x1 - margins.left(), y1 - margins.top()),
                 QPoint(x2 + margins.right(), y2 + margins.bottom()));
}

Q_DECL_CONSTEXPR inline KRect KRect::marginsRemoved(const QMargins &margins) const noexcept
{
    return KRect(QPoint(x1 + margins.left(), y1 + margins.top()),
                 QPoint(x2 - margins.right(), y2 - margins.bottom()));
}

Q_DECL_RELAXED_CONSTEXPR inline KRect &KRect::operator+=(const QMargins &margins) noexcept
{
    *this = marginsAdded(margins);
    return *this;
}

Q_DECL_RELAXED_CONSTEXPR inline KRect &KRect::operator-=(const QMargins &margins) noexcept
{
    *this = marginsRemoved(margins);
    return *this;
}

Q_DECL_CONSTEXPR inline KRect::operator QRect() const noexcept
{
    return QRect(QPoint(x1, y1), QPoint(x2, y2));
}

#ifndef QT_NO_DEBUG_STREAM
Q_CORE_EXPORT QDebug operator<<(QDebug, const KRect &);
#endif


class Q_CORE_EXPORT KRectF
{
public:
    Q_DECL_CONSTEXPR KRectF() noexcept : xp(0.), yp(0.), w(0.), h(0.) {}
    Q_DECL_CONSTEXPR KRectF(const QPointF &topleft, const QSizeF &size) noexcept;
    Q_DECL_CONSTEXPR KRectF(const QPointF &topleft, const QPointF &bottomRight) noexcept;
    Q_DECL_CONSTEXPR KRectF(qreal left, qreal top, qreal width, qreal height) noexcept;
    Q_DECL_CONSTEXPR KRectF(const KRect &rect) noexcept;
    Q_DECL_CONSTEXPR KRectF(const QRect &rect) noexcept;
    Q_DECL_CONSTEXPR KRectF(const QRectF &rect) noexcept;

    Q_DECL_CONSTEXPR inline bool isNull() const noexcept;
    Q_DECL_CONSTEXPR inline bool isEmpty() const noexcept;
    Q_DECL_CONSTEXPR inline bool isValid() const noexcept;
    Q_REQUIRED_RESULT KRectF normalized() const noexcept;

    Q_DECL_CONSTEXPR inline qreal left() const noexcept { return xp; }
    Q_DECL_CONSTEXPR inline qreal top() const noexcept { return yp; }
    Q_DECL_CONSTEXPR inline qreal right() const noexcept { return xp + w; }
    Q_DECL_CONSTEXPR inline qreal bottom() const noexcept { return yp + h; }

    Q_DECL_CONSTEXPR inline qreal x() const noexcept;
    Q_DECL_CONSTEXPR inline qreal y() const noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setLeft(qreal pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setTop(qreal pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setRight(qreal pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setBottom(qreal pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setX(qreal pos) noexcept { setLeft(pos); }
    Q_DECL_RELAXED_CONSTEXPR inline void setY(qreal pos) noexcept { setTop(pos); }

    Q_DECL_CONSTEXPR inline QPointF topLeft() const noexcept { return QPointF(xp, yp); }
    Q_DECL_CONSTEXPR inline QPointF bottomRight() const noexcept { return QPointF(xp+w, yp+h); }
    Q_DECL_CONSTEXPR inline QPointF topRight() const noexcept { return QPointF(xp+w, yp); }
    Q_DECL_CONSTEXPR inline QPointF bottomLeft() const noexcept { return QPointF(xp, yp+h); }
    Q_DECL_CONSTEXPR inline QPointF center() const noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void setTopLeft(const QPointF &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setBottomRight(const QPointF &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setTopRight(const QPointF &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setBottomLeft(const QPointF &p) noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void moveLeft(qreal pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveTop(qreal pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveRight(qreal pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveBottom(qreal pos) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveTopLeft(const QPointF &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveBottomRight(const QPointF &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveTopRight(const QPointF &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveBottomLeft(const QPointF &p) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveCenter(const QPointF &p) noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void translate(qreal dx, qreal dy) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void translate(const QPointF &p) noexcept;

    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRectF translated(qreal dx, qreal dy) const noexcept;
    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRectF translated(const QPointF &p) const noexcept;

    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRectF transposed() const noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void moveTo(qreal x, qreal y) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void moveTo(const QPointF &p) noexcept;

    Q_DECL_RELAXED_CONSTEXPR inline void setRect(qreal x, qreal y, qreal w, qreal h) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void getRect(qreal *x, qreal *y, qreal *w, qreal *h) const;

    Q_DECL_RELAXED_CONSTEXPR inline void setCoords(qreal x1, qreal y1, qreal x2, qreal y2) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void getCoords(qreal *x1, qreal *y1, qreal *x2, qreal *y2) const;

    Q_DECL_RELAXED_CONSTEXPR inline void adjust(qreal x1, qreal y1, qreal x2, qreal y2) noexcept;
    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRectF adjusted(qreal x1, qreal y1, qreal x2, qreal y2) const noexcept;

    Q_DECL_CONSTEXPR inline QSizeF size() const noexcept;
    Q_DECL_CONSTEXPR inline qreal width() const noexcept;
    Q_DECL_CONSTEXPR inline qreal height() const noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setWidth(qreal w) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setHeight(qreal h) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline void setSize(const QSizeF &s) noexcept;

    KRectF operator|(const KRectF &r) const noexcept;
    KRectF operator&(const KRectF &r) const noexcept;
    inline KRectF& operator|=(const KRectF &r) noexcept;
    inline KRectF& operator&=(const KRectF &r) noexcept;

    bool contains(const KRectF &r) const noexcept;
    bool contains(const QPointF &p) const noexcept;
    inline bool contains(qreal x, qreal y) const noexcept;
    Q_REQUIRED_RESULT inline KRectF united(const KRectF &other) const noexcept;
    Q_REQUIRED_RESULT inline KRectF intersected(const KRectF &other) const noexcept;
    bool intersects(const KRectF &r) const noexcept;

    Q_DECL_CONSTEXPR inline KRectF marginsAdded(const QMarginsF &margins) const noexcept;
    Q_DECL_CONSTEXPR inline KRectF marginsRemoved(const QMarginsF &margins) const noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline KRectF &operator+=(const QMarginsF &margins) noexcept;
    Q_DECL_RELAXED_CONSTEXPR inline KRectF &operator-=(const QMarginsF &margins) noexcept;

    friend Q_DECL_CONSTEXPR inline bool operator==(const KRectF &, const KRectF &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &, const KRectF &) noexcept;

    friend Q_DECL_CONSTEXPR inline bool operator==(const KRectF &, const QRect &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &, const QRect &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator==(const QRect &, const KRectF &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator!=(const QRect &, const KRectF &) noexcept;

    friend Q_DECL_CONSTEXPR inline bool operator==(const KRectF &, const QRectF &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &, const QRectF &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator==(const QRectF &, const KRectF &) noexcept;
    friend Q_DECL_CONSTEXPR inline bool operator!=(const QRectF &, const KRectF &) noexcept;

    Q_DECL_CONSTEXPR inline operator QRectF() const noexcept;

    Q_REQUIRED_RESULT Q_DECL_CONSTEXPR inline KRect toRect() const noexcept;
    Q_REQUIRED_RESULT KRect toAlignedRect() const noexcept;

#if defined(Q_OS_DARWIN) || defined(Q_QDOC)
    Q_REQUIRED_RESULT static KRectF fromCGRect(CGRect rect) noexcept;
    Q_REQUIRED_RESULT CGRect toCGRect() const noexcept;
#endif

private:
    qreal xp;
    qreal yp;
    qreal w;
    qreal h;
};
Q_DECLARE_TYPEINFO(KRectF, Q_MOVABLE_TYPE);

Q_DECL_CONSTEXPR inline bool operator==(const KRectF &, const KRectF &) noexcept;
Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &, const KRectF &) noexcept;

Q_DECL_CONSTEXPR inline bool operator==(const KRectF &, const QRect &) noexcept;
Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &, const QRect &) noexcept;
Q_DECL_CONSTEXPR inline bool operator==(const QRect &, const KRectF &) noexcept;
Q_DECL_CONSTEXPR inline bool operator!=(const QRect &, const KRectF &) noexcept;

Q_DECL_CONSTEXPR inline bool operator==(const KRectF &, const QRectF &) noexcept;
Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &, const QRectF &) noexcept;
Q_DECL_CONSTEXPR inline bool operator==(const QRectF &, const KRectF &) noexcept;
Q_DECL_CONSTEXPR inline bool operator!=(const QRectF &, const KRectF &) noexcept;

/*****************************************************************************
  KRectF stream functions
 *****************************************************************************/
#ifndef QT_NO_DATASTREAM
Q_CORE_EXPORT QDataStream &operator<<(QDataStream &, const KRectF &);
Q_CORE_EXPORT QDataStream &operator>>(QDataStream &, KRectF &);
#endif

/*****************************************************************************
  KRectF inline member functions
 *****************************************************************************/

Q_DECL_CONSTEXPR inline KRectF::KRectF(qreal aleft, qreal atop, qreal awidth, qreal aheight) noexcept
    : xp(aleft), yp(atop), w(awidth), h(aheight)
{
}

Q_DECL_CONSTEXPR inline KRectF::KRectF(const QPointF &atopLeft, const QSizeF &asize) noexcept
    : xp(atopLeft.x()), yp(atopLeft.y()), w(asize.width()), h(asize.height())
{
}


Q_DECL_CONSTEXPR inline KRectF::KRectF(const QPointF &atopLeft, const QPointF &abottomRight) noexcept
    : xp(atopLeft.x()), yp(atopLeft.y()), w(abottomRight.x() - atopLeft.x()), h(abottomRight.y() - atopLeft.y())
{
}

Q_DECL_CONSTEXPR inline KRectF::KRectF(const KRect &r) noexcept
    : xp(r.x()), yp(r.y()), w(r.width()), h(r.height())
{
}

Q_DECL_CONSTEXPR inline KRectF::KRectF(const QRect &r) noexcept
    : xp(r.x()), yp(r.y()), w(r.width()), h(r.height())
{
}

Q_DECL_CONSTEXPR inline KRectF::KRectF(const QRectF &r) noexcept
    : xp(r.x()), yp(r.y()), w(r.width()), h(r.height())
{
}

QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wfloat-equal")
QT_WARNING_DISABLE_GCC("-Wfloat-equal")
QT_WARNING_DISABLE_INTEL(1572)

Q_DECL_CONSTEXPR inline bool KRectF::isNull() const noexcept
{ return w == 0. && h == 0.; }

Q_DECL_CONSTEXPR inline bool KRectF::isEmpty() const noexcept
{ return w <= 0. || h <= 0.; }

QT_WARNING_POP

Q_DECL_CONSTEXPR inline bool KRectF::isValid() const noexcept
{ return w > 0. && h > 0.; }

Q_DECL_CONSTEXPR inline qreal KRectF::x() const noexcept
{ return xp; }

Q_DECL_CONSTEXPR inline qreal KRectF::y() const noexcept
{ return yp; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setLeft(qreal pos) noexcept
{ qreal diff = pos - xp; xp += diff; w -= diff; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setRight(qreal pos) noexcept
{ w = pos - xp; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setTop(qreal pos) noexcept
{ qreal diff = pos - yp; yp += diff; h -= diff; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setBottom(qreal pos) noexcept
{ h = pos - yp; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setTopLeft(const QPointF &p) noexcept
{ setLeft(p.x()); setTop(p.y()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setTopRight(const QPointF &p) noexcept
{ setRight(p.x()); setTop(p.y()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setBottomLeft(const QPointF &p) noexcept
{ setLeft(p.x()); setBottom(p.y()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setBottomRight(const QPointF &p) noexcept
{ setRight(p.x()); setBottom(p.y()); }

Q_DECL_CONSTEXPR inline QPointF KRectF::center() const noexcept
{ return QPointF(xp + w/2, yp + h/2); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveLeft(qreal pos) noexcept
{ xp = pos; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveTop(qreal pos) noexcept
{ yp = pos; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveRight(qreal pos) noexcept
{ xp = pos - w; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveBottom(qreal pos) noexcept
{ yp = pos - h; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveTopLeft(const QPointF &p) noexcept
{ moveLeft(p.x()); moveTop(p.y()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveTopRight(const QPointF &p) noexcept
{ moveRight(p.x()); moveTop(p.y()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveBottomLeft(const QPointF &p) noexcept
{ moveLeft(p.x()); moveBottom(p.y()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveBottomRight(const QPointF &p) noexcept
{ moveRight(p.x()); moveBottom(p.y()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveCenter(const QPointF &p) noexcept
{ xp = p.x() - w/2; yp = p.y() - h/2; }

Q_DECL_CONSTEXPR inline qreal KRectF::width() const noexcept
{ return w; }

Q_DECL_CONSTEXPR inline qreal KRectF::height() const noexcept
{ return h; }

Q_DECL_CONSTEXPR inline QSizeF KRectF::size() const noexcept
{ return QSizeF(w, h); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::translate(qreal dx, qreal dy) noexcept
{
    xp += dx;
    yp += dy;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::translate(const QPointF &p) noexcept
{
    xp += p.x();
    yp += p.y();
}

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveTo(qreal ax, qreal ay) noexcept
{
    xp = ax;
    yp = ay;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::moveTo(const QPointF &p) noexcept
{
    xp = p.x();
    yp = p.y();
}

Q_DECL_CONSTEXPR inline KRectF KRectF::translated(qreal dx, qreal dy) const noexcept
{ return KRectF(xp + dx, yp + dy, w, h); }

Q_DECL_CONSTEXPR inline KRectF KRectF::translated(const QPointF &p) const noexcept
{ return KRectF(xp + p.x(), yp + p.y(), w, h); }

Q_DECL_CONSTEXPR inline KRectF KRectF::transposed() const noexcept
{ return KRectF(topLeft(), size().transposed()); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::getRect(qreal *ax, qreal *ay, qreal *aaw, qreal *aah) const
{
    *ax = this->xp;
    *ay = this->yp;
    *aaw = this->w;
    *aah = this->h;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setRect(qreal ax, qreal ay, qreal aaw, qreal aah) noexcept
{
    this->xp = ax;
    this->yp = ay;
    this->w = aaw;
    this->h = aah;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::getCoords(qreal *xp1, qreal *yp1, qreal *xp2, qreal *yp2) const
{
    *xp1 = xp;
    *yp1 = yp;
    *xp2 = xp + w;
    *yp2 = yp + h;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setCoords(qreal xp1, qreal yp1, qreal xp2, qreal yp2) noexcept
{
    xp = xp1;
    yp = yp1;
    w = xp2 - xp1;
    h = yp2 - yp1;
}

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::adjust(qreal xp1, qreal yp1, qreal xp2, qreal yp2) noexcept
{ xp += xp1; yp += yp1; w += xp2 - xp1; h += yp2 - yp1; }

Q_DECL_CONSTEXPR inline KRectF KRectF::adjusted(qreal xp1, qreal yp1, qreal xp2, qreal yp2) const noexcept
{ return KRectF(xp + xp1, yp + yp1, w + xp2 - xp1, h + yp2 - yp1); }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setWidth(qreal aw) noexcept
{ this->w = aw; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setHeight(qreal ah) noexcept
{ this->h = ah; }

Q_DECL_RELAXED_CONSTEXPR inline void KRectF::setSize(const QSizeF &s) noexcept
{
    w = s.width();
    h = s.height();
}

inline bool KRectF::contains(qreal ax, qreal ay) const noexcept
{
    return contains(QPointF(ax, ay));
}

inline KRectF& KRectF::operator|=(const KRectF &r) noexcept
{
    *this = *this | r;
    return *this;
}

inline KRectF& KRectF::operator&=(const KRectF &r) noexcept
{
    *this = *this & r;
    return *this;
}

inline KRectF KRectF::intersected(const KRectF &r) const noexcept
{
    return *this & r;
}

inline KRectF KRectF::united(const KRectF &r) const noexcept
{
    return *this | r;
}

Q_DECL_CONSTEXPR inline bool operator==(const KRectF &r1, const KRectF &r2) noexcept
{
    return qFuzzyCompare(r1.xp, r2.xp) && qFuzzyCompare(r1.yp, r2.yp)
           && qFuzzyCompare(r1.w, r2.w) && qFuzzyCompare(r1.h, r2.h);
}

Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &r1, const KRectF &r2) noexcept
{
    return !qFuzzyCompare(r1.xp, r2.xp) || !qFuzzyCompare(r1.yp, r2.yp)
           || !qFuzzyCompare(r1.w, r2.w) || !qFuzzyCompare(r1.h, r2.h);
}

Q_DECL_CONSTEXPR inline bool operator==(const KRectF &r1, const QRect &r2) noexcept
{
    return qFuzzyCompare(r1.xp, r2.x()) && qFuzzyCompare(r1.yp, r2.y())
           && qFuzzyCompare(r1.w, r2.width()) && qFuzzyCompare(r1.h, r2.height());
}

Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &r1, const QRect &r2) noexcept
{
    return !qFuzzyCompare(r1.xp, r2.x()) || !qFuzzyCompare(r1.yp, r2.y())
           || !qFuzzyCompare(r1.w, r2.width()) || !qFuzzyCompare(r1.h, r2.height());
}

Q_DECL_CONSTEXPR inline bool operator==(const QRect &r1, const KRectF &r2) noexcept
{
    return qFuzzyCompare(r1.x(), r2.xp) && qFuzzyCompare(r1.y(), r2.yp)
           && qFuzzyCompare(r1.width(), r2.w) && qFuzzyCompare(r1.height(), r2.h);
}

Q_DECL_CONSTEXPR inline bool operator!=(const QRect &r1, const KRectF &r2) noexcept
{
    return !qFuzzyCompare(r1.x(), r2.xp) || !qFuzzyCompare(r1.y(), r2.yp)
           || !qFuzzyCompare(r1.width(), r2.w) || !qFuzzyCompare(r1.height(), r2.h);
}

Q_DECL_CONSTEXPR inline bool operator==(const KRectF &r1, const QRectF &r2) noexcept
{
    return qFuzzyCompare(r1.xp, r2.x()) && qFuzzyCompare(r1.yp, r2.y())
           && qFuzzyCompare(r1.w, r2.width()) && qFuzzyCompare(r1.h, r2.height());
}

Q_DECL_CONSTEXPR inline bool operator!=(const KRectF &r1, const QRectF &r2) noexcept
{
    return !qFuzzyCompare(r1.xp, r2.x()) || !qFuzzyCompare(r1.yp, r2.y())
           || !qFuzzyCompare(r1.w, r2.width()) || !qFuzzyCompare(r1.h, r2.height());
}

Q_DECL_CONSTEXPR inline bool operator==(const QRectF &r1, const KRectF &r2) noexcept
{
    return qFuzzyCompare(r1.x(), r2.xp) && qFuzzyCompare(r1.y(), r2.yp)
           && qFuzzyCompare(r1.width(), r2.w) && qFuzzyCompare(r1.height(), r2.h);
}

Q_DECL_CONSTEXPR inline bool operator!=(const QRectF &r1, const KRectF &r2) noexcept
{
    return !qFuzzyCompare(r1.x(), r2.xp) || !qFuzzyCompare(r1.y(), r2.yp)
           || !qFuzzyCompare(r1.width(), r2.w) || !qFuzzyCompare(r1.height(), r2.h);
}

Q_DECL_CONSTEXPR inline KRectF::operator QRectF() const noexcept
{
    return QRectF(xp, yp, w, h);
}

Q_DECL_CONSTEXPR inline KRect KRectF::toRect() const noexcept
{
    return KRect(QPoint(qRound(xp), qRound(yp)), QPoint(qRound(xp + w) - 1, qRound(yp + h) - 1));
}

Q_DECL_CONSTEXPR inline KRectF operator+(const KRectF &lhs, const QMarginsF &rhs) noexcept
{
    return KRectF(QPointF(lhs.left() - rhs.left(), lhs.top() - rhs.top()),
                  QSizeF(lhs.width() + rhs.left() + rhs.right(), lhs.height() + rhs.top() + rhs.bottom()));
}

Q_DECL_CONSTEXPR inline KRectF operator+(const QMarginsF &lhs, const KRectF &rhs) noexcept
{
    return KRectF(QPointF(rhs.left() - lhs.left(), rhs.top() - lhs.top()),
                  QSizeF(rhs.width() + lhs.left() + lhs.right(), rhs.height() + lhs.top() + lhs.bottom()));
}

Q_DECL_CONSTEXPR inline KRectF operator-(const KRectF &lhs, const QMarginsF &rhs) noexcept
{
    return KRectF(QPointF(lhs.left() + rhs.left(), lhs.top() + rhs.top()),
                  QSizeF(lhs.width() - rhs.left() - rhs.right(), lhs.height() - rhs.top() - rhs.bottom()));
}

Q_DECL_CONSTEXPR inline KRectF KRectF::marginsAdded(const QMarginsF &margins) const noexcept
{
    return KRectF(QPointF(xp - margins.left(), yp - margins.top()),
                  QSizeF(w + margins.left() + margins.right(), h + margins.top() + margins.bottom()));
}

Q_DECL_CONSTEXPR inline KRectF KRectF::marginsRemoved(const QMarginsF &margins) const noexcept
{
    return KRectF(QPointF(xp + margins.left(), yp + margins.top()),
                  QSizeF(w - margins.left() - margins.right(), h - margins.top() - margins.bottom()));
}

Q_DECL_RELAXED_CONSTEXPR inline KRectF &KRectF::operator+=(const QMarginsF &margins) noexcept
{
    *this = marginsAdded(margins);
    return *this;
}

Q_DECL_RELAXED_CONSTEXPR inline KRectF &KRectF::operator-=(const QMarginsF &margins) noexcept
{
    *this = marginsRemoved(margins);
    return *this;
}

#ifndef QT_NO_DEBUG_STREAM
Q_CORE_EXPORT QDebug operator<<(QDebug, const KRectF &);
#endif

#endif // KRECT_H
