/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include <kwineffects.h>

#include <QtGui/QVector2D>

#include <QtTest/QtTest>

using namespace KWin;

class TestScreenPaintData : public QObject
{
    Q_OBJECT
private slots:

    void testCtor();
    void testCopyCtor();
    void testAssignmentOperator();
    void testSetScale();
    void testOperatorMultiplyAssign();
    void testSetTranslate();
    void testOperatorPlus();
    void testSetAngle();
    void testSetRotationOrigin();
    void testSetRotationAxis();
};

void TestScreenPaintData::testCtor()
{
    ScreenPaintData data;
    QCOMPARE(data.xScale(), 1.0);
    QCOMPARE(data.yScale(), 1.0);
    QCOMPARE(data.zScale(), 1.0);
    QCOMPARE(data.xTranslation(), 0.0);
    QCOMPARE(data.yTranslation(), 0.0);
    QCOMPARE(data.zTranslation(), 0.0);
    QCOMPARE(data.translation(), QVector3D());
    QCOMPARE(data.rotationAngle(), 0.0);
    QCOMPARE(data.rotationOrigin(), QVector3D());
    QCOMPARE(data.rotationAxis(), QVector3D(0.0, 0.0, 1.0));
}

void TestScreenPaintData::testCopyCtor()
{
    ScreenPaintData data;
    ScreenPaintData data2(data);
    // no value had been changed
    QCOMPARE(data2.xScale(), 1.0);
    QCOMPARE(data2.yScale(), 1.0);
    QCOMPARE(data2.zScale(), 1.0);
    QCOMPARE(data2.xTranslation(), 0.0);
    QCOMPARE(data2.yTranslation(), 0.0);
    QCOMPARE(data2.zTranslation(), 0.0);
    QCOMPARE(data2.translation(), QVector3D());
    QCOMPARE(data2.rotationAngle(), 0.0);
    QCOMPARE(data2.rotationOrigin(), QVector3D());
    QCOMPARE(data2.rotationAxis(), QVector3D(0.0, 0.0, 1.0));

    data2.setScale(QVector3D(0.5, 2.0, 3.0));
    data2.translate(0.5, 2.0, 3.0);
    data2.setRotationAngle(45.0);
    data2.setRotationOrigin(QVector3D(1.0, 2.0, 3.0));
    data2.setRotationAxis(QVector3D(1.0, 1.0, 0.0));

    ScreenPaintData data3(data2);
    QCOMPARE(data3.xScale(), 0.5);
    QCOMPARE(data3.yScale(), 2.0);
    QCOMPARE(data3.zScale(), 3.0);
    QCOMPARE(data3.xTranslation(), 0.5);
    QCOMPARE(data3.yTranslation(), 2.0);
    QCOMPARE(data3.zTranslation(), 3.0);
    QCOMPARE(data3.translation(), QVector3D(0.5, 2.0, 3.0));
    QCOMPARE(data3.rotationAngle(), 45.0);
    QCOMPARE(data3.rotationOrigin(), QVector3D(1.0, 2.0, 3.0));
    QCOMPARE(data3.rotationAxis(), QVector3D(1.0, 1.0, 0.0));
}

