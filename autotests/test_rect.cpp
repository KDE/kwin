/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "core/rect.h"

using namespace KWin;

class TestRect : public QObject
{
    Q_OBJECT

public:
    TestRect() = default;

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
    void implicitConversions();
};

void TestRect::equals_data()
{
    QTest::addColumn<Rect>("rect1");
    QTest::addColumn<Rect>("rect2");
    QTest::addColumn<bool>("expected");

    QTest::addRow("default == default") << Rect() << Rect() << true;
    QTest::addRow("2,3 4x5 == default") << Rect(2, 3, 4, 5) << Rect() << false;
    QTest::addRow("default == 2,3 4x5") << Rect() << Rect(2, 3, 4, 5) << false;
    QTest::addRow("2,3 4x5 == 2,3 4x5") << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 5) << true;

    QTest::addRow("2,3 4x5 == 0,3 4x5") << Rect(2, 3, 4, 5) << Rect(0, 3, 4, 5) << false;
    QTest::addRow("2,3 4x5 == 2,0 4x5") << Rect(2, 3, 4, 5) << Rect(2, 0, 4, 5) << false;
    QTest::addRow("2,3 4x5 == 2,3 0x5") << Rect(2, 3, 4, 5) << Rect(2, 3, 0, 5) << false;
    QTest::addRow("2,3 4x5 == 2,3 4x0") << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 0) << false;
}

void TestRect::equals()
{
    QFETCH(Rect, rect1);
    QFETCH(Rect, rect2);

    QTEST(rect1 == rect2, "expected");
}

void TestRect::notEquals_data()
{
    QTest::addColumn<Rect>("rect1");
    QTest::addColumn<Rect>("rect2");
    QTest::addColumn<bool>("expected");

    QTest::addRow("default == default") << Rect() << Rect() << false;
    QTest::addRow("2,3 4x5 == default") << Rect(2, 3, 4, 5) << Rect() << true;
    QTest::addRow("default == 2,3 4x5") << Rect() << Rect(2, 3, 4, 5) << true;
    QTest::addRow("2,3 4x5 == 2,3 4x5") << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 5) << false;

    QTest::addRow("2,3 4x5 == 0,3 4x5") << Rect(2, 3, 4, 5) << Rect(0, 3, 4, 5) << true;
    QTest::addRow("2,3 4x5 == 2,0 4x5") << Rect(2, 3, 4, 5) << Rect(2, 0, 4, 5) << true;
    QTest::addRow("2,3 4x5 == 2,3 0x5") << Rect(2, 3, 4, 5) << Rect(2, 3, 0, 5) << true;
    QTest::addRow("2,3 4x5 == 2,3 4x0") << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 0) << true;
}

void TestRect::notEquals()
{
    QFETCH(Rect, rect1);
    QFETCH(Rect, rect2);

    QTEST(rect1 != rect2, "expected");
}

void TestRect::empty_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<bool>("empty");

    QTest::addRow("default") << Rect() << true;

    QTest::addRow("0,0 0x0") << Rect(0, 0, 0, 0) << true;
    QTest::addRow("2,3 0x0") << Rect(2, 3, 0, 0) << true;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << false;

    QTest::addRow("QPoint(1, 1) QPoint(1, 1)") << Rect(QPoint(1, 1), QPoint(1, 1)) << true;
    QTest::addRow("QPoint(1, 1) QPoint(1, 2)") << Rect(QPoint(1, 1), QPoint(1, 2)) << true;
    QTest::addRow("QPoint(1, 1) QPoint(2, 1)") << Rect(QPoint(1, 1), QPoint(2, 1)) << true;
    QTest::addRow("QPoint(1, 1) QPoint(2, 2)") << Rect(QPoint(1, 1), QPoint(2, 2)) << false;

    QTest::addRow("QPoint(1, 1) QSize(0, 0)") << Rect(QPoint(1, 1), QSize(0, 0)) << true;
    QTest::addRow("QPoint(1, 1) QSize(0, 1)") << Rect(QPoint(1, 1), QSize(0, 1)) << true;
    QTest::addRow("QPoint(1, 1) QSize(1, 0)") << Rect(QPoint(1, 1), QSize(1, 0)) << true;
    QTest::addRow("QPoint(1, 1) QSize(1, 1)") << Rect(QPoint(1, 1), QSize(1, 1)) << false;

    QTest::addRow("QRect()") << Rect(QRect()) << true;
    QTest::addRow("QRect(1, 1, 0, 0)") << Rect(QRect(1, 1, 0, 0)) << true;
    QTest::addRow("QRect(1, 1, 0, 1)") << Rect(QRect(1, 1, 0, 1)) << true;
    QTest::addRow("QRect(1, 1, 1, 0)") << Rect(QRect(1, 1, 1, 0)) << true;
    QTest::addRow("QRect(1, 1, 1, 1)") << Rect(QRect(1, 1, 1, 1)) << false;
}

void TestRect::empty()
{
    QFETCH(Rect, rect);

    QTEST(rect.isEmpty(), "empty");
}

