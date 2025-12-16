/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QDataStream>
#include <QObject>
#include <QRect>

namespace KWin
{

class RectF;

/*!
 * The Rect type represents a rectangle with integer coordinates.
 *
 * \note Unlike the QRect, the right() and the bottom() edges of a Rect correspond to \c{x() + width()},
 * and \c{y() + height()}, respectively.
 */
class KWIN_EXPORT Rect
{
    Q_GADGET
    Q_PROPERTY(int x READ x WRITE setX)
    Q_PROPERTY(int y READ y WRITE setY)
    Q_PROPERTY(int width READ width WRITE setWidth)
    Q_PROPERTY(int height READ height WRITE setHeight)

public:
    /*!
     * Constructs an empty rectangle.
     */
    constexpr Rect() noexcept;

    /*!
     * Constructs a rectangle with the specified coordinates (\a x, and \a y), and the size
     * (\a width, and \a height).
     */
    constexpr Rect(int x, int y, int width, int height) noexcept;

    /*!
     * Constructs a rectangle with the specified \a topLeft and \a bottomRight corners.
     */
    constexpr Rect(const QPoint &topLeft, const QPoint &bottomRight) noexcept;

    /*!
     * Constructs a rectangle with the specified \a topLeft position and \a size.
     */
    constexpr Rect(const QPoint &topLeft, const QSize &size) noexcept;

    /*!
     * Constructs a Rect from the given QRect \a rect.
     */
    constexpr Rect(const QRect &rect) noexcept;

    /*!
     * Constructs an uninitialized Rect.
     */
    constexpr explicit Rect(Qt::Initialization) noexcept;

    /*!
     * Returns \c true if the rectangle is empty; otherwise returns \c false. An empty rectangle has
     * left() >= right() or top() >= bottom() or both.
     *
     * \sa isNull(), isValid()
     */
    constexpr inline bool isEmpty() const noexcept;

    /*!
     * Returns \c true if the rectangle is empty; otherwise returns \c false. An empty rectangle has
     * left() >= right() or top() >= bottom() or both.
     *
     * This function is provided for compatibility reasons with QRect. It is identical to isEmpty().
     * Prefer using isEmpty().
     *
     * \sa isEmpty(), isValid()
     */
    constexpr inline bool isNull() const noexcept;

    /*!
     * Returns \c true if the rectangle is empty; otherwise returns \c false. An empty rectangle has
     * left() >= right() or top() >= bottom() or both.
     *
     * This function is provided for compatibility reasons with QRect. It is identical to isEmpty().
     * Prefer using isEmpty().
     *
     * \sa isEmpty(), isNull()
     */
    constexpr inline bool isValid() const noexcept;

    /*!
     * Returns \c true if the specified \a rect is inside this rectangle; otherwise returns \c false.
     * If the rectangle is empty, this function returns \c false.
     */
    constexpr inline bool contains(const Rect &rect) const noexcept;

    /*!
     * Returns \c true if the specified \a point is inside this rectangle; otherwise returns \c false.
     * If the rectangle is empty, this function returns \c false.
     *
     * The left and top edges are considered to be inside the rectangle, while the right and bottom
     * edges are not. In other words, this checks \c{left() <= point.x() < right()}, and
     * \c{top() <= point.y() < bottom()}.
     */
    constexpr inline bool contains(const QPoint &point) const noexcept;

    /*!
     * \overload
     */
    constexpr inline bool contains(int x, int y) const noexcept;

    /*!
     * Returns the left edge coordinate. Equivalent to left().
     *
     * \sa left(), setX()
     */
    constexpr inline int x() const noexcept;

    /*!
     * Sets the left edge coordinate to \a x. This may change the width of the rectangle. Equivalent
     * to setLeft().
     *
     * \sa x(), setLeft()
     */
    constexpr inline void setX(int x) noexcept;

    /*!
     * Returns the top edge coordinate. Equivalent to top().
     *
     * \sa top(), setY()
     */
    constexpr inline int y() const noexcept;

    /*!
     * Sets the top edge coordinate to \a y. This may change the height of the rectangle. Equivalent
     * to setTop().
     *
     * \sa y(), setTop()
     */
    constexpr inline void setY(int y) noexcept;

    /*!
     * Returns the width of the rectangle.
     *
     * \sa setWidth()
     */
    constexpr inline int width() const noexcept;

    /*!
     * Sets the width of the rectangle to \a width.
     *
     * \sa width()
     */
    constexpr inline void setWidth(int width) noexcept;

    /*!
     * Returns the height of the rectangle.
     *
     * \sa setHeight().
     */
    constexpr inline int height() const noexcept;

    /*!
     * Sets the height of the rectangle to \a height.
     *
     * \sa height()
     */
    constexpr inline void setHeight(int height) noexcept;

    /*!
     * Returns the size of the rectangle.
     *
     * \sa setSize()
     */
    constexpr inline QSize size() const noexcept;

    /*!
     * Sets the size of the rectangle to \a size.
     *
     * \sa size()
     */
    constexpr inline void setSize(const QSize &size) noexcept;

    /*!
     * Returns the left edge coordinate. Equivalent to x().
     *
     * \sa x(), setLeft(), moveLeft()
     */
    constexpr inline int left() const noexcept;

    /*!
     * Sets the left edge coordinate to \a left. This may change the width of the rectangle.
     *
     * \sa left(), moveLeft(), setX()
     */
    constexpr inline void setLeft(int left) noexcept;

    /*!
     * Returns the top edge coordinate. Equivalent to y().
     *
     * \sa y(), setTop(), moveTop()
     */
    constexpr inline int top() const noexcept;

    /*!
     * Sets the top edge coordinate to \a top. This may change the height of the rectangle.
     *
     * \sa top(), moveTop(), setY()
     */
    constexpr inline void setTop(int top) noexcept;

    /*!
     * Returns the right edge coordinate. This equivalent to \c{x() + width()}.
     *
     * \sa setRight(), moveRight()
     */
    constexpr inline int right() const noexcept;

    /*!
     * Sets the right edge coordinate to \a right. This may change the width of the rectangle.
     *
     * \sa right(), moveRight()
     */
    constexpr inline void setRight(int right) noexcept;

    /*!
     * Returns the bottom edge coordinate. This equivalent to \c{y() + height()}.
     *
     * \sa setBottom(), moveBottom()
     */
    constexpr inline int bottom() const noexcept;

    /*!
     * Sets the bottom edge coordinate to \a bottom. This may change the height of the rectangle.
     *
     * \sa bottom(), moveBottom()
     */
    constexpr inline void setBottom(int bottom) noexcept;

    /*!
     * Returns the center point of the rectangle.
     *
     * \sa moveCenter()
     */
    constexpr inline QPoint center() const noexcept;

    /*!
     * Returns the top-left corner of the rectangle.
     *
     * \sa setTopLeft(), moveTopLeft()
     */
    constexpr inline QPoint topLeft() const noexcept;

    /*!
     * Returns the bottom-right corner of the rectangle.
     *
     * \sa setBottomRight(), moveBottomRight()
     */
    constexpr inline QPoint bottomRight() const noexcept;

    /*!
     * Returns the top-right corner of the rectangle.
     *
     * \sa setTopRight(), moveTopRight()
     */
    constexpr inline QPoint topRight() const noexcept;

    /*!
     * Returns the bottom-left corner of the rectangle.
     *
     * \sa setBottomLeft(), moveBottomLeft()
     */
    constexpr inline QPoint bottomLeft() const noexcept;

    /*!
     * Sets the top-left corner coordinates to \a point. This may change the width and the height
     * of the rectangle.
     *
     * \sa topLeft(), moveTopLeft()
     */
    constexpr inline void setTopLeft(const QPoint &point) noexcept;

