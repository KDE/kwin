/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "core/rect.h"

using namespace KWin;

class TestRectF : public QObject
{
    Q_OBJECT

public:
    TestRectF() = default;

private Q_SLOTS:
    void equals_data();
    void equals();
    void notEquals_data();
    void notEquals();
    void empty_data();
    void empty();
    void containsPoint_data();
    void containsPoint();
    void containsRect_data();
    void containsRect();
    void x_data();
    void x();
    void setX_data();
    void setX();
    void y_data();
    void y();
    void setY_data();
    void setY();
    void width_data();
    void width();
    void setWidth_data();
    void setWidth();
    void height_data();
    void height();
    void setHeight_data();
    void setHeight();
    void left_data();
    void left();
    void setLeft_data();
    void setLeft();
    void moveLeft_data();
    void moveLeft();
    void top_data();
    void top();
    void setTop_data();
    void setTop();
    void moveTop_data();
    void moveTop();
    void right_data();
    void right();
    void setRight_data();
    void setRight();
    void moveRight_data();
    void moveRight();
    void bottom_data();
    void bottom();
    void setBottom_data();
    void setBottom();
    void moveBottom_data();
    void moveBottom();
    void center_data();
    void center();
    void moveCenter_data();
    void moveCenter();
    void topLeft_data();
    void topLeft();
    void setTopLeft_data();
    void setTopLeft();
    void moveTopLeft_data();
    void moveTopLeft();
    void topRight_data();
    void topRight();
    void setTopRight_data();
    void setTopRight();
    void moveTopRight_data();
    void moveTopRight();
    void bottomRight_data();
    void bottomRight();
    void setBottomRight_data();
    void setBottomRight();
    void moveBottomRight_data();
    void moveBottomRight();
    void bottomLeft_data();
    void bottomLeft();
    void setBottomLeft_data();
    void setBottomLeft();
    void moveBottomLeft_data();
    void moveBottomLeft();
    void transposed_data();
    void transposed();
    void translate_data();
    void translate();
    void translated_data();
    void translated();
    void scaled_data();
    void scaled();
    void adjust_data();
    void adjust();
    void adjusted_data();
    void adjusted();
    void grownBy_data();
    void grownBy();
    void shrunkBy_data();
    void shrunkBy();
    void united_data();
    void united();
    void intersects_data();
    void intersects();
    void intersected_data();
    void intersected();
    void rounded_data();
    void rounded();
    void roundedIn_data();
    void roundedIn();
    void roundedOut_data();
    void roundedOut();
    void implicitConversions();
};

void TestRectF::equals_data()
{
    QTest::addColumn<RectF>("rect1");
    QTest::addColumn<RectF>("rect2");
    QTest::addColumn<bool>("expected");

    QTest::addRow("default == default") << RectF() << RectF() << true;
    QTest::addRow("2,3 4x5 == default") << RectF(2, 3, 4, 5) << RectF() << false;
    QTest::addRow("default == 2,3 4x5") << RectF() << RectF(2, 3, 4, 5) << false;
    QTest::addRow("2,3 4x5 == 2,3 4x5") << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 5) << true;

    QTest::addRow("2,3 4x5 == 0,3 4x5") << RectF(2, 3, 4, 5) << RectF(0, 3, 4, 5) << false;
    QTest::addRow("2,3 4x5 == 2,0 4x5") << RectF(2, 3, 4, 5) << RectF(2, 0, 4, 5) << false;
    QTest::addRow("2,3 4x5 == 2,3 0x5") << RectF(2, 3, 4, 5) << RectF(2, 3, 0, 5) << false;
    QTest::addRow("2,3 4x5 == 2,3 4x0") << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 0) << false;
}

void TestRectF::equals()
{
    QFETCH(RectF, rect1);
    QFETCH(RectF, rect2);

    QTEST(rect1 == rect2, "expected");
}

void TestRectF::notEquals_data()
{
    QTest::addColumn<RectF>("rect1");
    QTest::addColumn<RectF>("rect2");
    QTest::addColumn<bool>("expected");

    QTest::addRow("default == default") << RectF() << RectF() << false;
    QTest::addRow("2,3 4x5 == default") << RectF(2, 3, 4, 5) << RectF() << true;
    QTest::addRow("default == 2,3 4x5") << RectF() << RectF(2, 3, 4, 5) << true;
    QTest::addRow("2,3 4x5 == 2,3 4x5") << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 5) << false;

    QTest::addRow("2,3 4x5 == 0,3 4x5") << RectF(2, 3, 4, 5) << RectF(0, 3, 4, 5) << true;
    QTest::addRow("2,3 4x5 == 2,0 4x5") << RectF(2, 3, 4, 5) << RectF(2, 0, 4, 5) << true;
    QTest::addRow("2,3 4x5 == 2,3 0x5") << RectF(2, 3, 4, 5) << RectF(2, 3, 0, 5) << true;
    QTest::addRow("2,3 4x5 == 2,3 4x0") << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 0) << true;
}

void TestRectF::notEquals()
{
    QFETCH(RectF, rect1);
    QFETCH(RectF, rect2);

    QTEST(rect1 != rect2, "expected");
}

void TestRectF::empty_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<bool>("empty");

    QTest::addRow("default") << RectF() << true;

    QTest::addRow("0,0 0x0") << RectF(0, 0, 0, 0) << true;
    QTest::addRow("2,3 0x0") << RectF(2, 3, 0, 0) << true;
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << false;

    QTest::addRow("QPointF(1, 1) QPointF(1, 1)") << RectF(QPointF(1, 1), QPointF(1, 1)) << true;
    QTest::addRow("QPointF(1, 1) QPointF(1, 2)") << RectF(QPointF(1, 1), QPointF(1, 2)) << true;
    QTest::addRow("QPointF(1, 1) QPointF(2, 1)") << RectF(QPointF(1, 1), QPointF(2, 1)) << true;
    QTest::addRow("QPointF(1, 1) QPointF(2, 2)") << RectF(QPointF(1, 1), QPointF(2, 2)) << false;

    QTest::addRow("QPointF(1, 1) QSizeF(0, 0)") << RectF(QPointF(1, 1), QSizeF(0, 0)) << true;
    QTest::addRow("QPointF(1, 1) QSizeF(0, 1)") << RectF(QPointF(1, 1), QSizeF(0, 1)) << true;
    QTest::addRow("QPointF(1, 1) QSizeF(1, 0)") << RectF(QPointF(1, 1), QSizeF(1, 0)) << true;
    QTest::addRow("QPointF(1, 1) QSizeF(1, 1)") << RectF(QPointF(1, 1), QSizeF(1, 1)) << false;

    QTest::addRow("QRect()") << RectF(QRect()) << true;
    QTest::addRow("QRect(1, 1, 0, 0)") << RectF(QRect(1, 1, 0, 0)) << true;
    QTest::addRow("QRect(1, 1, 0, 1)") << RectF(QRect(1, 1, 0, 1)) << true;
    QTest::addRow("QRect(1, 1, 1, 0)") << RectF(QRect(1, 1, 1, 0)) << true;
    QTest::addRow("QRect(1, 1, 1, 1)") << RectF(QRect(1, 1, 1, 1)) << false;
}