void TestRect::containsPoint_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("point");
    QTest::addColumn<bool>("contains");

    QTest::addRow("empty - 0,0") << Rect() << QPoint(0, 0) << false;
    QTest::addRow("empty - 1,1") << Rect() << QPoint(1, 1) << false;

    QTest::addRow("2,3 4x5 - 1,2") << Rect(2, 3, 4, 5) << QPoint(1, 2) << false;
    QTest::addRow("2,3 4x5 - 7,9") << Rect(2, 3, 4, 5) << QPoint(7, 9) << false;
    QTest::addRow("2,3 4x5 - 1,9") << Rect(2, 3, 4, 5) << QPoint(1, 9) << false;
    QTest::addRow("2,3 4x5 - 7,2") << Rect(2, 3, 4, 5) << QPoint(7, 2) << false;

    QTest::addRow("2,3 4x5 - 2,3") << Rect(2, 3, 4, 5) << QPoint(2, 3) << true;
    QTest::addRow("2,3 4x5 - 6,3") << Rect(2, 3, 4, 5) << QPoint(6, 3) << false;
    QTest::addRow("2,3 4x5 - 5,3") << Rect(2, 3, 4, 5) << QPoint(5, 3) << true;
    QTest::addRow("2,3 4x5 - 6,8") << Rect(2, 3, 4, 5) << QPoint(6, 8) << false;
    QTest::addRow("2,3 4x5 - 5,7") << Rect(2, 3, 4, 5) << QPoint(5, 7) << true;
    QTest::addRow("2,3 4x5 - 2,8") << Rect(2, 3, 4, 5) << QPoint(2, 8) << false;
    QTest::addRow("2,3 4x5 - 2,7") << Rect(2, 3, 4, 5) << QPoint(2, 7) << true;

    QTest::addRow("2,3 4x5 - 2,5") << Rect(2, 3, 4, 5) << QPoint(2, 5) << true;
    QTest::addRow("2,3 4x5 - 1,5") << Rect(2, 3, 4, 5) << QPoint(1, 5) << false;
    QTest::addRow("2,3 4x5 - 3,5") << Rect(2, 3, 4, 5) << QPoint(3, 5) << true;

    QTest::addRow("2,3 4x5 - 4,3") << Rect(2, 3, 4, 5) << QPoint(4, 3) << true;
    QTest::addRow("2,3 4x5 - 4,2") << Rect(2, 3, 4, 5) << QPoint(4, 2) << false;
    QTest::addRow("2,3 4x5 - 4,4") << Rect(2, 3, 4, 5) << QPoint(4, 4) << true;

    QTest::addRow("2,3 4x5 - 6,5") << Rect(2, 3, 4, 5) << QPoint(6, 5) << false;
    QTest::addRow("2,3 4x5 - 5,5") << Rect(2, 3, 4, 5) << QPoint(5, 5) << true;
    QTest::addRow("2,3 4x5 - 7,5") << Rect(2, 3, 4, 5) << QPoint(7, 5) << false;

    QTest::addRow("2,3 4x5 - 4,8") << Rect(2, 3, 4, 5) << QPoint(4, 8) << false;
    QTest::addRow("2,3 4x5 - 4,7") << Rect(2, 3, 4, 5) << QPoint(4, 7) << true;
    QTest::addRow("2,3 4x5 - 4,9") << Rect(2, 3, 4, 5) << QPoint(4, 9) << false;
}

void TestRect::containsPoint()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, point);

    QTEST(rect.contains(point), "contains");
    QTEST(rect.contains(point.x(), point.y()), "contains");
}

void TestRect::containsRect_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<Rect>("other");
    QTest::addColumn<bool>("contains");

    QTest::addRow("empty - empty") << Rect() << Rect() << false;
    QTest::addRow("empty - 2,3 4x5") << Rect() << Rect(2, 3, 4, 5) << false;
    QTest::addRow("2,3 4x5 - empty") << Rect(2, 3, 4, 5) << Rect() << false;

    QTest::addRow("2,3 4x5 - 2,3 4x5") << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 5) << true;
    QTest::addRow("2,3 4x5 - 2,3 0x0") << Rect(2, 3, 4, 5) << Rect(2, 3, 0, 0) << true;
    QTest::addRow("2,3 4x5 - 2,3 4x6") << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 6) << false;
    QTest::addRow("2,3 4x5 - 2,3 5x5") << Rect(2, 3, 4, 5) << Rect(2, 3, 5, 5) << false;
    QTest::addRow("2,3 4x5 - 1,3 4x5") << Rect(2, 3, 4, 5) << Rect(1, 3, 4, 5) << false;
    QTest::addRow("2,3 4x5 - 2,2 4x5") << Rect(2, 3, 4, 5) << Rect(2, 2, 4, 5) << false;

    QTest::addRow("5,5 1x1 - 2,2 1x1") << Rect(5, 5, 1, 1) << Rect(2, 2, 1, 1) << false;
    QTest::addRow("2,2 1x1 - 5,5 1x1") << Rect(2, 2, 1, 1) << Rect(5, 5, 1, 1) << false;
}