    /*!
     * Sets the bottom-right corner coordinates to \a point. This may change the width and the height
     * of the rectangle.
     *
     * \sa bottomRight(), moveBottomRight()
     */
    constexpr inline void setBottomRight(const QPoint &point) noexcept;

    /*!
     * Sets the top-right corner coordinates to \a point. This may change the width and the height
     * of the rectangle.
     *
     * \sa topRight(), moveTopRight()
     */
    constexpr inline void setTopRight(const QPoint &point) noexcept;

    /*!
     * Sets the bottom-left corner coordinates to \a point. This may change the width and the height
     * of the rectangle.
     *
     * \sa bottomLeft(), moveBottomLeft()
     */
    constexpr inline void setBottomLeft(const QPoint &point) noexcept;

    /*!
     * Moves the rectangle so its center point is at the specified position \a point. This does not
     * change the size of the rectangle.
     *
     * \sa center()
     */
    constexpr inline void moveCenter(const QPoint &point) noexcept;

    /*!
     * Moves the rectangle horizontally so its left edge is left at the specified position \a left.
     * The size of the rectangle won't be changed.
     *
     * \sa left(), setLeft()
     */
    constexpr inline void moveLeft(int left) noexcept;

    /*!
     * Moves the rectangle vertically so its top edge is left at the specified position \a top.
     * The size of the rectangle won't be changed.
     *
     * \sa top(), setTop()
     */
    constexpr inline void moveTop(int top) noexcept;

    /*!
     * Moves the rectangle horizontally so its right edge is left at the specified position \a right.
     * The size of the rectangle won't be changed.
     *
     * \sa right(), setRight()
     */
    constexpr inline void moveRight(int right) noexcept;

    /*!
     * Moves the rectangle vertically so its bottom edge is left at the specified position \a bottom.
     * The size of the rectangle won't be changed.
     *
     * \sa bottom(), setBottom()
     */
    constexpr inline void moveBottom(int bottom) noexcept;

    /*!
     * Moves the rectangle so its top-left corner is left at the specified position \a point. The size
     * of the rectangle won't be changed.
     *
     * \sa topLeft(), setTopLeft()
     */
    constexpr inline void moveTopLeft(const QPoint &point) noexcept;

    /*!
     * Moves the rectangle so its bottom-right corner is left at the specified position \a point. The size
     * of the rectangle won't be changed.
     *
     * \sa bottomRight(), setBottomRight()
     */
    constexpr inline void moveBottomRight(const QPoint &point) noexcept;

    /*!
     * Moves the rectangle so its top-right corner is left at the specified position \a point. The size
     * of the rectangle won't be changed.
     *
     * \sa topRight(), setTopRight()
     */
    constexpr inline void moveTopRight(const QPoint &point) noexcept;

    /*!
     * Moves the rectangle so its bottom-left corner is left at the specified position \a point. The size
     * of the rectangle won't be changed.
     *
     * \sa bottomLeft(), setBottomLeft()
     */
    constexpr inline void moveBottomLeft(const QPoint &point) noexcept;

    /*!
     * \overload
     */
    constexpr inline void moveTo(int x, int y) noexcept;

    /*!
     * Moves the top-left corner of the rectangle to the specified \a point. Equivalent to moveTopLeft().
     *
     * \sa moveTopLeft()
     */
    constexpr inline void moveTo(const QPoint &point) noexcept;

    /*!
     * Returns a copy of this rectangle that has its width and height swapped.
     */
    constexpr inline Rect transposed() const noexcept;

    /*!
     * \overload
     */
    constexpr inline void translate(int dx, int dy) noexcept;

    /*!
     * Moves the rectangle by adding the given \a offset to the rectangle coordinates. The size
     * will be left unchanged.
     */
    constexpr inline void translate(const QPoint &offset) noexcept;

    /*!
     * \overload
     */
    constexpr inline Rect translated(int dx, int dy) const noexcept;

    /*!
     * Returns a copy of this rectangle with the given \a offset added to its coordinates. The size
     * will be left unchanged.
     */
    constexpr inline Rect translated(const QPoint &offset) const noexcept;

    /*!
     * Returns a copy of this rectangle with its x and y coordinates scaled by the given \a scale factor.
     */
    constexpr inline RectF scaled(qreal scale) const noexcept;

    /*!
     * Returns a copy of this rectangle with its x coordinates scaled by the given \a xScale factor and
     * the y coordinates scaled by the \a yScale factor.
     */
    constexpr inline RectF scaled(qreal xScale, qreal yScale) const noexcept;

    /*!
     * Sets the rectangle's top-left corner position to (\a x, and \a y), and its size to (\a width, and \a height).
     *
     * This function is provided for compatibility reasons with QRect.
     *
     * \sa getRect()
     */
    constexpr inline void setRect(int x, int y, int width, int height) noexcept;

    /*!
     * Writes the rectangle's top-left corner position to \a x and \a y, and its size to \a width and \a height.
     *
     * This function is provided for compatibility reasons with QRect.
     *
     * \sa setRect()
     */
    constexpr inline void getRect(int *x, int *y, int *width, int *height) const;

    /*!
     * Sets the rectangle's top-left corner position to (\a x1, and \a y1), and its bottom-right corner
     * position to (\a x2, and \a y2).
     *
     * This function is provided for compatibility reasons with QRect.
     *
     * \sa getCoords()
     */
    constexpr inline void setCoords(int x1, int y1, int x2, int y2) noexcept;

    /*!
     * Writes the rectangle's top-left corner position to \a x1 and \a y1, and its bottom-right corner
     * position to \a x2 and \a y2.
     *
     * This function is provided for compatibility reasons with QRect.
     *
     * \sa setCoords()
     */
    constexpr inline void getCoords(int *x1, int *y1, int *x2, int *y2) const;

    /*!
     * Adds \a x1, \a y1, \a x2, and \a y2 to the left, top, right, and bottom edge coordinates,
     * respectively. This may change the size of the rectangle.
     *
     * This function is provided for compatibility reasons with QRect.
     *
     * Prefer using grownBy() or shrunkBy().
     *
     * \sa adjusted(), grownBy(), shrunkBy()
     */
    constexpr inline void adjust(int x1, int y1, int x2, int y2) noexcept;

    /*!
     * Returns a copy of this rectangle with \a x1, \a y1, \a x2, and \a y2 added to its left, top,
     * right, and bottom edge coordinates, respectively. This may change the size of the rectangle.
     *
     * This function is provided for compatibility reasons with QRect.
     *
     * Prefer using grownBy() or shrunkBy().
     *
     * \sa adjust(), grownBy(), shrunkBy()
     */
    constexpr inline Rect adjusted(int x1, int y1, int x2, int y2) const noexcept;

    /*!
     * Returns a copy of this rectangle grown by the given \a margins.
     *
     * \sa shrunkBy().
     */
    constexpr inline Rect grownBy(const QMargins &margins) const noexcept;

    /*!
     * Returns a copy of this rectangle shrunk by the given \a margins.
     *
     * \sa grownBy().
     */
    constexpr inline Rect shrunkBy(const QMargins &margins) const noexcept;

    /*!
     * Returns a copy of this rectangle grown by the given \a margins. Equivalent to grownBy().
     *
     * This function is provided for compatibility reasons with QRect.
     *
     * \sa shrunkBy(), marginsRemoved().
     */
    constexpr inline Rect marginsAdded(const QMargins &margins) const noexcept;

    /*!
     * Returns a copy of this rectangle shrunk by the given \a margins. Equivalent to shrunkBy().
     *
     * This function is provided for compatibility reasons with QRect.
     *
     * \sa grownBy(), marginsAdded().
     */
    constexpr inline Rect marginsRemoved(const QMargins &margins) const noexcept;

    /*!
     * Returns the bounding rectangle of this rectangle and the given \a other rectangle.
     */
    constexpr inline Rect united(const Rect &other) const noexcept;