void TestRectF::empty()
{
    QFETCH(RectF, rect);

    QTEST(rect.isEmpty(), "empty");
}

void TestRectF::containsPoint_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("point");
    QTest::addColumn<bool>("contains");

    QTest::addRow("empty - 0,0") << RectF() << QPointF(0, 0) << false;
    QTest::addRow("empty - 1,1") << RectF() << QPointF(1, 1) << false;

    QTest::addRow("2,3 4x5 - 1,2") << RectF(2, 3, 4, 5) << QPointF(1, 2) << false;
    QTest::addRow("2,3 4x5 - 7,9") << RectF(2, 3, 4, 5) << QPointF(7, 9) << false;
    QTest::addRow("2,3 4x5 - 1,9") << RectF(2, 3, 4, 5) << QPointF(1, 9) << false;
    QTest::addRow("2,3 4x5 - 7,2") << RectF(2, 3, 4, 5) << QPointF(7, 2) << false;

    QTest::addRow("2,3 4x5 - 2,3") << RectF(2, 3, 4, 5) << QPointF(2, 3) << true;
    QTest::addRow("2,3 4x5 - 6,3") << RectF(2, 3, 4, 5) << QPointF(6, 3) << false;
    QTest::addRow("2,3 4x5 - 5,3") << RectF(2, 3, 4, 5) << QPointF(5, 3) << true;
    QTest::addRow("2,3 4x5 - 6,8") << RectF(2, 3, 4, 5) << QPointF(6, 8) << false;
    QTest::addRow("2,3 4x5 - 5,7") << RectF(2, 3, 4, 5) << QPointF(5, 7) << true;
    QTest::addRow("2,3 4x5 - 2,8") << RectF(2, 3, 4, 5) << QPointF(2, 8) << false;
    QTest::addRow("2,3 4x5 - 2,7") << RectF(2, 3, 4, 5) << QPointF(2, 7) << true;

    QTest::addRow("2,3 4x5 - 2,5") << RectF(2, 3, 4, 5) << QPointF(2, 5) << true;
    QTest::addRow("2,3 4x5 - 1,5") << RectF(2, 3, 4, 5) << QPointF(1, 5) << false;
    QTest::addRow("2,3 4x5 - 3,5") << RectF(2, 3, 4, 5) << QPointF(3, 5) << true;

    QTest::addRow("2,3 4x5 - 4,3") << RectF(2, 3, 4, 5) << QPointF(4, 3) << true;
    QTest::addRow("2,3 4x5 - 4,2") << RectF(2, 3, 4, 5) << QPointF(4, 2) << false;
    QTest::addRow("2,3 4x5 - 4,4") << RectF(2, 3, 4, 5) << QPointF(4, 4) << true;

    QTest::addRow("2,3 4x5 - 6,5") << RectF(2, 3, 4, 5) << QPointF(6, 5) << false;
    QTest::addRow("2,3 4x5 - 5,5") << RectF(2, 3, 4, 5) << QPointF(5, 5) << true;
    QTest::addRow("2,3 4x5 - 7,5") << RectF(2, 3, 4, 5) << QPointF(7, 5) << false;

    QTest::addRow("2,3 4x5 - 4,8") << RectF(2, 3, 4, 5) << QPointF(4, 8) << false;
    QTest::addRow("2,3 4x5 - 4,7") << RectF(2, 3, 4, 5) << QPointF(4, 7) << true;
    QTest::addRow("2,3 4x5 - 4,9") << RectF(2, 3, 4, 5) << QPointF(4, 9) << false;
}

void TestRectF::containsPoint()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, point);

    QTEST(rect.contains(point), "contains");
    QTEST(rect.contains(point.x(), point.y()), "contains");
}

void TestRectF::containsRect_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<RectF>("other");
    QTest::addColumn<bool>("contains");

    QTest::addRow("empty - empty") << RectF() << RectF() << false;
    QTest::addRow("empty - 2,3 4x5") << RectF() << RectF(2, 3, 4, 5) << false;
    QTest::addRow("2,3 4x5 - empty") << RectF(2, 3, 4, 5) << RectF() << false;

    QTest::addRow("2,3 4x5 - 2,3 4x5") << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 5) << true;
    QTest::addRow("2,3 4x5 - 2,3 0x0") << RectF(2, 3, 4, 5) << RectF(2, 3, 0, 0) << true;
    QTest::addRow("2,3 4x5 - 2,3 4x6") << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 6) << false;
    QTest::addRow("2,3 4x5 - 2,3 5x5") << RectF(2, 3, 4, 5) << RectF(2, 3, 5, 5) << false;
    QTest::addRow("2,3 4x5 - 1,3 4x5") << RectF(2, 3, 4, 5) << RectF(1, 3, 4, 5) << false;
    QTest::addRow("2,3 4x5 - 2,2 4x5") << RectF(2, 3, 4, 5) << RectF(2, 2, 4, 5) << false;

    QTest::addRow("5,5 1x1 - 2,2 1x1") << RectF(5, 5, 1, 1) << RectF(2, 2, 1, 1) << false;
    QTest::addRow("2,2 1x1 - 5,5 1x1") << RectF(2, 2, 1, 1) << RectF(5, 5, 1, 1) << false;
}