void TestScreenPaintData::testAssignmentOperator()
{
    ScreenPaintData data;
    ScreenPaintData data2;

    data2.setScale(QVector3D(0.5, 2.0, 3.0));
    data2.translate(0.5, 2.0, 3.0);
    data2.setRotationAngle(45.0);
    data2.setRotationOrigin(QVector3D(1.0, 2.0, 3.0));
    data2.setRotationAxis(QVector3D(1.0, 1.0, 0.0));

    data = data2;
    // data and data2 should be the same
    QCOMPARE(data.xScale(), 0.5);
    QCOMPARE(data.yScale(), 2.0);
    QCOMPARE(data.zScale(), 3.0);
    QCOMPARE(data.xTranslation(), 0.5);
    QCOMPARE(data.yTranslation(), 2.0);
    QCOMPARE(data.zTranslation(), 3.0);
    QCOMPARE(data.translation(), QVector3D(0.5, 2.0, 3.0));
    QCOMPARE(data.rotationAngle(), 45.0);
    QCOMPARE(data.rotationOrigin(), QVector3D(1.0, 2.0, 3.0));
    QCOMPARE(data.rotationAxis(), QVector3D(1.0, 1.0, 0.0));
    // data 2
    QCOMPARE(data2.xScale(), 0.5);
    QCOMPARE(data2.yScale(), 2.0);
    QCOMPARE(data2.zScale(), 3.0);
    QCOMPARE(data2.xTranslation(), 0.5);
    QCOMPARE(data2.yTranslation(), 2.0);
    QCOMPARE(data2.zTranslation(), 3.0);
    QCOMPARE(data2.translation(), QVector3D(0.5, 2.0, 3.0));
    QCOMPARE(data2.rotationAngle(), 45.0);
    QCOMPARE(data2.rotationOrigin(), QVector3D(1.0, 2.0, 3.0));
    QCOMPARE(data2.rotationAxis(), QVector3D(1.0, 1.0, 0.0));
}

void TestScreenPaintData::testSetScale()
{
    ScreenPaintData data;
    // without anything set, it's 1.0 on all axis
    QCOMPARE(data.xScale(), 1.0);
    QCOMPARE(data.yScale(), 1.0);
    QCOMPARE(data.zScale(), 1.0);
    // changing xScale should not affect y and z
    data.setXScale(2.0);
    QCOMPARE(data.xScale(), 2.0);
    QCOMPARE(data.yScale(), 1.0);
    QCOMPARE(data.zScale(), 1.0);
    // changing yScale should not affect x and z
    data.setYScale(3.0);
    QCOMPARE(data.xScale(), 2.0);
    QCOMPARE(data.yScale(), 3.0);
    QCOMPARE(data.zScale(), 1.0);
    // changing zScale should not affect x and y
    data.setZScale(4.0);
    QCOMPARE(data.xScale(), 2.0);
    QCOMPARE(data.yScale(), 3.0);
    QCOMPARE(data.zScale(), 4.0);
    // setting a vector2d should affect x and y components
    data.setScale(QVector2D(0.5, 2.0));
    QCOMPARE(data.xScale(), 0.5);
    QCOMPARE(data.yScale(), 2.0);
    QCOMPARE(data.zScale(), 4.0);
    // setting a vector3d should affect all components
    data.setScale(QVector3D(1.5, 2.5, 3.5));
    QCOMPARE(data.xScale(), 1.5);
    QCOMPARE(data.yScale(), 2.5);
    QCOMPARE(data.zScale(), 3.5);
}

void TestScreenPaintData::testOperatorMultiplyAssign()
{
    ScreenPaintData data;
    // without anything set, it's 1.0 on all axis
    QCOMPARE(data.xScale(), 1.0);
    QCOMPARE(data.yScale(), 1.0);
    QCOMPARE(data.zScale(), 1.0);
    // multiplying by a factor should set all components
    data *= 2.0;
    QCOMPARE(data.xScale(), 2.0);
    QCOMPARE(data.yScale(), 2.0);
    QCOMPARE(data.zScale(), 2.0);
    // multiplying by a vector2D should set x and y components
    data *= QVector2D(2.0, 3.0);
    QCOMPARE(data.xScale(), 4.0);
    QCOMPARE(data.yScale(), 6.0);
    QCOMPARE(data.zScale(), 2.0);
    // multiplying by a vector3d should set all components
    data *= QVector3D(0.5, 1.5, 2.0);
    QCOMPARE(data.xScale(), 2.0);
    QCOMPARE(data.yScale(), 9.0);
    QCOMPARE(data.zScale(), 4.0);
}