void TestRect::containsRect()
{
    QFETCH(Rect, rect);
    QFETCH(Rect, other);

    QTEST(rect.contains(other), "contains");
}

void TestRect::x_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("x");

    QTest::addRow("default") << Rect() << 0;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 2;
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << 2;
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << 2;
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << 2;
}

void TestRect::x()
{
    QFETCH(Rect, rect);

    QTEST(rect.x(), "x");
}

void TestRect::setX_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << -1 << Rect(-1, 0, 1, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 0 << Rect(0, 3, 6, 5);
}

void TestRect::setX()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.setX(value);
    QTEST(rect, "expected");
}

void TestRect::y_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("y");

    QTest::addRow("default") << Rect() << 0;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 3;
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << 3;
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << 3;
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << 3;
}

void TestRect::y()
{
    QFETCH(Rect, rect);

    QTEST(rect.y(), "y");
}

void TestRect::setY_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << -1 << Rect(0, -1, 0, 1);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 0 << Rect(2, 0, 4, 8);
}

void TestRect::setY()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.setY(value);
    QTEST(rect, "expected");
}

void TestRect::width_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("width");

    QTest::addRow("default") << Rect() << 0;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 4;
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << 4;
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << 4;
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << 4;
}

void TestRect::width()
{
    QFETCH(Rect, rect);

    QTEST(rect.width(), "width");
}

void TestRect::setWidth_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << 1 << Rect(0, 0, 1, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 0 << Rect(2, 3, 0, 5);
}

void TestRect::setWidth()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.setWidth(value);
    QTEST(rect, "expected");
}

void TestRect::height_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("height");

    QTest::addRow("default") << Rect() << 0;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 5;
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << 5;
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << 5;
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << 5;
}

void TestRect::height()
{
    QFETCH(Rect, rect);

    QTEST(rect.height(), "height");
}

void TestRect::setHeight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << 1 << Rect(0, 0, 0, 1);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 0 << Rect(2, 3, 4, 0);
}

void TestRect::setHeight()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.setHeight(value);
    QTEST(rect, "expected");
}

void TestRect::left_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("left");

    QTest::addRow("default") << Rect() << 0;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 2;
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << 2;
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << 2;
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << 2;
}

void TestRect::left()
{
    QFETCH(Rect, rect);

    QTEST(rect.left(), "left");
}

void TestRect::setLeft_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << -1 << Rect(-1, 0, 1, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 0 << Rect(0, 3, 6, 5);
}

void TestRect::setLeft()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.setLeft(value);
    QTEST(rect, "expected");
}

void TestRect::moveLeft_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << -1 << Rect(-1, 0, 0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 0 << Rect(0, 3, 4, 5);
}

void TestRect::moveLeft()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.moveLeft(value);
    QTEST(rect, "expected");
}

void TestRect::top_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("top");

    QTest::addRow("default") << Rect() << 0;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 3;
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << 3;
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << 3;
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << 3;
}

void TestRect::top()
{
    QFETCH(Rect, rect);

    QTEST(rect.top(), "top");
}

void TestRect::setTop_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << -1 << Rect(0, -1, 0, 1);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 0 << Rect(2, 0, 4, 8);
}

void TestRect::setTop()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.setTop(value);
    QTEST(rect, "expected");
}

void TestRect::moveTop_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << -1 << Rect(0, -1, 0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 0 << Rect(2, 0, 4, 5);
}

void TestRect::moveTop()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.moveTop(value);
    QTEST(rect, "expected");
}

void TestRect::right_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("right");

    QTest::addRow("default") << Rect() << 0;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 6;
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << 6;
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << 6;
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << 6;
}

void TestRect::right()
{
    QFETCH(Rect, rect);

    QTEST(rect.right(), "right");
}

void TestRect::setRight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << 1 << Rect(0, 0, 1, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 12 << Rect(2, 3, 10, 5);
}

void TestRect::setRight()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.setRight(value);
    QTEST(rect, "expected");
}

void TestRect::moveRight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << -1 << Rect(-1, 0, 0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 14 << Rect(10, 3, 4, 5);
}

void TestRect::moveRight()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.moveRight(value);
    QTEST(rect, "expected");
}

void TestRect::bottom_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("bottom");

    QTest::addRow("default") << Rect() << 0;
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 8;
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << 8;
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << 8;
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << 8;
}

void TestRect::bottom()
{
    QFETCH(Rect, rect);

    QTEST(rect.bottom(), "bottom");
}

void TestRect::setBottom_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << 1 << Rect(0, 0, 0, 1);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 13 << Rect(2, 3, 4, 10);
}

void TestRect::setBottom()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.setBottom(value);
    QTEST(rect, "expected");
}

void TestRect::moveBottom_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("empty") << Rect() << -1 << Rect(0, -1, 0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << 15 << Rect(2, 10, 4, 5);
}