void TestRectF::containsRect()
{
    QFETCH(RectF, rect);
    QFETCH(RectF, other);

    QTEST(rect.contains(other), "contains");
}

void TestRectF::x_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("x");

    QTest::addRow("default") << RectF() << qreal(0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(2);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << qreal(2);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << qreal(2);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << qreal(2);
}

void TestRectF::x()
{
    QFETCH(RectF, rect);

    QTEST(rect.x(), "x");
}

void TestRectF::setX_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(-1) << RectF(-1, 0, 1, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(0) << RectF(0, 3, 6, 5);
}

void TestRectF::setX()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.setX(value);
    QTEST(rect, "expected");
}

void TestRectF::y_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("y");

    QTest::addRow("default") << RectF() << qreal(0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(3);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << qreal(3);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << qreal(3);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << qreal(3);
}

void TestRectF::y()
{
    QFETCH(RectF, rect);

    QTEST(rect.y(), "y");
}

void TestRectF::setY_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(-1) << RectF(0, -1, 0, 1);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(0) << RectF(2, 0, 4, 8);
}

void TestRectF::setY()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.setY(value);
    QTEST(rect, "expected");
}

void TestRectF::width_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("width");

    QTest::addRow("default") << RectF() << qreal(0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(4);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << qreal(4);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << qreal(4);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << qreal(4);
}

void TestRectF::width()
{
    QFETCH(RectF, rect);

    QTEST(rect.width(), "width");
}

void TestRectF::setWidth_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(1) << RectF(0, 0, 1, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(0) << RectF(2, 3, 0, 5);
}

void TestRectF::setWidth()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.setWidth(value);
    QTEST(rect, "expected");
}

void TestRectF::height_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("height");

    QTest::addRow("default") << RectF() << qreal(0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(5);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << qreal(5);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << qreal(5);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << qreal(5);
}

void TestRectF::height()
{
    QFETCH(RectF, rect);

    QTEST(rect.height(), "height");
}

void TestRectF::setHeight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(1) << RectF(0, 0, 0, 1);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(0) << RectF(2, 3, 4, 0);
}

void TestRectF::setHeight()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.setHeight(value);
    QTEST(rect, "expected");
}

void TestRectF::left_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("left");

    QTest::addRow("default") << RectF() << qreal(0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(2);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << qreal(2);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << qreal(2);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << qreal(2);
}

void TestRectF::left()
{
    QFETCH(RectF, rect);

    QTEST(rect.left(), "left");
}

void TestRectF::setLeft_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(-1) << RectF(-1, 0, 1, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(0) << RectF(0, 3, 6, 5);
}

void TestRectF::setLeft()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.setLeft(value);
    QTEST(rect, "expected");
}

void TestRectF::moveLeft_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(-1) << RectF(-1, 0, 0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(0) << RectF(0, 3, 4, 5);
}

void TestRectF::moveLeft()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.moveLeft(value);
    QTEST(rect, "expected");
}

void TestRectF::top_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("top");

    QTest::addRow("default") << RectF() << qreal(0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(3);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << qreal(3);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << qreal(3);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << qreal(3);
}

void TestRectF::top()
{
    QFETCH(RectF, rect);

    QTEST(rect.top(), "top");
}

void TestRectF::setTop_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(-1) << RectF(0, -1, 0, 1);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(0) << RectF(2, 0, 4, 8);
}

void TestRectF::setTop()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.setTop(value);
    QTEST(rect, "expected");
}

void TestRectF::moveTop_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(-1) << RectF(0, -1, 0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(0) << RectF(2, 0, 4, 5);
}

void TestRectF::moveTop()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.moveTop(value);
    QTEST(rect, "expected");
}

void TestRectF::right_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("right");

    QTest::addRow("default") << RectF() << qreal(0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(6);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << qreal(6);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << qreal(6);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << qreal(6);
}

void TestRectF::right()
{
    QFETCH(RectF, rect);

    QTEST(rect.right(), "right");
}

void TestRectF::setRight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(1) << RectF(0, 0, 1, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(12) << RectF(2, 3, 10, 5);
}

void TestRectF::setRight()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.setRight(value);
    QTEST(rect, "expected");
}

void TestRectF::moveRight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(-1) << RectF(-1, 0, 0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(14) << RectF(10, 3, 4, 5);
}

void TestRectF::moveRight()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.moveRight(value);
    QTEST(rect, "expected");
}

void TestRectF::bottom_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("bottom");

    QTest::addRow("default") << RectF() << qreal(0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(8);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << qreal(8);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << qreal(8);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << qreal(8);
}

void TestRectF::bottom()
{
    QFETCH(RectF, rect);

    QTEST(rect.bottom(), "bottom");
}

void TestRectF::setBottom_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(1) << RectF(0, 0, 0, 1);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(13) << RectF(2, 3, 4, 10);
}

void TestRectF::setBottom()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.setBottom(value);
    QTEST(rect, "expected");
}

void TestRectF::moveBottom_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("empty") << RectF() << qreal(-1) << RectF(0, -1, 0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << qreal(15) << RectF(2, 10, 4, 5);
}

void TestRectF::moveBottom()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, value);

    rect.moveBottom(value);
    QTEST(rect, "expected");
}

void TestRectF::center_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("center");

    QTest::addRow("default") << RectF() << QPointF(0, 0);
    QTest::addRow("2,3 0x0") << RectF(2, 3, 0, 0) << QPointF(2, 3);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(4, 5.5);
    QTest::addRow("2,3 4x6") << RectF(2, 3, 4, 6) << QPointF(4, 6);
    QTest::addRow("2,3 5x5") << RectF(2, 3, 5, 5) << QPointF(4.5, 5.5);
}

void TestRectF::center()
{
    QFETCH(RectF, rect);

    QTEST(rect.center(), "center");
}