    /*!
     * Returns the intersection of this rectangle and the given \a other rectangle. If the rectangles
     * don't intersect, an empty rectangle will be returned.
     *
     * \sa intersects()
     */
    constexpr inline Rect intersected(const Rect &other) const noexcept;

    /*!
     * Returns \c true if this rectangle and the given \a other rectangle intersect; otherwise returns \c false.
     *
     * \sa intersected()
     */
    constexpr inline bool intersects(const Rect &other) const noexcept;

    /*!
     * Returns \c true if this rectangle and the \a other rectangle are equal; otherwise returns \c false.
     */
    constexpr inline bool operator==(const Rect &other) const noexcept;

    /*!
     * Returns the bounding rectangle of this rectangle and the given \a other rectangle. Equivalent
     * to united().
     *
     * \sa united()
     */
    constexpr inline Rect operator|(const Rect &other) const noexcept;

    /*!
     * Returns the intersection of this rectangle and the given \a other rectangle. Equivalent to
     * intersected().
     *
     * \sa intersected()
     */
    constexpr inline Rect operator&(const Rect &other) const noexcept;

    /*!
     * Unites this rectangle with the given \a other rectangle.
     *
     * \sa united(), operator|()
     */
    constexpr inline Rect &operator|=(const Rect &other) noexcept;

    /*!
     * Intersects this rectangle with the given \a other rectangle.
     *
     * \sa intersected(), operator&()
     */
    constexpr inline Rect &operator&=(const Rect &other) noexcept;

    /*!
     * Grows this rectangle by the given \a margins.
     *
     * \sa grownBy().
     */
    constexpr inline Rect &operator+=(const QMargins &margins) noexcept;

    /*!
     * Shrinks this rectangle by the given \a margins.
     *
     * \sa shrunkBy().
     */
    constexpr inline Rect &operator-=(const QMargins &margins) noexcept;

    /*!
     * Implicitly converts this rectangle to QRect.
     */
    constexpr inline operator QRect() const noexcept;

private:
    int m_left;
    int m_top;
    int m_right;
    int m_bottom;
};

/*!
 * The RectF type represents a rectangle with floating point coordinates.
 */
class KWIN_EXPORT RectF
{
    Q_GADGET
    Q_PROPERTY(qreal x READ x WRITE setX)
    Q_PROPERTY(qreal y READ y WRITE setY)
    Q_PROPERTY(qreal width READ width WRITE setWidth)
    Q_PROPERTY(qreal height READ height WRITE setHeight)

public:
    /*!
     * Constructs an empty rectangle.
     */
    constexpr RectF() noexcept;

    /*!
     * Constructs a rectangle with the specified coordinates (\a x, and \a y), and the size
     * (\a width, and \a height).
     */
    constexpr RectF(qreal x, qreal y, qreal width, qreal height) noexcept;

    /*!
     * Constructs a rectangle with the specified \a topLeft and \a bottomRight corners.
     */
    constexpr RectF(const QPointF &topLeft, const QPointF &bottomRight) noexcept;

    /*!
     * Constructs a rectangle with the specified \a topLeft position and \a size.
     */
    constexpr RectF(const QPointF &topLeft, const QSizeF &size) noexcept;

    /*!
     * Constructs a RectF from the specified Rect \a rect.
     */
    constexpr RectF(const Rect &rect) noexcept;

    /*!
     * Constructs a RectF from the specified QRect \a rect.
     */
    constexpr RectF(const QRect &rect) noexcept;

    /*!
     * Constructs a RectF from the specified QRectF \a rect.
     */
    constexpr RectF(const QRectF &rect) noexcept;

    /*!
     * Constructs an uninitialized RectF.
     */
    constexpr explicit RectF(Qt::Initialization) noexcept;

    /*!
     * Returns \c true if the rectangle is empty; otherwise returns \c false. An empty rectangle has
     * left() >= right() or top() >= bottom() or both.
     *
     * \sa isNull(), isValid()
     */
    constexpr inline bool isEmpty() const noexcept;

    /*!
     * Returns \c true if the rectangle is empty; otherwise returns \c false. An empty rectangle has
     * left() >= right() or top() >= bottom() or both.
     *
     * This function is provided for compatibility reasons with QRectF. It is identical to isEmpty().
     * Prefer using isEmpty().
     *
     * \sa isEmpty(), isValid()
     */
    constexpr inline bool isNull() const noexcept;

    /*!
     * Returns \c true if the rectangle is empty; otherwise returns \c false. An empty rectangle has
     * left() >= right() or top() >= bottom() or both.
     *
     * This function is provided for compatibility reasons with QRectF. It is identical to isEmpty().
     * Prefer using isEmpty().
     *
     * \sa isEmpty(), isNull()
     */
    constexpr inline bool isValid() const noexcept;

    /*!
     * Returns \c true if the specified \a rect is inside this rectangle; otherwise returns \c false.
     * If the rectangle is empty, this function returns \c false.
     */
    constexpr inline bool contains(const RectF &rect) const noexcept;

    /*!
     * Returns \c true if the specified \a point is inside this rectangle; otherwise returns \c false.
     * If the rectangle is empty, this function returns \c false.
     *
     * The left and top edges are considered to be inside the rectangle, while the right and bottom
     * edges are not. In other words, this checks \c{left() <= point.x() < right()}, and
     * \c{top() <= point.y() < bottom()}.
     */
    constexpr inline bool contains(const QPointF &point) const noexcept;

    /*!
     * \overload
     */
    constexpr inline bool contains(qreal x, qreal y) const noexcept;

    /*!
     * Returns the left edge coordinate. Equivalent to left().
     *
     * \sa left(), setX()
     */
    constexpr inline qreal x() const noexcept;

    /*!
     * Sets the left edge coordinate to \a x. This may change the width of the rectangle. Equivalent
     * to setLeft().
     *
     * \sa x(), setLeft()
     */
    constexpr inline void setX(qreal x) noexcept;

    /*!
     * Returns the top edge coordinate. Equivalent to top().
     *
     * \sa top(), setY()
     */
    constexpr inline qreal y() const noexcept;

    /*!
     * Sets the top edge coordinate to \a y. This may change the height of the rectangle. Equivalent
     * to setTop().
     *
     * \sa y(), setTop()
     */
    constexpr inline void setY(qreal y) noexcept;

    /*!
     * Returns the width of the rectangle.
     *
     * \sa setWidth()
     */
    constexpr inline qreal width() const noexcept;

    /*!
     * Sets the width of the rectangle to \a width.
     *
     * \sa width()
     */
    constexpr inline void setWidth(qreal width) noexcept;

    /*!
     * Returns the height of the rectangle.
     *
     * \sa setHeight().
     */
    constexpr inline qreal height() const noexcept;

    /*!
     * Sets the height of the rectangle to \a height.
     *
     * \sa height()
     */
    constexpr inline void setHeight(qreal height) noexcept;

    /*!
     * Returns the size of the rectangle.
     *
     * \sa setSize()
     */
    constexpr inline QSizeF size() const noexcept;

    /*!
     * Sets the size of the rectangle to \a size.
     *
     * \sa size()
     */
    constexpr inline void setSize(const QSizeF &size) noexcept;

    /*!
     * Returns the left edge coordinate. Equivalent to x().
     *
     * \sa x(), setLeft(), moveLeft()
     */
    constexpr inline qreal left() const noexcept;

    /*!
     * Sets the left edge coordinate to \a left. This may change the width of the rectangle.
     *
     * \sa left(), moveLeft(), setX()
     */
    constexpr inline void setLeft(qreal left) noexcept;

    /*!
     * Returns the top edge coordinate. Equivalent to y().
     *
     * \sa y(), setTop(), moveTop()
     */
    constexpr inline qreal top() const noexcept;

