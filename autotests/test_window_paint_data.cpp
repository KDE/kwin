/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/effecthandler.h"

#include "virtualdesktops.h"

#include <QGraphicsRotation>
#include <QGraphicsScale>
#include <QVector2D>

#include <QTest>

using namespace KWin;

class TestWindowPaintData : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCtor();
    void testCopyCtor();
    void testMultiplyOpacity();
    void testMultiplySaturation();
    void testMultiplyBrightness();
};

void TestWindowPaintData::testCtor()
{
    WindowPaintData data;
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
    QCOMPARE(data2.rotationAngle(), 0.0);
    QCOMPARE(data2.rotationOrigin(), QVector3D());
    QCOMPARE(data2.rotationAxis(), QVector3D(0.0, 0.0, 1.0));
    QCOMPARE(data2.opacity(), 1.0);
    QCOMPARE(data2.brightness(), 1.0);
    QCOMPARE(data2.saturation(), 1.0);

    data2.setRotationAngle(45.0);
    data2.setRotationOrigin(QVector3D(1.0, 2.0, 3.0));
    data2.setRotationAxis(QVector3D(1.0, 1.0, 0.0));
    data2.setOpacity(0.1);
    data2.setBrightness(0.3);
    data2.setSaturation(0.4);

    WindowPaintData data3(data2);
    QCOMPARE(data3.rotationAngle(), 45.0);
    QCOMPARE(data3.rotationOrigin(), QVector3D(1.0, 2.0, 3.0));
    QCOMPARE(data3.rotationAxis(), QVector3D(1.0, 1.0, 0.0));
    QCOMPARE(data3.opacity(), 0.1);
    QCOMPARE(data3.brightness(), 0.3);
    QCOMPARE(data3.saturation(), 0.4);
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