void TestRectF::moveCenter_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("center");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default - 0,0") << RectF() << QPointF(0, 0) << RectF();
    QTest::addRow("2,3 0x0 - 2,3") << RectF(2, 3, 0, 0) << QPointF(2, 3) << RectF(2, 3, 0, 0);
    QTest::addRow("2,3 4x5 - 4,5") << RectF(2, 3, 4, 5) << QPointF(4, 5) << RectF(2, 3, 4, 5);
    QTest::addRow("2,3 4x6 - 4,6") << RectF(2, 3, 4, 6) << QPointF(4, 6) << RectF(2, 3, 4, 6);
    QTest::addRow("2,3 5x5 - 4,5") << RectF(2, 3, 5, 5) << QPointF(4, 5) << RectF(2, 3, 5, 5);

    QTest::addRow("2,3 4x5 - 10,10") << RectF(2, 3, 4, 5) << QPointF(10, 10) << RectF(8, 8, 4, 5);
    QTest::addRow("2,3 4x6 - 10,10") << RectF(2, 3, 4, 6) << QPointF(10, 10) << RectF(8, 7, 4, 6);

    QTest::addRow("2,3 5x5 - 10,10") << RectF(2, 3, 5, 5) << QPointF(10, 10) << RectF(8, 8, 5, 5);
    QTest::addRow("2,3 5x6 - 10,10") << RectF(2, 3, 5, 6) << QPointF(10, 10) << RectF(8, 7, 5, 6);
}

void TestRectF::moveCenter()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, center);

    rect.moveCenter(center);
    QCOMPARE(rect.center(), center);
}

void TestRectF::topLeft_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("topLeft");

    QTest::addRow("default") << RectF() << QPointF(0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(2, 3);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(2, 3);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(2, 3);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(2, 3);
}

void TestRectF::topLeft()
{
    QFETCH(RectF, rect);

    QTEST(rect.topLeft(), "topLeft");
}

void TestRectF::setTopLeft_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(-1, -2) << RectF(-1, -2, 1, 2);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(0, -1) << RectF(0, -1, 6, 9);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(0, -1) << RectF(0, -1, 6, 9);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(0, -1) << RectF(0, -1, 6, 9);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(0, -1) << RectF(0, -1, 6, 9);
}

void TestRectF::setTopLeft()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, value);

    rect.setTopLeft(value);
    QTEST(rect, "expected");
}

void TestRectF::moveTopLeft_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(1, 2) << RectF(1, 2, 0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(10, 11) << RectF(10, 11, 4, 5);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(10, 11) << RectF(10, 11, 4, 5);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(10, 11) << RectF(10, 11, 4, 5);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(10, 11) << RectF(10, 11, 4, 5);
}

void TestRectF::moveTopLeft()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, value);

    rect.moveTopLeft(value);
    QTEST(rect, "expected");
}

void TestRectF::topRight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("topRight");

    QTest::addRow("default") << RectF() << QPointF(0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(6, 3);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(6, 3);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(6, 3);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(6, 3);
}

void TestRectF::topRight()
{
    QFETCH(RectF, rect);

    QTEST(rect.topRight(), "topRight");
}

void TestRectF::setTopRight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(1, -2) << RectF(0, -2, 1, 2);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(12, 0) << RectF(2, 0, 10, 8);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(12, 0) << RectF(2, 0, 10, 8);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(12, 0) << RectF(2, 0, 10, 8);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(12, 0) << RectF(2, 0, 10, 8);
}

void TestRectF::setTopRight()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, value);

    rect.setTopRight(value);
    QTEST(rect, "expected");
}

void TestRectF::moveTopRight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(10, 1) << RectF(10, 1, 0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(10, 1) << RectF(6, 1, 4, 5);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(10, 1) << RectF(6, 1, 4, 5);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(10, 1) << RectF(6, 1, 4, 5);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(10, 1) << RectF(6, 1, 4, 5);
}

void TestRectF::moveTopRight()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, value);

    rect.moveTopRight(value);
    QTEST(rect, "expected");
}

void TestRectF::bottomRight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("bottomRight");

    QTest::addRow("default") << RectF() << QPointF(0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(6, 8);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(6, 8);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(6, 8);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(6, 8);
}

void TestRectF::bottomRight()
{
    QFETCH(RectF, rect);

    QTEST(rect.bottomRight(), "bottomRight");
}

void TestRectF::setBottomRight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(1, 2) << RectF(0, 0, 1, 2);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(12, 16) << RectF(2, 3, 10, 13);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(12, 16) << RectF(2, 3, 10, 13);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(12, 16) << RectF(2, 3, 10, 13);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(12, 16) << RectF(2, 3, 10, 13);
}

void TestRectF::setBottomRight()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, value);

    rect.setBottomRight(value);
    QTEST(rect, "expected");
}

void TestRectF::moveBottomRight_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(10, 15) << RectF(10, 15, 0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(10, 15) << RectF(6, 10, 4, 5);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(10, 15) << RectF(6, 10, 4, 5);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(10, 15) << RectF(6, 10, 4, 5);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(10, 15) << RectF(6, 10, 4, 5);
}

void TestRectF::moveBottomRight()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, value);

    rect.moveBottomRight(value);
    QTEST(rect, "expected");
}

void TestRectF::bottomLeft_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("bottomLeft");

    QTest::addRow("default") << RectF() << QPointF(0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(2, 8);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(2, 8);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(2, 8);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(2, 8);
}

void TestRectF::bottomLeft()
{
    QFETCH(RectF, rect);

    QTEST(rect.bottomLeft(), "bottomLeft");
}

void TestRectF::setBottomLeft_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(-1, 2) << RectF(-1, 0, 1, 2);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(0, 10) << RectF(0, 3, 6, 7);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(0, 10) << RectF(0, 3, 6, 7);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(0, 10) << RectF(0, 3, 6, 7);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(0, 10) << RectF(0, 3, 6, 7);
}

void TestRectF::setBottomLeft()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, value);

    rect.setBottomLeft(value);
    QTEST(rect, "expected");
}

void TestRectF::moveBottomLeft_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(1, 15) << RectF(1, 15, 0, 0);
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << QPointF(1, 15) << RectF(1, 10, 4, 5);
    QTest::addRow("QPointF(2, 3) QPointF(6, 8)") << RectF(QPointF(2, 3), QPointF(6, 8)) << QPointF(1, 15) << RectF(1, 10, 4, 5);
    QTest::addRow("QPointF(2, 3) QSizeF(4, 5)") << RectF(QPointF(2, 3), QSizeF(4, 5)) << QPointF(1, 15) << RectF(1, 10, 4, 5);
    QTest::addRow("QRect(2, 3, 4, 5)") << RectF(QRect(2, 3, 4, 5)) << QPointF(1, 15) << RectF(1, 10, 4, 5);
}