    /*!
     * Sets the top edge coordinate to \a top. This may change the height of the rectangle.
     *
     * \sa top(), moveTop(), setY()
     */
    constexpr inline void setTop(qreal top) noexcept;

    /*!
     * Returns the right edge coordinate. This equivalent to \c{x() + width()}.
     *
     * \sa setRight(), moveRight()
     */
    constexpr inline qreal right() const noexcept;

    /*!
     * Sets the right edge coordinate to \a right. This may change the width of the rectangle.
     *
     * \sa right(), moveRight()
     */
    constexpr inline void setRight(qreal right) noexcept;

    /*!
     * Returns the bottom edge coordinate. This equivalent to \c{y() + height()}.
     *
     * \sa setBottom(), moveBottom()
     */
    constexpr inline qreal bottom() const noexcept;

    /*!
     * Sets the bottom edge coordinate to \a bottom. This may change the height of the rectangle.
     *
     * \sa bottom(), moveBottom()
     */
    constexpr inline void setBottom(qreal bottom) noexcept;

    /*!
     * Returns the center point of the rectangle.
     *
     * \sa moveCenter()
     */
    constexpr inline QPointF center() const noexcept;

    /*!
     * Returns the top-left corner of the rectangle.
     *
     * \sa setTopLeft(), moveTopLeft()
     */
    constexpr inline QPointF topLeft() const noexcept;

    /*!
     * Returns the bottom-right corner of the rectangle.
     *
     * \sa setBottomRight(), moveBottomRight()
     */
    constexpr inline QPointF bottomRight() const noexcept;

    /*!
     * Returns the top-right corner of the rectangle.
     *
     * \sa setTopRight(), moveTopRight()
     */
    constexpr inline QPointF topRight() const noexcept;

    /*!
     * Returns the bottom-left corner of the rectangle.
     *
     * \sa setBottomLeft(), moveBottomLeft()
     */
    constexpr inline QPointF bottomLeft() const noexcept;

    /*!
     * Sets the top-left corner coordinates to \a point. This may change the width and the height
     * of the rectangle.
     *
     * \sa topLeft(), moveTopLeft()
     */
    constexpr inline void setTopLeft(const QPointF &point) noexcept;

    /*!
     * Sets the bottom-right corner coordinates to \a point. This may change the width and the height
     * of the rectangle.
     *
     * \sa bottomRight(), moveBottomRight()
     */
    constexpr inline void setBottomRight(const QPointF &point) noexcept;

    /*!
     * Sets the top-right corner coordinates to \a point. This may change the width and the height
     * of the rectangle.
     *
     * \sa topRight(), moveTopRight()
     */
    constexpr inline void setTopRight(const QPointF &point) noexcept;

    /*!
     * Sets the bottom-left corner coordinates to \a point. This may change the width and the height
     * of the rectangle.
     *
     * \sa bottomLeft(), moveBottomLeft()
     */
    constexpr inline void setBottomLeft(const QPointF &point) noexcept;

    /*!
     * Moves the rectangle so its center point is at the specified position \a point. This does not
     * change the size of the rectangle.
     *
     * \sa center()
     */
    constexpr inline void moveCenter(const QPointF &point) noexcept;

    /*!
     * Moves the rectangle horizontally so its left edge is left at the specified position \a left.
     * The size of the rectangle won't be changed.
     *
     * \sa left(), setLeft()
     */
    constexpr inline void moveLeft(qreal left) noexcept;

    /*!
     * Moves the rectangle vertically so its top edge is left at the specified position \a top.
     * The size of the rectangle won't be changed.
     *
     * \sa top(), setTop()
     */
    constexpr inline void moveTop(qreal top) noexcept;

    /*!
     * Moves the rectangle horizontally so its right edge is left at the specified position \a right.
     * The size of the rectangle won't be changed.
     *
     * \sa right(), setRight()
     */
    constexpr inline void moveRight(qreal right) noexcept;

    /*!
     * Moves the rectangle vertically so its bottom edge is left at the specified position \a bottom.
     * The size of the rectangle won't be changed.
     *
     * \sa bottom(), setBottom()
     */
    constexpr inline void moveBottom(qreal bottom) noexcept;

    /*!
     * Moves the rectangle so its top-left corner is left at the specified position \a point. The size
     * of the rectangle won't be changed.
     *
     * \sa topLeft(), setTopLeft()
     */
    constexpr inline void moveTopLeft(const QPointF &point) noexcept;

    /*!
     * Moves the rectangle so its bottom-right corner is left at the specified position \a point. The size
     * of the rectangle won't be changed.
     *
     * \sa bottomRight(), setBottomRight()
     */
    constexpr inline void moveBottomRight(const QPointF &point) noexcept;

    /*!
     * Moves the rectangle so its top-right corner is left at the specified position \a point. The size
     * of the rectangle won't be changed.
     *
     * \sa topRight(), setTopRight()
     */
    constexpr inline void moveTopRight(const QPointF &point) noexcept;

    /*!
     * Moves the rectangle so its bottom-left corner is left at the specified position \a point. The size
     * of the rectangle won't be changed.
     *
     * \sa bottomLeft(), setBottomLeft()
     */
    constexpr inline void moveBottomLeft(const QPointF &point) noexcept;

    /*!
     * \overload
     */
    constexpr inline void moveTo(qreal x, qreal y) noexcept;

    /*!
     * Moves the top-left corner of the rectangle to the specified \a point. Equivalent to moveTopLeft().
     *
     * \sa moveTopLeft()
     */
    constexpr inline void moveTo(const QPointF &point) noexcept;

    /*!
     * Returns a copy of this rectangle that has its width and height swapped.
     */
    constexpr inline RectF transposed() const noexcept;

    /*!
     * \overload
     */
    constexpr inline void translate(qreal dx, qreal dy) noexcept;

    /*!
     * Moves the rectangle by adding the given \a offset to the rectangle coordinates. The size
     * will be left unchanged.
     */
    constexpr inline void translate(const QPointF &offset) noexcept;

    /*!
     * \overload
     */
    constexpr inline RectF translated(qreal dx, qreal dy) const noexcept;

    /*!
     * Returns a copy of this rectangle with the given \a offset added to its coordinates. The size
     * will be left unchanged.
     */
    constexpr inline RectF translated(const QPointF &offset) const noexcept;

    /*!
     * Returns a copy of this rectangle with its x and y coordinates scaled by the given \a scale factor.
     */
    constexpr inline RectF scaled(qreal scale) const noexcept;

    /*!
     * Returns a copy of this rectangle with its x coordinates scaled by the given \a xScale factor and
     * the y coordinates scaled by the \a yScale factor.
     */
    constexpr inline RectF scaled(qreal xScale, qreal yScale) const noexcept;

    /*!
     * Sets the rectangle's top-left corner position to (\a x, and \a y), and its size to (\a width, and \a height).
     *
     * This function is provided for compatibility reasons with QRectF.
     *
     * \sa getRect()
     */
    constexpr inline void setRect(qreal x, qreal y, qreal width, qreal height) noexcept;

    /*!
     * Writes the rectangle's top-left corner position to \a x and \a y, and its size to \a width and \a height.
     *
     * This function is provided for compatibility reasons with QRectF.
     *
     * \sa setRect()
     */
    constexpr inline void getRect(qreal *x, qreal *y, qreal *width, qreal *height) const;

    /*!
     * Sets the rectangle's top-left corner position to (\a x1, and \a y1), and its bottom-right corner
     * position to (\a x2, and \a y2).
     *
     * This function is provided for compatibility reasons with QRectF.
     *
     * \sa getCoords()
     */
    constexpr inline void setCoords(qreal x1, qreal y1, qreal x2, qreal y2) noexcept;

    /*!
     * Writes the rectangle's top-left corner position to \a x1 and \a y1, and its bottom-right corner
     * position to \a x2 and \a y2.
     *
     * This function is provided for compatibility reasons with QRectF.
     *
     * \sa setCoords()
     */
    constexpr inline void getCoords(qreal *x1, qreal *y1, qreal *x2, qreal *y2) const;