void TestRect::moveBottom()
{
    QFETCH(Rect, rect);
    QFETCH(int, value);

    rect.moveBottom(value);
    QTEST(rect, "expected");
}

void TestRect::center_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("center");

    QTest::addRow("default") << Rect() << QPoint(0, 0);
    QTest::addRow("2,3 0x0") << Rect(2, 3, 0, 0) << QPoint(2, 3);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(4, 5);
    QTest::addRow("2,3 4x6") << Rect(2, 3, 4, 6) << QPoint(4, 6);
    QTest::addRow("2,3 5x5") << Rect(2, 3, 5, 5) << QPoint(4, 5);
}

void TestRect::center()
{
    QFETCH(Rect, rect);

    QTEST(rect.center(), "center");
}

void TestRect::moveCenter_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("center");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default - 0,0") << Rect() << QPoint(0, 0) << Rect();
    QTest::addRow("2,3 0x0 - 2,3") << Rect(2, 3, 0, 0) << QPoint(2, 3) << Rect(2, 3, 0, 0);
    QTest::addRow("2,3 4x5 - 4,5") << Rect(2, 3, 4, 5) << QPoint(4, 5) << Rect(2, 3, 4, 5);
    QTest::addRow("2,3 4x6 - 4,6") << Rect(2, 3, 4, 6) << QPoint(4, 6) << Rect(2, 3, 4, 6);
    QTest::addRow("2,3 5x5 - 4,5") << Rect(2, 3, 5, 5) << QPoint(4, 5) << Rect(2, 3, 5, 5);

    QTest::addRow("2,3 4x5 - 10,10") << Rect(2, 3, 4, 5) << QPoint(10, 10) << Rect(8, 8, 4, 5);
    QTest::addRow("2,3 4x6 - 10,10") << Rect(2, 3, 4, 6) << QPoint(10, 10) << Rect(8, 7, 4, 6);

    QTest::addRow("2,3 5x5 - 10,10") << Rect(2, 3, 5, 5) << QPoint(10, 10) << Rect(8, 8, 5, 5);
    QTest::addRow("2,3 5x6 - 10,10") << Rect(2, 3, 5, 6) << QPoint(10, 10) << Rect(8, 7, 5, 6);
}

void TestRect::moveCenter()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, center);

    rect.moveCenter(center);
    QCOMPARE(rect.center(), center);
}

void TestRect::topLeft_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("topLeft");

    QTest::addRow("default") << Rect() << QPoint(0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(2, 3);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(2, 3);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(2, 3);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(2, 3);
}

void TestRect::topLeft()
{
    QFETCH(Rect, rect);

    QTEST(rect.topLeft(), "topLeft");
}

void TestRect::setTopLeft_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(-1, -2) << Rect(-1, -2, 1, 2);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(0, -1) << Rect(0, -1, 6, 9);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(0, -1) << Rect(0, -1, 6, 9);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(0, -1) << Rect(0, -1, 6, 9);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(0, -1) << Rect(0, -1, 6, 9);
}

void TestRect::setTopLeft()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, value);

    rect.setTopLeft(value);
    QTEST(rect, "expected");
}

void TestRect::moveTopLeft_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(1, 2) << Rect(1, 2, 0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(10, 11) << Rect(10, 11, 4, 5);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(10, 11) << Rect(10, 11, 4, 5);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(10, 11) << Rect(10, 11, 4, 5);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(10, 11) << Rect(10, 11, 4, 5);
}

void TestRect::moveTopLeft()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, value);

    rect.moveTopLeft(value);
    QTEST(rect, "expected");
}

void TestRect::topRight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("topRight");

    QTest::addRow("default") << Rect() << QPoint(0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(6, 3);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(6, 3);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(6, 3);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(6, 3);
}

void TestRect::topRight()
{
    QFETCH(Rect, rect);

    QTEST(rect.topRight(), "topRight");
}

void TestRect::setTopRight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(1, -2) << Rect(0, -2, 1, 2);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(12, 0) << Rect(2, 0, 10, 8);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(12, 0) << Rect(2, 0, 10, 8);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(12, 0) << Rect(2, 0, 10, 8);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(12, 0) << Rect(2, 0, 10, 8);
}

void TestRect::setTopRight()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, value);

    rect.setTopRight(value);
    QTEST(rect, "expected");
}

void TestRect::moveTopRight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(10, 1) << Rect(10, 1, 0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(10, 1) << Rect(6, 1, 4, 5);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(10, 1) << Rect(6, 1, 4, 5);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(10, 1) << Rect(6, 1, 4, 5);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(10, 1) << Rect(6, 1, 4, 5);
}

void TestRect::moveTopRight()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, value);

    rect.moveTopRight(value);
    QTEST(rect, "expected");
}

void TestRect::bottomRight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("bottomRight");

    QTest::addRow("default") << Rect() << QPoint(0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(6, 8);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(6, 8);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(6, 8);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(6, 8);
}

void TestRect::bottomRight()
{
    QFETCH(Rect, rect);

    QTEST(rect.bottomRight(), "bottomRight");
}