void TestRectF::moveBottomLeft()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, value);

    rect.moveBottomLeft(value);
    QTEST(rect, "expected");
}

void TestRectF::transposed_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << RectF();
    QTest::addRow("2,3 4x5") << RectF(2, 3, 4, 5) << RectF(2, 3, 5, 4);
}

void TestRectF::transposed()
{
    QFETCH(RectF, rect);

    QTEST(rect.transposed(), "expected");
}

void TestRectF::translate_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("offset");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(1, 2) << RectF(1, 2, 0, 0);
    QTest::addRow("2,3 4x5 by 1,0") << RectF(2, 3, 4, 5) << QPointF(1, 0) << RectF(3, 3, 4, 5);
    QTest::addRow("2,3 4x5 by 0,1") << RectF(2, 3, 4, 5) << QPointF(0, 1) << RectF(2, 4, 4, 5);
    QTest::addRow("2,3 4x5 by 1,1") << RectF(2, 3, 4, 5) << QPointF(1, 1) << RectF(3, 4, 4, 5);
}

void TestRectF::translate()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, offset);

    {
        RectF tmp = rect;
        tmp.translate(offset);
        QTEST(tmp, "expected");
    }

    {
        RectF tmp = rect;
        tmp.translate(offset.x(), offset.y());
        QTEST(tmp, "expected");
    }
}

void TestRectF::translated_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QPointF>("offset");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default") << RectF() << QPointF(1, 2) << RectF(1, 2, 0, 0);
    QTest::addRow("2,3 4x5 by 1,0") << RectF(2, 3, 4, 5) << QPointF(1, 0) << RectF(3, 3, 4, 5);
    QTest::addRow("2,3 4x5 by 0,1") << RectF(2, 3, 4, 5) << QPointF(0, 1) << RectF(2, 4, 4, 5);
    QTest::addRow("2,3 4x5 by 1,1") << RectF(2, 3, 4, 5) << QPointF(1, 1) << RectF(3, 4, 4, 5);
}

void TestRectF::translated()
{
    QFETCH(RectF, rect);
    QFETCH(QPointF, offset);

    QTEST(rect.translated(offset), "expected");
    QTEST(rect.translated(offset.x(), offset.y()), "expected");
}

void TestRectF::scaled_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default x 1.0") << RectF() << 1.0 << RectF();
    QTest::addRow("default x 2.0") << RectF() << 2.0 << RectF();
    QTest::addRow("empty x 1.0") << RectF(0, 0, 0, 0) << 1.0 << RectF(0, 0, 0, 0);
    QTest::addRow("empty x 2.0") << RectF(0, 0, 0, 0) << 2.0 << RectF(0, 0, 0, 0);
    QTest::addRow("1,1 0,0 x 1.0") << RectF(1, 1, 0, 0) << 1.0 << RectF(1, 1, 0, 0);
    QTest::addRow("1,1 0,0 x 2.0") << RectF(1, 1, 0, 0) << 2.0 << RectF(2, 2, 0, 0);
    QTest::addRow("2,3 4,5 x 1.0") << RectF(2, 3, 4, 5) << 1.0 << RectF(2, 3, 4, 5);
    QTest::addRow("2,3 4,5 x 1.75") << RectF(2, 3, 4, 5) << 1.75 << RectF(3.5, 5.25, 7, 8.75);
    QTest::addRow("2,3 4,5 x 2.0") << RectF(2, 3, 4, 5) << 2.0 << RectF(4, 6, 8, 10);
}

void TestRectF::scaled()
{
    QFETCH(RectF, rect);
    QFETCH(qreal, scale);

    QTEST(rect.scaled(scale), "expected");
}

void TestRectF::adjust_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<int>("x0");
    QTest::addColumn<int>("y0");
    QTest::addColumn<int>("x1");
    QTest::addColumn<int>("y1");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default - -1,0,0,0") << RectF() << -1 << 0 << 0 << 0 << RectF(-1, 0, 1, 0);
    QTest::addRow("default - 0,-1,0,0") << RectF() << 0 << -1 << 0 << 0 << RectF(0, -1, 0, 1);
    QTest::addRow("default - 0,0,1,0") << RectF() << 0 << 0 << 1 << 0 << RectF(0, 0, 1, 0);
    QTest::addRow("default - 0,0,0,1") << RectF() << 0 << 0 << 0 << 1 << RectF(0, 0, 0, 1);
    QTest::addRow("default - -1,0,1,0") << RectF() << -1 << 0 << 1 << 0 << RectF(-1, 0, 2, 0);
    QTest::addRow("default - 0,-1,0,1") << RectF() << 0 << -1 << 0 << 1 << RectF(0, -1, 0, 2);
    QTest::addRow("default - -1,-1,1,1") << RectF() << -1 << -1 << 1 << 1 << RectF(-1, -1, 2, 2);

    QTest::addRow("2,3 4x5 - -1,0,1,0") << RectF(2, 3, 4, 5) << -1 << 0 << 1 << 0 << RectF(1, 3, 6, 5);
    QTest::addRow("2,3 4x5 - 0,-1,0,1") << RectF(2, 3, 4, 5) << 0 << -1 << 0 << 1 << RectF(2, 2, 4, 7);
}

void TestRectF::adjust()
{
    QFETCH(RectF, rect);
    QFETCH(int, x0);
    QFETCH(int, y0);
    QFETCH(int, x1);
    QFETCH(int, y1);

    rect.adjust(x0, y0, x1, y1);
    QTEST(rect, "expected");
}