    /*!
     * Adds \a x1, \a y1, \a x2, and \a y2 to the left, top, right, and bottom edge coordinates,
     * respectively. This may change the size of the rectangle.
     *
     * This function is provided for compatibility reasons with QRectF.
     *
     * Prefer using grownBy() or shrunkBy().
     *
     * \sa adjusted(), grownBy(), shrunkBy()
     */
    constexpr inline void adjust(qreal x1, qreal y1, qreal x2, qreal y2) noexcept;

    /*!
     * Returns a copy of this rectangle with \a x1, \a y1, \a x2, and \a y2 added to its left, top,
     * right, and bottom edge coordinates, respectively. This may change the size of the rectangle.
     *
     * This function is provided for compatibility reasons with QRectF.
     *
     * Prefer using grownBy() or shrunkBy().
     *
     * \sa adjust(), grownBy(), shrunkBy()
     */
    constexpr inline RectF adjusted(qreal x1, qreal y1, qreal x2, qreal y2) const noexcept;

    /*!
     * Returns a copy of this rectangle grown by the given \a margins.
     *
     * \sa shrunkBy().
     */
    constexpr inline RectF grownBy(const QMarginsF &margins) const noexcept;

    /*!
     * Returns a copy of this rectangle shrunk by the given \a margins.
     *
     * \sa grownBy().
     */
    constexpr inline RectF shrunkBy(const QMarginsF &margins) const noexcept;

    /*!
     * Returns a copy of this rectangle grown by the given \a margins. Equivalent to grownBy().
     *
     * This function is provided for compatibility reasons with QRectF.
     *
     * \sa shrunkBy(), marginsRemoved().
     */
    constexpr inline RectF marginsAdded(const QMarginsF &margins) const noexcept;

    /*!
     * Returns a copy of this rectangle shrunk by the given \a margins. Equivalent to shrunkBy().
     *
     * This function is provided for compatibility reasons with QRectF.
     *
     * \sa grownBy(), marginsAdded().
     */
    constexpr inline RectF marginsRemoved(const QMarginsF &margins) const noexcept;

    /*!
     * Returns the bounding rectangle of this rectangle and the given \a other rectangle.
     */
    constexpr inline RectF united(const RectF &other) const noexcept;

    /*!
     * Returns the intersection of this rectangle and the given \a other rectangle. If the rectangles
     * don't intersect, an empty rectangle will be returned.
     *
     * \sa intersects()
     */
    constexpr inline RectF intersected(const RectF &other) const noexcept;

    /*!
     * Returns \c true if this rectangle and the given \a other rectangle intersect; otherwise returns \c false.
     *
     * \sa intersected()
     */
    constexpr inline bool intersects(const RectF &other) const noexcept;

    /*!
     * Returns a copy of this rectangle with its left, top, right, and bottom edge coordinates rounded
     * to the nearest integers.
     *
     * \sa roundedOut()
     */
    constexpr inline Rect rounded() const noexcept;

    /*!
     * Returns a copy of this rectangle whose left and top edge coordinates have been floored, and
     * right and bottom edge coordinates have been ceiled.
     *
     * \sa rounded()
     */
    constexpr inline Rect roundedOut() const noexcept;

    /*!
     * Equivalent to rounded(). This function is provided for compatibility reasons with QRectF.
     *
     * \note This function does not return the same values as QRectF::toRect().
     * \sa rounded()
     */
    constexpr inline Rect toRect() const noexcept;

    /*!
     * Equivalent to roundedOut(). This function is provided for compatibility reasons with QRectF.
     *
     * \sa roundedOut()
     */
    constexpr inline Rect toAlignedRect() const noexcept;

    /*!
     * Returns \c true if this rectangle and the \a other rectangle are equal; otherwise returns \c false.
     */
    constexpr inline bool operator==(const RectF &other) const noexcept;

    /*!
     * Returns the bounding rectangle of this rectangle and the given \a other rectangle. Equivalent
     * to united().
     *
     * \sa united()
     */
    constexpr inline RectF operator|(const RectF &other) const noexcept;

    /*!
     * Returns the intersection of this rectangle and the given \a other rectangle. Equivalent to
     * intersected().
     *
     * \sa intersected()
     */
    constexpr inline RectF operator&(const RectF &other) const noexcept;

    /*!
     * Unites this rectangle with the given \a other rectangle.
     *
     * \sa united(), operator|()
     */
    constexpr inline RectF &operator|=(const RectF &other) noexcept;

    /*!
     * Intersects this rectangle with the given \a other rectangle.
     *
     * \sa intersected(), operator&()
     */
    constexpr inline RectF &operator&=(const RectF &other) noexcept;

    /*!
     * Grows this rectangle by the given \a margins.
     *
     * \sa grownBy().
     */
    constexpr inline RectF &operator+=(const QMarginsF &margins) noexcept;

    /*!
     * Shrinks this rectangle by the given \a margins.
     *
     * \sa shrunkBy().
     */
    constexpr inline RectF &operator-=(const QMarginsF &margins) noexcept;