void TestScreenPaintData::testSetTranslate()
{
    ScreenPaintData data;
    QCOMPARE(data.xTranslation(), 0.0);
    QCOMPARE(data.yTranslation(), 0.0);
    QCOMPARE(data.zTranslation(), 0.0);
    QCOMPARE(data.translation(), QVector3D());
    // set x translate, should not affect y and z
    data.setXTranslation(1.0);
    QCOMPARE(data.xTranslation(), 1.0);
    QCOMPARE(data.yTranslation(), 0.0);
    QCOMPARE(data.zTranslation(), 0.0);
    QCOMPARE(data.translation(), QVector3D(1.0, 0.0, 0.0));
    // set y translate, should not affect x and z
    data.setYTranslation(2.0);
    QCOMPARE(data.xTranslation(), 1.0);
    QCOMPARE(data.yTranslation(), 2.0);
    QCOMPARE(data.zTranslation(), 0.0);
    QCOMPARE(data.translation(), QVector3D(1.0, 2.0, 0.0));
    // set z translate, should not affect x and y
    data.setZTranslation(3.0);
    QCOMPARE(data.xTranslation(), 1.0);
    QCOMPARE(data.yTranslation(), 2.0);
    QCOMPARE(data.zTranslation(), 3.0);
    QCOMPARE(data.translation(), QVector3D(1.0, 2.0, 3.0));
    // translate in x
    data.translate(0.5);
    QCOMPARE(data.translation(), QVector3D(1.5, 2.0, 3.0));
    // translate in x and y
    data.translate(0.5, 0.75);
    QCOMPARE(data.translation(), QVector3D(2.0, 2.75, 3.0));
    // translate in x, y and z
    data.translate(1.0, 2.0, 3.0);
    QCOMPARE(data.translation(), QVector3D(3.0, 4.75, 6.0));
    // translate using vector
    data.translate(QVector3D(2.0, 1.0, 0.5));
    QCOMPARE(data.translation(), QVector3D(5.0, 5.75, 6.5));
}

void TestScreenPaintData::testOperatorPlus()
{
    ScreenPaintData data;
    QCOMPARE(data.xTranslation(), 0.0);
    QCOMPARE(data.yTranslation(), 0.0);
    QCOMPARE(data.zTranslation(), 0.0);
    QCOMPARE(data.translation(), QVector3D());
    // test with point
    data += QPoint(1, 2);
    QCOMPARE(data.translation(), QVector3D(1.0, 2.0, 0.0));
    // test with pointf
    data += QPointF(0.5, 0.75);
    QCOMPARE(data.translation(), QVector3D(1.5, 2.75, 0.0));
    // test with QVector2D
    data += QVector2D(0.25, 1.5);
    QCOMPARE(data.translation(), QVector3D(1.75, 4.25, 0.0));
    // test with QVector3D
    data += QVector3D(1.0, 2.0, 3.5);
    QCOMPARE(data.translation(), QVector3D(2.75, 6.25, 3.5));
}

void TestScreenPaintData::testSetAngle()
{
    ScreenPaintData data;
    QCOMPARE(data.rotationAngle(), 0.0);
    data.setRotationAngle(20.0);
    QCOMPARE(data.rotationAngle(), 20.0);
}

void TestScreenPaintData::testSetRotationOrigin()
{
    ScreenPaintData data;
    QCOMPARE(data.rotationOrigin(), QVector3D());
    data.setRotationOrigin(QVector3D(1.0, 2.0, 3.0));
    QCOMPARE(data.rotationOrigin(), QVector3D(1.0, 2.0, 3.0));
}

void TestScreenPaintData::testSetRotationAxis()
{
    ScreenPaintData data;
    QCOMPARE(data.rotationAxis(), QVector3D(0.0, 0.0, 1.0));
    data.setRotationAxis(Qt::XAxis);
    QCOMPARE(data.rotationAxis(), QVector3D(1.0, 0.0, 0.0));
    data.setRotationAxis(Qt::YAxis);
    QCOMPARE(data.rotationAxis(), QVector3D(0.0, 1.0, 0.0));
    data.setRotationAxis(Qt::ZAxis);
    QCOMPARE(data.rotationAxis(), QVector3D(0.0, 0.0, 1.0));
    data.setRotationAxis(QVector3D(1.0, 1.0, 0.0));
    QCOMPARE(data.rotationAxis(), QVector3D(1.0, 1.0, 0.0));
}

QTEST_MAIN(TestScreenPaintData)
#include "test_screen_paint_data.moc"