void TestRectF::adjusted_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<int>("x0");
    QTest::addColumn<int>("y0");
    QTest::addColumn<int>("x1");
    QTest::addColumn<int>("y1");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default - -1,0,0,0") << RectF() << -1 << 0 << 0 << 0 << RectF(-1, 0, 1, 0);
    QTest::addRow("default - 0,-1,0,0") << RectF() << 0 << -1 << 0 << 0 << RectF(0, -1, 0, 1);
    QTest::addRow("default - 0,0,1,0") << RectF() << 0 << 0 << 1 << 0 << RectF(0, 0, 1, 0);
    QTest::addRow("default - 0,0,0,1") << RectF() << 0 << 0 << 0 << 1 << RectF(0, 0, 0, 1);
    QTest::addRow("default - -1,0,1,0") << RectF() << -1 << 0 << 1 << 0 << RectF(-1, 0, 2, 0);
    QTest::addRow("default - 0,-1,0,1") << RectF() << 0 << -1 << 0 << 1 << RectF(0, -1, 0, 2);
    QTest::addRow("default - -1,-1,1,1") << RectF() << -1 << -1 << 1 << 1 << RectF(-1, -1, 2, 2);

    QTest::addRow("2,3 4x5 - -1,0,1,0") << RectF(2, 3, 4, 5) << -1 << 0 << 1 << 0 << RectF(1, 3, 6, 5);
    QTest::addRow("2,3 4x5 - 0,-1,0,1") << RectF(2, 3, 4, 5) << 0 << -1 << 0 << 1 << RectF(2, 2, 4, 7);
}

void TestRectF::adjusted()
{
    QFETCH(RectF, rect);
    QFETCH(int, x0);
    QFETCH(int, y0);
    QFETCH(int, x1);
    QFETCH(int, y1);

    QTEST(rect.adjusted(x0, y0, x1, y1), "expected");
}

void TestRectF::grownBy_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QMargins>("margins");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("2,3 4x5 - 1,0,0,0") << RectF(2, 3, 4, 5) << QMargins(1, 0, 0, 0) << RectF(1, 3, 5, 5);
    QTest::addRow("2,3 4x5 - 0,1,0,0") << RectF(2, 3, 4, 5) << QMargins(0, 1, 0, 0) << RectF(2, 2, 4, 6);
    QTest::addRow("2,3 4x5 - 0,0,1,0") << RectF(2, 3, 4, 5) << QMargins(0, 0, 1, 0) << RectF(2, 3, 5, 5);
    QTest::addRow("2,3 4x5 - 0,0,0,1") << RectF(2, 3, 4, 5) << QMargins(0, 0, 0, 1) << RectF(2, 3, 4, 6);
    QTest::addRow("2,3 4x5 - 1,1,1,1") << RectF(2, 3, 4, 5) << QMargins(1, 1, 1, 1) << RectF(1, 2, 6, 7);
}

void TestRectF::grownBy()
{
    QFETCH(RectF, rect);
    QFETCH(QMargins, margins);

    {
        QTEST(rect.grownBy(margins), "expected");
        QTEST(rect.marginsAdded(margins), "expected");
    }

    {
        RectF tmp = rect;
        tmp += margins;
        QTEST(tmp, "expected");
    }
}

void TestRectF::shrunkBy_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<QMargins>("margins");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("2,3 4x5 - 1,0,0,0") << RectF(2, 3, 4, 5) << QMargins(1, 0, 0, 0) << RectF(3, 3, 3, 5);
    QTest::addRow("2,3 4x5 - 0,1,0,0") << RectF(2, 3, 4, 5) << QMargins(0, 1, 0, 0) << RectF(2, 4, 4, 4);
    QTest::addRow("2,3 4x5 - 0,0,1,0") << RectF(2, 3, 4, 5) << QMargins(0, 0, 1, 0) << RectF(2, 3, 3, 5);
    QTest::addRow("2,3 4x5 - 0,0,0,1") << RectF(2, 3, 4, 5) << QMargins(0, 0, 0, 1) << RectF(2, 3, 4, 4);
    QTest::addRow("2,3 4x5 - 1,1,1,1") << RectF(2, 3, 4, 5) << QMargins(1, 1, 1, 1) << RectF(3, 4, 2, 3);
}

void TestRectF::shrunkBy()
{
    QFETCH(RectF, rect);
    QFETCH(QMargins, margins);

    {
        QTEST(rect.shrunkBy(margins), "expected");
        QTEST(rect.marginsRemoved(margins), "expected");
    }

    {
        RectF tmp = rect;
        tmp -= margins;
        QTEST(tmp, "expected");
    }
}

void TestRectF::united_data()
{
    QTest::addColumn<RectF>("rect1");
    QTest::addColumn<RectF>("rect2");
    QTest::addColumn<RectF>("result");

    QTest::addRow("empty - empty") << RectF() << RectF() << RectF();
    QTest::addRow("2,3 4x5 - empty") << RectF(2, 3, 4, 5) << RectF() << RectF(2, 3, 4, 5);
    QTest::addRow("empty - 2,3 4x5") << RectF() << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 5);
    QTest::addRow("2,3 4x5 - 2,3 4x5") << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 5) << RectF(2, 3, 4, 5);

    QTest::addRow("1,0 1x5 - 5,0 1x5") << RectF(1, 0, 1, 5) << RectF(5, 0, 1, 5) << RectF(1, 0, 5, 5);
    QTest::addRow("0,1 5x1 - 0,5 5x1") << RectF(0, 1, 5, 1) << RectF(0, 5, 5, 1) << RectF(0, 1, 5, 5);

    QTest::addRow("2,3 4x5 - 10,10 0x0") << RectF(2, 3, 4, 5) << RectF(10, 10, 0, 0) << RectF(2, 3, 4, 5);
}

void TestRectF::united()
{
    QFETCH(RectF, rect1);
    QFETCH(RectF, rect2);

    {
        QTEST(rect1.united(rect2), "result");
    }

    {
        QTEST(rect1 | rect2, "result");
    }

    {
        RectF tmp = rect1;
        tmp |= rect2;
        QTEST(tmp, "result");
    }
}