void TestRect::setBottomRight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(1, 2) << Rect(0, 0, 1, 2);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(12, 16) << Rect(2, 3, 10, 13);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(12, 16) << Rect(2, 3, 10, 13);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(12, 16) << Rect(2, 3, 10, 13);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(12, 16) << Rect(2, 3, 10, 13);
}

void TestRect::setBottomRight()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, value);

    rect.setBottomRight(value);
    QTEST(rect, "expected");
}

void TestRect::moveBottomRight_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(10, 15) << Rect(10, 15, 0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(10, 15) << Rect(6, 10, 4, 5);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(10, 15) << Rect(6, 10, 4, 5);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(10, 15) << Rect(6, 10, 4, 5);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(10, 15) << Rect(6, 10, 4, 5);
}

void TestRect::moveBottomRight()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, value);

    rect.moveBottomRight(value);
    QTEST(rect, "expected");
}

void TestRect::bottomLeft_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("bottomLeft");

    QTest::addRow("default") << Rect() << QPoint(0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(2, 8);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(2, 8);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(2, 8);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(2, 8);
}

void TestRect::bottomLeft()
{
    QFETCH(Rect, rect);

    QTEST(rect.bottomLeft(), "bottomLeft");
}

void TestRect::setBottomLeft_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(-1, 2) << Rect(-1, 0, 1, 2);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(0, 10) << Rect(0, 3, 6, 7);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(0, 10) << Rect(0, 3, 6, 7);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(0, 10) << Rect(0, 3, 6, 7);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(0, 10) << Rect(0, 3, 6, 7);
}

void TestRect::setBottomLeft()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, value);

    rect.setBottomLeft(value);
    QTEST(rect, "expected");
}

void TestRect::moveBottomLeft_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("value");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(1, 15) << Rect(1, 15, 0, 0);
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << QPoint(1, 15) << Rect(1, 10, 4, 5);
    QTest::addRow("QPoint(2, 3) QPoint(6, 8)") << Rect(QPoint(2, 3), QPoint(6, 8)) << QPoint(1, 15) << Rect(1, 10, 4, 5);
    QTest::addRow("QPoint(2, 3) QSize(4, 5)") << Rect(QPoint(2, 3), QSize(4, 5)) << QPoint(1, 15) << Rect(1, 10, 4, 5);
    QTest::addRow("QRect(2, 3, 4, 5)") << Rect(QRect(2, 3, 4, 5)) << QPoint(1, 15) << Rect(1, 10, 4, 5);
}

void TestRect::moveBottomLeft()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, value);

    rect.moveBottomLeft(value);
    QTEST(rect, "expected");
}

void TestRect::transposed_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << Rect();
    QTest::addRow("2,3 4x5") << Rect(2, 3, 4, 5) << Rect(2, 3, 5, 4);
}

void TestRect::transposed()
{
    QFETCH(Rect, rect);

    QTEST(rect.transposed(), "expected");
}

void TestRect::translate_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("offset");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(1, 2) << Rect(1, 2, 0, 0);
    QTest::addRow("2,3 4x5 by 1,0") << Rect(2, 3, 4, 5) << QPoint(1, 0) << Rect(3, 3, 4, 5);
    QTest::addRow("2,3 4x5 by 0,1") << Rect(2, 3, 4, 5) << QPoint(0, 1) << Rect(2, 4, 4, 5);
    QTest::addRow("2,3 4x5 by 1,1") << Rect(2, 3, 4, 5) << QPoint(1, 1) << Rect(3, 4, 4, 5);
}

void TestRect::translate()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, offset);

    {
        Rect tmp = rect;
        tmp.translate(offset);
        QTEST(tmp, "expected");
    }

    {
        Rect tmp = rect;
        tmp.translate(offset.x(), offset.y());
        QTEST(tmp, "expected");
    }
}

void TestRect::translated_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QPoint>("offset");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default") << Rect() << QPoint(1, 2) << Rect(1, 2, 0, 0);
    QTest::addRow("2,3 4x5 by 1,0") << Rect(2, 3, 4, 5) << QPoint(1, 0) << Rect(3, 3, 4, 5);
    QTest::addRow("2,3 4x5 by 0,1") << Rect(2, 3, 4, 5) << QPoint(0, 1) << Rect(2, 4, 4, 5);
    QTest::addRow("2,3 4x5 by 1,1") << Rect(2, 3, 4, 5) << QPoint(1, 1) << Rect(3, 4, 4, 5);
}

void TestRect::translated()
{
    QFETCH(Rect, rect);
    QFETCH(QPoint, offset);

    QTEST(rect.translated(offset), "expected");
    QTEST(rect.translated(offset.x(), offset.y()), "expected");
}

