/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <kwineffects.h>

#include "virtualdesktops.h"

#include <QGraphicsRotation>
#include <QGraphicsScale>
#include <QVector2D>

#include <QtTest>

using namespace KWin;

class TestWindowPaintData : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCtor();
    void testCopyCtor();
    void testOperatorMultiplyAssign();
    void testOperatorPlus();
    void testMultiplyOpacity();
    void testMultiplySaturation();
    void testMultiplyBrightness();
};

void TestWindowPaintData::testCtor()
{
    WindowPaintData data;
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
    QCOMPARE(data.opacity(), 1.0);
    QCOMPARE(data.brightness(), 1.0);
    QCOMPARE(data.saturation(), 1.0);
}

void TestWindowPaintData::testCopyCtor()
{
    WindowPaintData data;
    WindowPaintData data2(data);
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
    QCOMPARE(data2.opacity(), 1.0);
    QCOMPARE(data2.brightness(), 1.0);
    QCOMPARE(data2.saturation(), 1.0);

    data2.setScale(QVector3D(0.5, 2.0, 3.0));
    data2.translate(0.5, 2.0, 3.0);
    data2.setRotationAngle(45.0);
    data2.setRotationOrigin(QVector3D(1.0, 2.0, 3.0));
    data2.setRotationAxis(QVector3D(1.0, 1.0, 0.0));
    data2.setOpacity(0.1);
    data2.setBrightness(0.3);
    data2.setSaturation(0.4);

    WindowPaintData data3(data2);
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
    QCOMPARE(data3.opacity(), 0.1);
    QCOMPARE(data3.brightness(), 0.3);
    QCOMPARE(data3.saturation(), 0.4);
}

void TestWindowPaintData::testOperatorMultiplyAssign()
{
    WindowPaintData data;
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

void TestWindowPaintData::testOperatorPlus()
{
    WindowPaintData data;
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

void TestWindowPaintData::testMultiplyBrightness()
{
    WindowPaintData data;
    QCOMPARE(0.2, data.multiplyBrightness(0.2));
    QCOMPARE(0.2, data.brightness());
    QCOMPARE(0.6, data.multiplyBrightness(3.0));
    QCOMPARE(0.6, data.brightness());
    // just for safety
    QCOMPARE(1.0, data.opacity());
    QCOMPARE(1.0, data.saturation());
}

void TestWindowPaintData::testMultiplyOpacity()
{
    WindowPaintData data;
    QCOMPARE(0.2, data.multiplyOpacity(0.2));
    QCOMPARE(0.2, data.opacity());
    QCOMPARE(0.6, data.multiplyOpacity(3.0));
    QCOMPARE(0.6, data.opacity());
    // just for safety
    QCOMPARE(1.0, data.brightness());
    QCOMPARE(1.0, data.saturation());
}

void TestWindowPaintData::testMultiplySaturation()
{
    WindowPaintData data;
    QCOMPARE(0.2, data.multiplySaturation(0.2));
    QCOMPARE(0.2, data.saturation());
    QCOMPARE(0.6, data.multiplySaturation(3.0));
    QCOMPARE(0.6, data.saturation());
    // just for safety
    QCOMPARE(1.0, data.brightness());
    QCOMPARE(1.0, data.opacity());
}

QTEST_MAIN(TestWindowPaintData)
#include "test_window_paint_data.moc"