void TestRectF::intersects_data()
{
    QTest::addColumn<RectF>("rect1");
    QTest::addColumn<RectF>("rect2");
    QTest::addColumn<bool>("intersects");

    QTest::addRow("empty - empty") << RectF() << RectF() << false;
    QTest::addRow("2,3 4x5 - empty") << RectF(2, 3, 4, 5) << RectF() << false;

    QTest::addRow("0,0 5x5 - 0,-5 5x5") << RectF(0, 0, 5, 5) << RectF(0, -5, 5, 5) << false;
    QTest::addRow("0,0 5x5 - -5,0 5x5") << RectF(0, 0, 5, 5) << RectF(-5, 0, 5, 5) << false;
    QTest::addRow("0,0 5x5 - -5,-5 5x5") << RectF(0, 0, 5, 5) << RectF(-5, -5, 5, 5) << false;
    QTest::addRow("0,0 5x5 - 0,5 5x5") << RectF(0, 0, 5, 5) << RectF(0, 5, 5, 5) << false;
    QTest::addRow("0,0 5x5 - 5,0 5x5") << RectF(0, 0, 5, 5) << RectF(5, 0, 5, 5) << false;
    QTest::addRow("0,0 5x5 - 5,5 5x5") << RectF(0, 0, 5, 5) << RectF(5, 5, 5, 5) << false;

    QTest::addRow("0,0 5x5 - 2,2 5x5") << RectF(0, 0, 5, 5) << RectF(2, 2, 5, 5) << true;
    QTest::addRow("0,0 5x5 - 2,2 0x0") << RectF(0, 0, 5, 5) << RectF(2, 2, 0, 0) << false;
}

void TestRectF::intersects()
{
    QFETCH(RectF, rect1);
    QFETCH(RectF, rect2);

    QTEST(rect1.intersects(rect2), "intersects");
    QTEST(rect2.intersects(rect1), "intersects");
}

void TestRectF::intersected_data()
{
    QTest::addColumn<RectF>("rect1");
    QTest::addColumn<RectF>("rect2");
    QTest::addColumn<RectF>("result");

    QTest::addRow("empty - empty") << RectF() << RectF() << RectF();
    QTest::addRow("2,3 4x5 - empty") << RectF(2, 3, 4, 5) << RectF() << RectF();

    QTest::addRow("0,0 5x5 - 0,-5 5x5") << RectF(0, 0, 5, 5) << RectF(0, -5, 5, 5) << RectF();
    QTest::addRow("0,0 5x5 - -5,0 5x5") << RectF(0, 0, 5, 5) << RectF(-5, 0, 5, 5) << RectF();
    QTest::addRow("0,0 5x5 - -5,-5 5x5") << RectF(0, 0, 5, 5) << RectF(-5, -5, 5, 5) << RectF();
    QTest::addRow("0,0 5x5 - 0,5 5x5") << RectF(0, 0, 5, 5) << RectF(0, 5, 5, 5) << RectF();
    QTest::addRow("0,0 5x5 - 5,0 5x5") << RectF(0, 0, 5, 5) << RectF(5, 0, 5, 5) << RectF();
    QTest::addRow("0,0 5x5 - 5,5 5x5") << RectF(0, 0, 5, 5) << RectF(5, 5, 5, 5) << RectF();

    QTest::addRow("0,0 5x5 - 2,2 5x5") << RectF(0, 0, 5, 5) << RectF(2, 2, 5, 5) << RectF(2, 2, 3, 3);
    QTest::addRow("0,0 5x5 - 2,2 0x0") << RectF(0, 0, 5, 5) << RectF(2, 2, 0, 0) << RectF();
}

void TestRectF::intersected()
{
    QFETCH(RectF, rect1);
    QFETCH(RectF, rect2);

    {
        QTEST(rect1.intersected(rect2), "result");
        QTEST(rect2.intersected(rect1), "result");
    }

    {
        QTEST(rect1 & rect2, "result");
        QTEST(rect2 & rect1, "result");
    }

    {
        RectF tmp = rect1;
        tmp &= rect2;
        QTEST(tmp, "result");
    }

    {
        RectF tmp = rect2;
        tmp &= rect1;
        QTEST(tmp, "result");
    }
}

void TestRectF::rounded_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << RectF() << Rect();

    QTest::addRow("1,2 3,4") << RectF(QPointF(1, 2), QPointF(3, 4)) << Rect(QPoint(1, 2), QPoint(3, 4));
    QTest::addRow("1.1,2.1 3.1,4.1") << RectF(QPointF(1.1, 2.1), QPointF(3.1, 4.1)) << Rect(QPoint(1, 2), QPoint(3, 4));
    QTest::addRow("1.5,2.5 3.5,4.5") << RectF(QPointF(1.5, 2.5), QPointF(3.5, 4.5)) << Rect(QPoint(2, 3), QPoint(4, 5));
    QTest::addRow("1.9,2.9 3.9,4.9") << RectF(QPointF(1.9, 2.9), QPointF(3.9, 4.9)) << Rect(QPoint(2, 3), QPoint(4, 5));

    QTest::addRow("-3,-4 -1,-2") << RectF(QPointF(-3, -4), QPointF(-1, -2)) << Rect(QPoint(-3, -4), QPoint(-1, -2));
    QTest::addRow("-3.1,-4.1 -1.1,-2.1") << RectF(QPointF(-3.1, -4.1), QPointF(-1.1, -2.1)) << Rect(QPoint(-3, -4), QPoint(-1, -2));
    QTest::addRow("-3.5,-4.5 -1.5,-2.5") << RectF(QPointF(-3.5, -4.5), QPointF(-1.5, -2.5)) << Rect(QPoint(-4, -5), QPoint(-2, -3));
    QTest::addRow("-3.9,-4.9 -1.9,-2.9") << RectF(QPointF(-3.9, -4.9), QPointF(-1.9, -2.9)) << Rect(QPoint(-4, -5), QPoint(-2, -3));
}

void TestRectF::rounded()
{
    QFETCH(RectF, rect);
    QFETCH(Rect, expected);

    const Rect rounded = rect.rounded();
    QCOMPARE(rounded, expected);
    QVERIFY(std::abs(rect.left() - rounded.left()) <= 0.5);
    QVERIFY(std::abs(rect.top() - rounded.top()) <= 0.5);
    QVERIFY(std::abs(rect.right() - rounded.right()) <= 0.5);
    QVERIFY(std::abs(rect.bottom() - rounded.bottom()) <= 0.5);

    QCOMPARE(rect.toRect(), expected);
}