void TestRect::scaled_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<RectF>("expected");

    QTest::addRow("default x 1.0") << Rect() << 1.0 << RectF();
    QTest::addRow("default x 2.0") << Rect() << 2.0 << RectF();
    QTest::addRow("empty x 1.0") << Rect(0, 0, 0, 0) << 1.0 << RectF(0, 0, 0, 0);
    QTest::addRow("empty x 2.0") << Rect(0, 0, 0, 0) << 2.0 << RectF(0, 0, 0, 0);
    QTest::addRow("1,1 0,0 x 1.0") << Rect(1, 1, 0, 0) << 1.0 << RectF(1, 1, 0, 0);
    QTest::addRow("1,1 0,0 x 2.0") << Rect(1, 1, 0, 0) << 2.0 << RectF(2, 2, 0, 0);
    QTest::addRow("2,3 4,5 x 1.0") << Rect(2, 3, 4, 5) << 1.0 << RectF(2, 3, 4, 5);
    QTest::addRow("2,3 4,5 x 1.75") << Rect(2, 3, 4, 5) << 1.75 << RectF(3.5, 5.25, 7, 8.75);
    QTest::addRow("2,3 4,5 x 2.0") << Rect(2, 3, 4, 5) << 2.0 << RectF(4, 6, 8, 10);
}

void TestRect::scaled()
{
    QFETCH(Rect, rect);
    QFETCH(qreal, scale);

    QTEST(rect.scaled(scale), "expected");
}

void TestRect::adjust_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("x0");
    QTest::addColumn<int>("y0");
    QTest::addColumn<int>("x1");
    QTest::addColumn<int>("y1");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default - -1,0,0,0") << Rect() << -1 << 0 << 0 << 0 << Rect(-1, 0, 1, 0);
    QTest::addRow("default - 0,-1,0,0") << Rect() << 0 << -1 << 0 << 0 << Rect(0, -1, 0, 1);
    QTest::addRow("default - 0,0,1,0") << Rect() << 0 << 0 << 1 << 0 << Rect(0, 0, 1, 0);
    QTest::addRow("default - 0,0,0,1") << Rect() << 0 << 0 << 0 << 1 << Rect(0, 0, 0, 1);
    QTest::addRow("default - -1,0,1,0") << Rect() << -1 << 0 << 1 << 0 << Rect(-1, 0, 2, 0);
    QTest::addRow("default - 0,-1,0,1") << Rect() << 0 << -1 << 0 << 1 << Rect(0, -1, 0, 2);
    QTest::addRow("default - -1,-1,1,1") << Rect() << -1 << -1 << 1 << 1 << Rect(-1, -1, 2, 2);

    QTest::addRow("2,3 4x5 - -1,0,1,0") << Rect(2, 3, 4, 5) << -1 << 0 << 1 << 0 << Rect(1, 3, 6, 5);
    QTest::addRow("2,3 4x5 - 0,-1,0,1") << Rect(2, 3, 4, 5) << 0 << -1 << 0 << 1 << Rect(2, 2, 4, 7);
}

void TestRect::adjust()
{
    QFETCH(Rect, rect);
    QFETCH(int, x0);
    QFETCH(int, y0);
    QFETCH(int, x1);
    QFETCH(int, y1);

    rect.adjust(x0, y0, x1, y1);
    QTEST(rect, "expected");
}

void TestRect::adjusted_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<int>("x0");
    QTest::addColumn<int>("y0");
    QTest::addColumn<int>("x1");
    QTest::addColumn<int>("y1");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("default - -1,0,0,0") << Rect() << -1 << 0 << 0 << 0 << Rect(-1, 0, 1, 0);
    QTest::addRow("default - 0,-1,0,0") << Rect() << 0 << -1 << 0 << 0 << Rect(0, -1, 0, 1);
    QTest::addRow("default - 0,0,1,0") << Rect() << 0 << 0 << 1 << 0 << Rect(0, 0, 1, 0);
    QTest::addRow("default - 0,0,0,1") << Rect() << 0 << 0 << 0 << 1 << Rect(0, 0, 0, 1);
    QTest::addRow("default - -1,0,1,0") << Rect() << -1 << 0 << 1 << 0 << Rect(-1, 0, 2, 0);
    QTest::addRow("default - 0,-1,0,1") << Rect() << 0 << -1 << 0 << 1 << Rect(0, -1, 0, 2);
    QTest::addRow("default - -1,-1,1,1") << Rect() << -1 << -1 << 1 << 1 << Rect(-1, -1, 2, 2);

    QTest::addRow("2,3 4x5 - -1,0,1,0") << Rect(2, 3, 4, 5) << -1 << 0 << 1 << 0 << Rect(1, 3, 6, 5);
    QTest::addRow("2,3 4x5 - 0,-1,0,1") << Rect(2, 3, 4, 5) << 0 << -1 << 0 << 1 << Rect(2, 2, 4, 7);
}

void TestRect::adjusted()
{
    QFETCH(Rect, rect);
    QFETCH(int, x0);
    QFETCH(int, y0);
    QFETCH(int, x1);
    QFETCH(int, y1);

    QTEST(rect.adjusted(x0, y0, x1, y1), "expected");
}