    /*!
     * Implicitly converts this rectangle to QRectF.
     */
    constexpr inline operator QRectF() const noexcept;

private:
    qreal m_left;
    qreal m_top;
    qreal m_right;
    qreal m_bottom;
};

constexpr inline Rect::Rect() noexcept
    : m_left(0)
    , m_top(0)
    , m_right(0)
    , m_bottom(0)
{
}

constexpr inline Rect::Rect(int x, int y, int width, int height) noexcept
    : m_left(x)
    , m_top(y)
    , m_right(x + width)
    , m_bottom(y + height)
{
}

constexpr inline Rect::Rect(const QPoint &topLeft, const QPoint &bottomRight) noexcept
    : m_left(topLeft.x())
    , m_top(topLeft.y())
    , m_right(bottomRight.x())
    , m_bottom(bottomRight.y())
{
}

constexpr inline Rect::Rect(const QPoint &topLeft, const QSize &size) noexcept
    : m_left(topLeft.x())
    , m_top(topLeft.y())
    , m_right(topLeft.x() + size.width())
    , m_bottom(topLeft.y() + size.height())
{
}

constexpr inline Rect::Rect(const QRect &rect) noexcept
    : m_left(rect.x())
    , m_top(rect.y())
    , m_right(rect.right() + 1)
    , m_bottom(rect.bottom() + 1)
{
}

constexpr inline Rect::Rect(Qt::Initialization) noexcept
{
}

constexpr inline bool Rect::isEmpty() const noexcept
{
    return m_left >= m_right || m_top >= m_bottom;
}

constexpr inline bool Rect::isNull() const noexcept
{
    return isEmpty();
}

constexpr inline bool Rect::isValid() const noexcept
{
    return !isEmpty();
}

constexpr inline bool Rect::contains(const Rect &rect) const noexcept
{
    return !isEmpty() && (left() <= rect.left() && rect.right() <= right()) && (top() <= rect.top() && rect.bottom() <= bottom());
}

constexpr inline bool Rect::contains(const QPoint &point) const noexcept
{
    return (left() <= point.x() && point.x() < right()) && (top() <= point.y() && point.y() < bottom());
}

constexpr inline bool Rect::contains(int x, int y) const noexcept
{
    return contains(QPoint(x, y));
}

constexpr inline int Rect::x() const noexcept
{
    return m_left;
}

constexpr inline void Rect::setX(int x) noexcept
{
    m_left = x;
}

constexpr inline int Rect::y() const noexcept
{
    return m_top;
}

constexpr inline void Rect::setY(int y) noexcept
{
    m_top = y;
}

constexpr inline int Rect::width() const noexcept
{
    return m_right - m_left;
}

constexpr inline void Rect::setWidth(int width) noexcept
{
    m_right = m_left + width;
}

constexpr inline int Rect::height() const noexcept
{
    return m_bottom - m_top;
}

constexpr inline void Rect::setHeight(int height) noexcept
{
    m_bottom = m_top + height;
}

constexpr inline QSize Rect::size() const noexcept
{
    return QSize(width(), height());
}

constexpr inline void Rect::setSize(const QSize &size) noexcept
{
    setWidth(size.width());
    setHeight(size.height());
}

constexpr inline int Rect::left() const noexcept
{
    return m_left;
}

constexpr inline void Rect::setLeft(int left) noexcept
{
    m_left = left;
}

constexpr inline int Rect::top() const noexcept
{
    return m_top;
}

constexpr inline void Rect::setTop(int top) noexcept
{
    m_top = top;
}

constexpr inline int Rect::right() const noexcept
{
    return m_right;
}

constexpr inline void Rect::setRight(int right) noexcept
{
    m_right = right;
}

constexpr inline int Rect::bottom() const noexcept
{
    return m_bottom;
}

constexpr inline void Rect::setBottom(int bottom) noexcept
{
    m_bottom = bottom;
}

constexpr inline QPoint Rect::center() const noexcept
{
    return QPoint(m_left + (m_right - m_left) / 2, m_top + (m_bottom - m_top) / 2);
}

constexpr inline QPoint Rect::topLeft() const noexcept
{
    return QPoint(left(), top());
}

constexpr inline QPoint Rect::bottomRight() const noexcept
{
    return QPoint(right(), bottom());
}

constexpr inline QPoint Rect::topRight() const noexcept
{
    return QPoint(right(), top());
}

constexpr inline QPoint Rect::bottomLeft() const noexcept
{
    return QPoint(left(), bottom());
}

constexpr inline void Rect::setTopLeft(const QPoint &point) noexcept
{
    setLeft(point.x());
    setTop(point.y());
}

constexpr inline void Rect::setBottomRight(const QPoint &point) noexcept
{
    setRight(point.x());
    setBottom(point.y());
}

constexpr inline void Rect::setTopRight(const QPoint &point) noexcept
{
    setRight(point.x());
    setTop(point.y());
}

constexpr inline void Rect::setBottomLeft(const QPoint &point) noexcept
{
    setLeft(point.x());
    setBottom(point.y());
}

constexpr inline void Rect::moveCenter(const QPoint &point) noexcept
{
    moveLeft(point.x() - width() / 2);
    moveTop(point.y() - height() / 2);
}

constexpr inline void Rect::moveLeft(int left) noexcept
{
    const int offset = left - m_left;
    m_left = left;
    m_right += offset;
}

constexpr inline void Rect::moveTop(int top) noexcept
{
    const int offset = top - m_top;
    m_top = top;
    m_bottom += offset;
}

constexpr inline void Rect::moveRight(int right) noexcept
{
    const int offset = right - m_right;
    m_right = right;
    m_left += offset;
}

constexpr inline void Rect::moveBottom(int bottom) noexcept
{
    const int offset = bottom - m_bottom;
    m_bottom = bottom;
    m_top += offset;
}

constexpr inline void Rect::moveTopLeft(const QPoint &point) noexcept
{
    moveLeft(point.x());
    moveTop(point.y());
}

constexpr inline void Rect::moveBottomRight(const QPoint &point) noexcept
{
    moveRight(point.x());
    moveBottom(point.y());
}

constexpr inline void Rect::moveTopRight(const QPoint &point) noexcept
{
    moveRight(point.x());
    moveTop(point.y());
}

constexpr inline void Rect::moveBottomLeft(const QPoint &point) noexcept
{
    moveLeft(point.x());
    moveBottom(point.y());
}

constexpr inline void Rect::moveTo(int x, int y) noexcept
{
    moveTo(QPoint(x, y));
}

constexpr inline void Rect::moveTo(const QPoint &point) noexcept
{
    moveTopLeft(point);
}

constexpr inline Rect Rect::transposed() const noexcept
{
    return Rect(topLeft(), size().transposed());
}

constexpr inline void Rect::translate(int dx, int dy) noexcept
{
    translate(QPoint(dx, dy));
}

constexpr inline void Rect::translate(const QPoint &offset) noexcept
{
    m_left += offset.x();
    m_right += offset.x();
    m_top += offset.y();
    m_bottom += offset.y();
}

constexpr inline Rect Rect::translated(int dx, int dy) const noexcept
{
    return translated(QPoint(dx, dy));
}

constexpr inline Rect Rect::translated(const QPoint &offset) const noexcept
{
    return Rect(topLeft() + offset, bottomRight() + offset);
}

constexpr inline RectF Rect::scaled(qreal scale) const noexcept
{
    return RectF(topLeft(), bottomRight()).scaled(scale);
}

constexpr inline RectF Rect::scaled(qreal xScale, qreal yScale) const noexcept
{
    return RectF(topLeft(), bottomRight()).scaled(xScale, yScale);
}

constexpr inline void Rect::setRect(int x, int y, int width, int height) noexcept
{
    m_left = x;
    m_top = y;
    m_right = x + width;
    m_bottom = y + height;
}

constexpr inline void Rect::getRect(int *x, int *y, int *width, int *height) const
{
    *x = m_left;
    *y = m_top;
    *width = m_right - m_left;
    *height = m_bottom - m_top;
}

constexpr inline void Rect::setCoords(int x1, int y1, int x2, int y2) noexcept
{
    m_left = std::min(x1, x2);
    m_right = std::max(x1, x2);
    m_top = std::min(y1, y2);
    m_bottom = std::max(y1, y2);
}

constexpr inline void Rect::getCoords(int *x1, int *y1, int *x2, int *y2) const
{
    *x1 = m_left;
    *y1 = m_top;
    *x2 = m_right;
    *y2 = m_bottom;
}

constexpr inline void Rect::adjust(int x1, int y1, int x2, int y2) noexcept
{
    m_left += x1;
    m_top += y1;
    m_right += x2;
    m_bottom += y2;
}

constexpr inline Rect Rect::adjusted(int x1, int y1, int x2, int y2) const noexcept
{
    return Rect(QPoint(left() + x1, top() + y1), QPoint(right() + x2, bottom() + y2));
}

constexpr inline Rect Rect::grownBy(const QMargins &margins) const noexcept
{
    return Rect(QPoint(left() - margins.left(), top() - margins.top()), QPoint(right() + margins.right(), bottom() + margins.bottom()));
}

constexpr inline Rect Rect::shrunkBy(const QMargins &margins) const noexcept
{
    return Rect(QPoint(left() + margins.left(), top() + margins.top()), QPoint(right() - margins.right(), bottom() - margins.bottom()));
}

constexpr inline Rect Rect::marginsAdded(const QMargins &margins) const noexcept
{
    return grownBy(margins);
}

constexpr inline Rect Rect::marginsRemoved(const QMargins &margins) const noexcept
{
    return shrunkBy(margins);
}

constexpr inline Rect Rect::united(const Rect &other) const noexcept
{
    if (isEmpty()) {
        return other;
    }
    if (other.isEmpty()) {
        return *this;
    }
    return Rect(QPoint(std::min(left(), other.left()), std::min(top(), other.top())),
                QPoint(std::max(right(), other.right()), std::max(bottom(), other.bottom())));
}

constexpr inline Rect Rect::intersected(const Rect &other) const noexcept
{
    if (isEmpty()) {
        return Rect();
    }
    if (other.isEmpty()) {
        return Rect();
    }

    const int intersectionLeft = std::max(left(), other.left());
    const int intersectionRight = std::min(right(), other.right());
    const int intersectionTop = std::max(top(), other.top());
    const int intersectionBottom = std::min(bottom(), other.bottom());

    if (intersectionLeft >= intersectionRight) {
        return Rect();
    }
    if (intersectionTop >= intersectionBottom) {
        return Rect();
    }

    return Rect(QPoint(intersectionLeft, intersectionTop), QPoint(intersectionRight, intersectionBottom));
}

constexpr inline bool Rect::intersects(const Rect &other) const noexcept
{
    return left() < other.right() && other.left() < right()
        && top() < other.bottom() && other.top() < bottom();
}

constexpr inline bool Rect::operator==(const Rect &other) const noexcept
{
    return m_left == other.m_left && m_top == other.m_top && m_right == other.m_right && m_bottom == other.m_bottom;
}

constexpr inline Rect Rect::operator|(const Rect &other) const noexcept
{
    return united(other);
}

constexpr inline Rect Rect::operator&(const Rect &other) const noexcept
{
    return intersected(other);
}

constexpr inline Rect &Rect::operator|=(const Rect &other) noexcept
{
    *this = united(other);
    return *this;
}

constexpr inline Rect &Rect::operator&=(const Rect &other) noexcept
{
    *this = intersected(other);
    return *this;
}

constexpr inline Rect &Rect::operator+=(const QMargins &margins) noexcept
{
    *this = grownBy(margins);
    return *this;
}

constexpr inline Rect &Rect::operator-=(const QMargins &margins) noexcept
{
    *this = shrunkBy(margins);
    return *this;
}

constexpr inline Rect::operator QRect() const noexcept
{
    return QRect(topLeft(), bottomRight() - QPoint(1, 1));
}

constexpr inline RectF::RectF() noexcept
    : m_left(0)
    , m_top(0)
    , m_right(0)
    , m_bottom(0)
{
}

constexpr inline RectF::RectF(qreal x, qreal y, qreal width, qreal height) noexcept
    : m_left(x)
    , m_top(y)
    , m_right(x + width)
    , m_bottom(y + height)
{
}

constexpr inline RectF::RectF(const QPointF &topLeft, const QPointF &bottomRight) noexcept
    : m_left(topLeft.x())
    , m_top(topLeft.y())
    , m_right(bottomRight.x())
    , m_bottom(bottomRight.y())
{
}

constexpr inline RectF::RectF(const QPointF &topLeft, const QSizeF &size) noexcept
    : m_left(topLeft.x())
    , m_top(topLeft.y())
    , m_right(topLeft.x() + size.width())
    , m_bottom(topLeft.y() + size.height())
{
}

constexpr inline RectF::RectF(const Rect &rect) noexcept
    : m_left(rect.left())
    , m_top(rect.top())
    , m_right(rect.right())
    , m_bottom(rect.bottom())
{
}

constexpr inline RectF::RectF(const QRect &rect) noexcept
    : m_left(rect.left())
    , m_top(rect.top())
    , m_right(rect.right() + 1)
    , m_bottom(rect.bottom() + 1)
{
}

constexpr inline RectF::RectF(const QRectF &rect) noexcept
    : m_left(rect.left())
    , m_top(rect.top())
    , m_right(rect.right())
    , m_bottom(rect.bottom())
{
}

constexpr inline RectF::RectF(Qt::Initialization) noexcept
{
}

constexpr inline bool RectF::isEmpty() const noexcept
{
    return m_left >= m_right || m_top >= m_bottom;
}

constexpr inline bool RectF::isNull() const noexcept
{
    return isEmpty();
}

constexpr inline bool RectF::isValid() const noexcept
{
    return !isEmpty();
}

constexpr inline bool RectF::contains(const RectF &rect) const noexcept
{
    return !isEmpty() && (left() <= rect.left() && rect.right() <= right()) && (top() <= rect.top() && rect.bottom() <= bottom());
}

constexpr inline bool RectF::contains(const QPointF &point) const noexcept
{
    return (left() <= point.x() && point.x() < right()) && (top() <= point.y() && point.y() < bottom());
}

constexpr inline bool RectF::contains(qreal x, qreal y) const noexcept
{
    return contains(QPointF(x, y));
}

constexpr inline qreal RectF::x() const noexcept
{
    return m_left;
}

constexpr inline void RectF::setX(qreal x) noexcept
{
    m_left = x;
}

constexpr inline qreal RectF::y() const noexcept
{
    return m_top;
}

constexpr inline void RectF::setY(qreal y) noexcept
{
    m_top = y;
}

constexpr inline qreal RectF::width() const noexcept
{
    return m_right - m_left;
}

constexpr inline void RectF::setWidth(qreal width) noexcept
{
    m_right = m_left + width;
}

constexpr inline qreal RectF::height() const noexcept
{
    return m_bottom - m_top;
}

constexpr inline void RectF::setHeight(qreal height) noexcept
{
    m_bottom = m_top + height;
}

constexpr inline QSizeF RectF::size() const noexcept
{
    return QSizeF(width(), height());
}

constexpr inline void RectF::setSize(const QSizeF &size) noexcept
{
    setWidth(size.width());
    setHeight(size.height());
}

constexpr inline qreal RectF::left() const noexcept
{
    return m_left;
}

constexpr inline void RectF::setLeft(qreal left) noexcept
{
    m_left = left;
}

constexpr inline qreal RectF::top() const noexcept
{
    return m_top;
}

constexpr inline void RectF::setTop(qreal top) noexcept
{
    m_top = top;
}

constexpr inline qreal RectF::right() const noexcept
{
    return m_right;
}

constexpr inline void RectF::setRight(qreal right) noexcept
{
    m_right = right;
}

constexpr inline qreal RectF::bottom() const noexcept
{
    return m_bottom;
}

constexpr inline void RectF::setBottom(qreal bottom) noexcept
{
    m_bottom = bottom;
}

constexpr inline QPointF RectF::center() const noexcept
{
    return QPointF(m_left + (m_right - m_left) / 2, m_top + (m_bottom - m_top) / 2);
}

constexpr inline QPointF RectF::topLeft() const noexcept
{
    return QPointF(left(), top());
}

constexpr inline QPointF RectF::bottomRight() const noexcept
{
    return QPointF(right(), bottom());
}

constexpr inline QPointF RectF::topRight() const noexcept
{
    return QPointF(right(), top());
}

constexpr inline QPointF RectF::bottomLeft() const noexcept
{
    return QPointF(left(), bottom());
}

constexpr inline void RectF::setTopLeft(const QPointF &point) noexcept
{
    setLeft(point.x());
    setTop(point.y());
}

constexpr inline void RectF::setBottomRight(const QPointF &point) noexcept
{
    setRight(point.x());
    setBottom(point.y());
}

constexpr inline void RectF::setTopRight(const QPointF &point) noexcept
{
    setRight(point.x());
    setTop(point.y());
}

constexpr inline void RectF::setBottomLeft(const QPointF &point) noexcept
{
    setLeft(point.x());
    setBottom(point.y());
}

constexpr inline void RectF::moveCenter(const QPointF &point) noexcept
{
    moveLeft(point.x() - width() * 0.5);
    moveTop(point.y() - height() * 0.5);
}

constexpr inline void RectF::moveLeft(qreal left) noexcept
{
    const qreal offset = left - m_left;
    m_left = left;
    m_right += offset;
}

constexpr inline void RectF::moveTop(qreal top) noexcept
{
    const qreal offset = top - m_top;
    m_top = top;
    m_bottom += offset;
}

constexpr inline void RectF::moveRight(qreal right) noexcept
{
    const qreal offset = right - m_right;
    m_right = right;
    m_left += offset;
}

constexpr inline void RectF::moveBottom(qreal bottom) noexcept
{
    const qreal offset = bottom - m_bottom;
    m_bottom = bottom;
    m_top += offset;
}

constexpr inline void RectF::moveTopLeft(const QPointF &point) noexcept
{
    moveLeft(point.x());
    moveTop(point.y());
}

constexpr inline void RectF::moveBottomRight(const QPointF &point) noexcept
{
    moveRight(point.x());
    moveBottom(point.y());
}

constexpr inline void RectF::moveTopRight(const QPointF &point) noexcept
{
    moveRight(point.x());
    moveTop(point.y());
}

constexpr inline void RectF::moveBottomLeft(const QPointF &point) noexcept
{
    moveLeft(point.x());
    moveBottom(point.y());
}

constexpr inline void RectF::moveTo(qreal x, qreal y) noexcept
{
    moveTo(QPointF(x, y));
}

constexpr inline void RectF::moveTo(const QPointF &point) noexcept
{
    moveTopLeft(point);
}

constexpr inline RectF RectF::transposed() const noexcept
{
    return RectF(topLeft(), size().transposed());
}

constexpr inline void RectF::translate(qreal dx, qreal dy) noexcept
{
    translate(QPointF(dx, dy));
}

constexpr inline void RectF::translate(const QPointF &offset) noexcept
{
    m_left += offset.x();
    m_right += offset.x();
    m_top += offset.y();
    m_bottom += offset.y();
}

constexpr inline RectF RectF::translated(qreal dx, qreal dy) const noexcept
{
    return translated(QPointF(dx, dy));
}

constexpr inline RectF RectF::translated(const QPointF &offset) const noexcept
{
    return RectF(topLeft() + offset, bottomRight() + offset);
}

constexpr inline RectF RectF::scaled(qreal scale) const noexcept
{
    return RectF(topLeft() * scale, bottomRight() * scale);
}

constexpr inline RectF RectF::scaled(qreal xScale, qreal yScale) const noexcept
{
    return RectF(QPointF(left() * xScale, top() * yScale), QPointF(right() * xScale, bottom() * yScale));
}

constexpr inline void RectF::setRect(qreal x, qreal y, qreal width, qreal height) noexcept
{
    m_left = x;
    m_top = y;
    m_right = x + width;
    m_bottom = y + height;
}

constexpr inline void RectF::getRect(qreal *x, qreal *y, qreal *width, qreal *height) const
{
    *x = m_left;
    *y = m_top;
    *width = m_right - m_left;
    *height = m_bottom - m_top;
}

constexpr inline void RectF::setCoords(qreal x1, qreal y1, qreal x2, qreal y2) noexcept
{
    m_left = std::min(x1, x2);
    m_right = std::max(x1, x2);
    m_top = std::min(y1, y2);
    m_bottom = std::max(y1, y2);
}

constexpr inline void RectF::getCoords(qreal *x1, qreal *y1, qreal *x2, qreal *y2) const
{
    *x1 = m_left;
    *y1 = m_top;
    *x2 = m_right;
    *y2 = m_bottom;
}

constexpr inline void RectF::adjust(qreal x1, qreal y1, qreal x2, qreal y2) noexcept
{
    m_left += x1;
    m_top += y1;
    m_right += x2;
    m_bottom += y2;
}

constexpr inline RectF RectF::adjusted(qreal x1, qreal y1, qreal x2, qreal y2) const noexcept
{
    return RectF(QPointF(left() + x1, top() + y1), QPointF(right() + x2, bottom() + y2));
}

constexpr inline RectF RectF::grownBy(const QMarginsF &margins) const noexcept
{
    return RectF(QPointF(left() - margins.left(), top() - margins.top()), QPointF(right() + margins.right(), bottom() + margins.bottom()));
}

constexpr inline RectF RectF::shrunkBy(const QMarginsF &margins) const noexcept
{
    return RectF(QPointF(left() + margins.left(), top() + margins.top()), QPointF(right() - margins.right(), bottom() - margins.bottom()));
}

constexpr inline RectF RectF::marginsAdded(const QMarginsF &margins) const noexcept
{
    return grownBy(margins);
}

constexpr inline RectF RectF::marginsRemoved(const QMarginsF &margins) const noexcept
{
    return shrunkBy(margins);
}

constexpr inline RectF RectF::united(const RectF &other) const noexcept
{
    if (isEmpty()) {
        return other;
    }
    if (other.isEmpty()) {
        return *this;
    }

    return RectF(QPointF(std::min(left(), other.left()), std::min(top(), other.top())),
                 QPointF(std::max(right(), other.right()), std::max(bottom(), other.bottom())));
}

constexpr inline RectF RectF::intersected(const RectF &other) const noexcept
{
    if (isEmpty()) {
        return Rect();
    }
    if (other.isEmpty()) {
        return Rect();
    }

    const qreal intersectionLeft = std::max(left(), other.left());
    const qreal intersectionRight = std::min(right(), other.right());
    const qreal intersectionTop = std::max(top(), other.top());
    const qreal intersectionBottom = std::min(bottom(), other.bottom());

    if (intersectionLeft >= intersectionRight) {
        return RectF();
    }
    if (intersectionTop >= intersectionBottom) {
        return RectF();
    }

    return RectF(QPointF(intersectionLeft, intersectionTop), QPointF(intersectionRight, intersectionBottom));
}

constexpr inline bool RectF::intersects(const RectF &other) const noexcept
{
    return left() < other.right() && other.left() < right()
        && top() < other.bottom() && other.top() < bottom();
}

constexpr inline Rect RectF::rounded() const noexcept
{
    return Rect(QPoint(std::round(left()), std::round(top())),
                QPoint(std::round(right()), std::round(bottom())));
}

constexpr inline Rect RectF::roundedOut() const noexcept
{
    return Rect(QPoint(std::floor(left()), std::floor(top())),
                QPoint(std::ceil(right()), std::ceil(bottom())));
}

constexpr inline Rect RectF::toRect() const noexcept
{
    return rounded();
}

constexpr inline Rect RectF::toAlignedRect() const noexcept
{
    return roundedOut();
}

constexpr inline bool RectF::operator==(const RectF &other) const noexcept
{
    return m_left == other.m_left && m_top == other.m_top && m_right == other.m_right && m_bottom == other.m_bottom;
}

constexpr inline RectF RectF::operator|(const RectF &other) const noexcept
{
    return united(other);
}

constexpr inline RectF RectF::operator&(const RectF &other) const noexcept
{
    return intersected(other);
}

constexpr inline RectF &RectF::operator|=(const RectF &other) noexcept
{
    *this = united(other);
    return *this;
}

constexpr inline RectF &RectF::operator&=(const RectF &other) noexcept
{
    *this = intersected(other);
    return *this;
}

constexpr inline RectF &RectF::operator+=(const QMarginsF &margins) noexcept
{
    *this = grownBy(margins);
    return *this;
}

constexpr inline RectF &RectF::operator-=(const QMarginsF &margins) noexcept
{
    *this = shrunkBy(margins);
    return *this;
}

constexpr inline RectF::operator QRectF() const noexcept
{
    return QRectF(topLeft(), bottomRight());
}

constexpr inline bool operator==(const Rect &rect, const QRect &qrect) noexcept
{
    return rect == Rect(qrect);
}

constexpr inline bool operator==(const RectF &rect, const QRectF &qrect) noexcept
{
    return rect == RectF(qrect);
}

} // namespace KWin

Q_DECLARE_TYPEINFO(KWin::Rect, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(KWin::RectF, Q_PRIMITIVE_TYPE);

KWIN_EXPORT QDebug operator<<(QDebug dbg, const KWin::Rect &rect);
KWIN_EXPORT QDebug operator<<(QDebug dbg, const KWin::RectF &rect);

KWIN_EXPORT QDataStream &operator<<(QDataStream &stream, const KWin::Rect &rect);
KWIN_EXPORT QDataStream &operator>>(QDataStream &stream, KWin::Rect &rect);

KWIN_EXPORT QDataStream &operator<<(QDataStream &stream, const KWin::RectF &rect);
KWIN_EXPORT QDataStream &operator>>(QDataStream &stream, KWin::RectF &rect);