void TestRectF::roundedIn_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << RectF() << Rect();

    QTest::addRow("1,2 3,4") << RectF(QPointF(1, 2), QPointF(3, 4)) << Rect(QPoint(1, 2), QPoint(3, 4));
    QTest::addRow("1.1,2.1 3.1,4.1") << RectF(QPointF(1.1, 2.1), QPointF(3.1, 4.1)) << Rect(QPoint(2, 3), QPoint(3, 4));
    QTest::addRow("1.5,2.5 3.5,4.5") << RectF(QPointF(1.5, 2.5), QPointF(3.5, 4.5)) << Rect(QPoint(2, 3), QPoint(3, 4));
    QTest::addRow("1.9,2.9 3.9,4.9") << RectF(QPointF(1.9, 2.9), QPointF(3.9, 4.9)) << Rect(QPoint(2, 3), QPoint(3, 4));

    QTest::addRow("-3,-4 -1,-2") << RectF(QPointF(-3, -4), QPointF(-1, -2)) << Rect(QPoint(-3, -4), QPoint(-1, -2));
    QTest::addRow("-3.1,-4.1 -1.1,-2.1") << RectF(QPointF(-3.1, -4.1), QPointF(-1.1, -2.1)) << Rect(QPoint(-3, -4), QPoint(-2, -3));
    QTest::addRow("-3.5,-4.5 -1.5,-2.5") << RectF(QPointF(-3.5, -4.5), QPointF(-1.5, -2.5)) << Rect(QPoint(-3, -4), QPoint(-2, -3));
    QTest::addRow("-3.9,-4.9 -1.9,-2.9") << RectF(QPointF(-3.9, -4.9), QPointF(-1.9, -2.9)) << Rect(QPoint(-3, -4), QPoint(-2, -3));
}

void TestRectF::roundedIn()
{
    QFETCH(RectF, rect);
    QFETCH(Rect, expected);

    const Rect rounded = rect.roundedIn();
    QCOMPARE(rounded, expected);
    QVERIFY(std::abs(rect.left() - rounded.left()) < 1.0);
    QVERIFY(std::abs(rect.top() - rounded.top()) < 1.0);
    QVERIFY(std::abs(rect.right() - rounded.right()) < 1.0);
    QVERIFY(std::abs(rect.bottom() - rounded.bottom()) < 1.0);
}

void TestRectF::roundedOut_data()
{
    QTest::addColumn<RectF>("rect");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << RectF() << Rect();

    QTest::addRow("1,2 3,4") << RectF(QPointF(1, 2), QPointF(3, 4)) << Rect(QPoint(1, 2), QPoint(3, 4));
    QTest::addRow("1.1,2.1 3.1,4.1") << RectF(QPointF(1.1, 2.1), QPointF(3.1, 4.1)) << Rect(QPoint(1, 2), QPoint(4, 5));
    QTest::addRow("1.5,2.5 3.5,4.5") << RectF(QPointF(1.5, 2.5), QPointF(3.5, 4.5)) << Rect(QPoint(1, 2), QPoint(4, 5));
    QTest::addRow("1.9,2.9 3.9,4.9") << RectF(QPointF(1.9, 2.9), QPointF(3.9, 4.9)) << Rect(QPoint(1, 2), QPoint(4, 5));

    QTest::addRow("-3,-4 -1,-2") << RectF(QPointF(-3, -4), QPointF(-1, -2)) << Rect(QPoint(-3, -4), QPoint(-1, -2));
    QTest::addRow("-3.1,-4.1 -1.1,-2.1") << RectF(QPointF(-3.1, -4.1), QPointF(-1.1, -2.1)) << Rect(QPoint(-4, -5), QPoint(-1, -2));
    QTest::addRow("-3.5,-4.5 -1.5,-2.5") << RectF(QPointF(-3.5, -4.5), QPointF(-1.5, -2.5)) << Rect(QPoint(-4, -5), QPoint(-1, -2));
    QTest::addRow("-3.9,-4.9 -1.9,-2.9") << RectF(QPointF(-3.9, -4.9), QPointF(-1.9, -2.9)) << Rect(QPoint(-4, -5), QPoint(-1, -2));
}

void TestRectF::roundedOut()
{
    QFETCH(RectF, rect);
    QFETCH(Rect, expected);

    const Rect rounded = rect.roundedOut();
    QCOMPARE(rounded, expected);
    QVERIFY(std::abs(rect.left() - rounded.left()) < 1.0);
    QVERIFY(std::abs(rect.top() - rounded.top()) < 1.0);
    QVERIFY(std::abs(rect.right() - rounded.right()) < 1.0);
    QVERIFY(std::abs(rect.bottom() - rounded.bottom()) < 1.0);

    QCOMPARE(rect.toAlignedRect(), expected);
}

void TestRectF::implicitConversions()
{
    const auto qrectFunction = [](const QRectF &rect) { };
    qrectFunction(RectF());

    const auto rectFunction = [](const RectF &rect) { };
    rectFunction(QRectF());

    {
        const RectF rect;
        const QRectF qrect = rect;
        QCOMPARE(qrect, QRectF());
        QVERIFY(qrect == rect);
        QVERIFY(rect == qrect);
    }

    {
        const RectF rect(1, 2, 3, 4);
        const QRectF qrect = rect;
        QCOMPARE(qrect, QRectF(1, 2, 3, 4));
        QVERIFY(qrect == rect);
        QVERIFY(rect == qrect);
    }

    {
        RectF rect(0, 0, 1, 1);
        rect |= QRectF(1, 1, 1, 1);
        QCOMPARE(rect, RectF(0, 0, 2, 2));
    }

    {
        QRectF qrect(0, 0, 1, 1);
        qrect |= RectF(1, 1, 1, 1);
        QCOMPARE(qrect, QRectF(0, 0, 2, 2));
    }

    {
        RectF rect(0, 0, 3, 3);
        rect &= QRectF(1, 1, 3, 3);
        QCOMPARE(rect, RectF(1, 1, 2, 2));
    }

    {
        QRectF qrect(0, 0, 3, 3);
        qrect &= RectF(1, 1, 3, 3);
        QCOMPARE(qrect, QRectF(1, 1, 2, 2));
    }
}

QTEST_MAIN(TestRectF)

#include "test_rectf.moc"