void TestRect::grownBy_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QMargins>("margins");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("2,3 4x5 - 1,0,0,0") << Rect(2, 3, 4, 5) << QMargins(1, 0, 0, 0) << Rect(1, 3, 5, 5);
    QTest::addRow("2,3 4x5 - 0,1,0,0") << Rect(2, 3, 4, 5) << QMargins(0, 1, 0, 0) << Rect(2, 2, 4, 6);
    QTest::addRow("2,3 4x5 - 0,0,1,0") << Rect(2, 3, 4, 5) << QMargins(0, 0, 1, 0) << Rect(2, 3, 5, 5);
    QTest::addRow("2,3 4x5 - 0,0,0,1") << Rect(2, 3, 4, 5) << QMargins(0, 0, 0, 1) << Rect(2, 3, 4, 6);
    QTest::addRow("2,3 4x5 - 1,1,1,1") << Rect(2, 3, 4, 5) << QMargins(1, 1, 1, 1) << Rect(1, 2, 6, 7);
}

void TestRect::grownBy()
{
    QFETCH(Rect, rect);
    QFETCH(QMargins, margins);

    {
        QTEST(rect.grownBy(margins), "expected");
        QTEST(rect.marginsAdded(margins), "expected");
    }

    {
        Rect tmp = rect;
        tmp += margins;
        QTEST(tmp, "expected");
    }
}

void TestRect::shrunkBy_data()
{
    QTest::addColumn<Rect>("rect");
    QTest::addColumn<QMargins>("margins");
    QTest::addColumn<Rect>("expected");

    QTest::addRow("2,3 4x5 - 1,0,0,0") << Rect(2, 3, 4, 5) << QMargins(1, 0, 0, 0) << Rect(3, 3, 3, 5);
    QTest::addRow("2,3 4x5 - 0,1,0,0") << Rect(2, 3, 4, 5) << QMargins(0, 1, 0, 0) << Rect(2, 4, 4, 4);
    QTest::addRow("2,3 4x5 - 0,0,1,0") << Rect(2, 3, 4, 5) << QMargins(0, 0, 1, 0) << Rect(2, 3, 3, 5);
    QTest::addRow("2,3 4x5 - 0,0,0,1") << Rect(2, 3, 4, 5) << QMargins(0, 0, 0, 1) << Rect(2, 3, 4, 4);
    QTest::addRow("2,3 4x5 - 1,1,1,1") << Rect(2, 3, 4, 5) << QMargins(1, 1, 1, 1) << Rect(3, 4, 2, 3);
}

void TestRect::shrunkBy()
{
    QFETCH(Rect, rect);
    QFETCH(QMargins, margins);

    {
        QTEST(rect.shrunkBy(margins), "expected");
        QTEST(rect.marginsRemoved(margins), "expected");
    }

    {
        Rect tmp = rect;
        tmp -= margins;
        QTEST(tmp, "expected");
    }
}

void TestRect::united_data()
{
    QTest::addColumn<Rect>("rect1");
    QTest::addColumn<Rect>("rect2");
    QTest::addColumn<Rect>("result");

    QTest::addRow("empty - empty") << Rect() << Rect() << Rect();
    QTest::addRow("2,3 4x5 - empty") << Rect(2, 3, 4, 5) << Rect() << Rect(2, 3, 4, 5);
    QTest::addRow("empty - 2,3 4x5") << Rect() << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 5);
    QTest::addRow("2,3 4x5 - 2,3 4x5") << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 5) << Rect(2, 3, 4, 5);

    QTest::addRow("1,0 1x5 - 5,0 1x5") << Rect(1, 0, 1, 5) << Rect(5, 0, 1, 5) << Rect(1, 0, 5, 5);
    QTest::addRow("0,1 5x1 - 0,5 5x1") << Rect(0, 1, 5, 1) << Rect(0, 5, 5, 1) << Rect(0, 1, 5, 5);

    QTest::addRow("2,3 4x5 - 10,10 0x0") << Rect(2, 3, 4, 5) << Rect(10, 10, 0, 0) << Rect(2, 3, 4, 5);
}

void TestRect::united()
{
    QFETCH(Rect, rect1);
    QFETCH(Rect, rect2);

    {
        QTEST(rect1.united(rect2), "result");
    }

    {
        QTEST(rect1 | rect2, "result");
    }

    {
        Rect tmp = rect1;
        tmp |= rect2;
        QTEST(tmp, "result");
    }
}

void TestRect::intersects_data()
{
    QTest::addColumn<Rect>("rect1");
    QTest::addColumn<Rect>("rect2");
    QTest::addColumn<bool>("intersects");

    QTest::addRow("empty - empty") << Rect() << Rect() << false;
    QTest::addRow("2,3 4x5 - empty") << Rect(2, 3, 4, 5) << Rect() << false;

    QTest::addRow("0,0 5x5 - 0,-5 5x5") << Rect(0, 0, 5, 5) << Rect(0, -5, 5, 5) << false;
    QTest::addRow("0,0 5x5 - -5,0 5x5") << Rect(0, 0, 5, 5) << Rect(-5, 0, 5, 5) << false;
    QTest::addRow("0,0 5x5 - -5,-5 5x5") << Rect(0, 0, 5, 5) << Rect(-5, -5, 5, 5) << false;
    QTest::addRow("0,0 5x5 - 0,5 5x5") << Rect(0, 0, 5, 5) << Rect(0, 5, 5, 5) << false;
    QTest::addRow("0,0 5x5 - 5,0 5x5") << Rect(0, 0, 5, 5) << Rect(5, 0, 5, 5) << false;
    QTest::addRow("0,0 5x5 - 5,5 5x5") << Rect(0, 0, 5, 5) << Rect(5, 5, 5, 5) << false;

    QTest::addRow("0,0 5x5 - 2,2 5x5") << Rect(0, 0, 5, 5) << Rect(2, 2, 5, 5) << true;
    QTest::addRow("0,0 5x5 - 2,2 0x0") << Rect(0, 0, 5, 5) << Rect(2, 2, 0, 0) << false;
}

void TestRect::intersects()
{
    QFETCH(Rect, rect1);
    QFETCH(Rect, rect2);

    QTEST(rect1.intersects(rect2), "intersects");
    QTEST(rect2.intersects(rect1), "intersects");
}

void TestRect::intersected_data()
{
    QTest::addColumn<Rect>("rect1");
    QTest::addColumn<Rect>("rect2");
    QTest::addColumn<Rect>("result");

    QTest::addRow("empty - empty") << Rect() << Rect() << Rect();
    QTest::addRow("2,3 4x5 - empty") << Rect(2, 3, 4, 5) << Rect() << Rect();

    QTest::addRow("0,0 5x5 - 0,-5 5x5") << Rect(0, 0, 5, 5) << Rect(0, -5, 5, 5) << Rect();
    QTest::addRow("0,0 5x5 - -5,0 5x5") << Rect(0, 0, 5, 5) << Rect(-5, 0, 5, 5) << Rect();
    QTest::addRow("0,0 5x5 - -5,-5 5x5") << Rect(0, 0, 5, 5) << Rect(-5, -5, 5, 5) << Rect();
    QTest::addRow("0,0 5x5 - 0,5 5x5") << Rect(0, 0, 5, 5) << Rect(0, 5, 5, 5) << Rect();
    QTest::addRow("0,0 5x5 - 5,0 5x5") << Rect(0, 0, 5, 5) << Rect(5, 0, 5, 5) << Rect();
    QTest::addRow("0,0 5x5 - 5,5 5x5") << Rect(0, 0, 5, 5) << Rect(5, 5, 5, 5) << Rect();

    QTest::addRow("0,0 5x5 - 2,2 5x5") << Rect(0, 0, 5, 5) << Rect(2, 2, 5, 5) << Rect(2, 2, 3, 3);
    QTest::addRow("0,0 5x5 - 2,2 0x0") << Rect(0, 0, 5, 5) << Rect(2, 2, 0, 0) << Rect();
}

void TestRect::intersected()
{
    QFETCH(Rect, rect1);
    QFETCH(Rect, rect2);

    {
        QTEST(rect1.intersected(rect2), "result");
        QTEST(rect2.intersected(rect1), "result");
    }

    {
        QTEST(rect1 & rect2, "result");
        QTEST(rect2 & rect1, "result");
    }

    {
        Rect tmp = rect1;
        tmp &= rect2;
        QTEST(tmp, "result");
    }

    {
        Rect tmp = rect2;
        tmp &= rect1;
        QTEST(tmp, "result");
    }
}

void TestRect::implicitConversions()
{
    const auto qrectFunction = [](const QRect &rect) { };
    qrectFunction(Rect());

    const auto rectFunction = [](const Rect &rect) { };
    rectFunction(QRect());

    {
        const Rect rect;
        const QRect qrect = rect;
        QCOMPARE(qrect, QRect());
        QVERIFY(qrect == rect);
        QVERIFY(rect == qrect);
    }

    {
        const Rect rect(1, 2, 3, 4);
        const QRect qrect = rect;
        QCOMPARE(qrect, QRect(1, 2, 3, 4));
        QVERIFY(qrect == rect);
        QVERIFY(rect == qrect);
    }

    {
        Rect rect(0, 0, 1, 1);
        rect |= QRect(1, 1, 1, 1);
        QCOMPARE(rect, Rect(0, 0, 2, 2));
    }

    {
        QRect qrect(0, 0, 1, 1);
        qrect |= Rect(1, 1, 1, 1);
        QCOMPARE(qrect, QRect(0, 0, 2, 2));
    }

    {
        Rect rect(0, 0, 3, 3);
        rect &= QRect(1, 1, 3, 3);
        QCOMPARE(rect, Rect(1, 1, 2, 2));
    }

    {
        QRect qrect(0, 0, 3, 3);
        qrect &= Rect(1, 1, 3, 3);
        QCOMPARE(qrect, QRect(1, 1, 2, 2));
    }
}

QTEST_MAIN(TestRect)

#include "test_rect.moc"
