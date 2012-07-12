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

class MockEffectWindowHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
public:
    MockEffectWindowHelper(QObject *parent = NULL);
    double opacity() const {
        return m_opacity;
    }
    void setOpacity(qreal opacity) {
        m_opacity = opacity;
    }
private:
    qreal m_opacity;
};

MockEffectWindowHelper::MockEffectWindowHelper(QObject *parent)
    : QObject(parent)
    , m_opacity(1.0)
{
}

class MockEffectWindow : public EffectWindow
{
    Q_OBJECT
public:
    MockEffectWindow(QObject *parent = 0);
    virtual WindowQuadList buildQuads(bool force = false) const;
    virtual QVariant data(int role) const;
    virtual QRect decorationInnerRect() const;
    virtual void deleteProperty(long int atom) const;
    virtual void disablePainting(int reason);
    virtual void enablePainting(int reason);
    virtual EffectWindow *findModal();
    virtual const EffectWindowGroup *group() const;
    virtual bool isPaintingEnabled();
    virtual EffectWindowList mainWindows() const;
    virtual QByteArray readProperty(long int atom, long int type, int format) const;
    virtual void refWindow();
    virtual void unrefWindow();
    virtual QRegion shape() const;
    virtual void setData(int role, const QVariant &data);
};

MockEffectWindow::MockEffectWindow(QObject *parent)
    : EffectWindow(parent)
{
}

WindowQuadList MockEffectWindow::buildQuads(bool force) const
{
    Q_UNUSED(force)
    return WindowQuadList();
}

QVariant MockEffectWindow::data(int role) const
{
    Q_UNUSED(role)
    return QVariant();
}

QRect MockEffectWindow::decorationInnerRect() const
{
    return QRect();
}

void MockEffectWindow::deleteProperty(long int atom) const
{
    Q_UNUSED(atom)
}

void MockEffectWindow::disablePainting(int reason)
{
    Q_UNUSED(reason)
}

void MockEffectWindow::enablePainting(int reason)
{
    Q_UNUSED(reason)
}

EffectWindow *MockEffectWindow::findModal()
{
    return NULL;
}

const EffectWindowGroup *MockEffectWindow::group() const
{
    return NULL;
}

bool MockEffectWindow::isPaintingEnabled()
{
    return true;
}

EffectWindowList MockEffectWindow::mainWindows() const
{
    return EffectWindowList();
}

QByteArray MockEffectWindow::readProperty(long int atom, long int type, int format) const
{
    Q_UNUSED(atom)
    Q_UNUSED(type)
    Q_UNUSED(format)
    return QByteArray();
}

void MockEffectWindow::refWindow()
{
}

void MockEffectWindow::setData(int role, const QVariant &data)
{
    Q_UNUSED(role)
    Q_UNUSED(data)
}

QRegion MockEffectWindow::shape() const
{
    return QRegion();
}

void MockEffectWindow::unrefWindow()
{
}

class TestWindowPaintData : public QObject
{
    Q_OBJECT
private slots:
    void testCtor();
    void testCopyCtor();
    void testOperatorMultiplyAssign();
    void testOperatorPlus();
    void testMultiplyOpacity();
    void testMultiplyDecorationOpacity();
    void testMultiplySaturation();
    void testMultiplyBrightness();
};

void TestWindowPaintData::testCtor()
{
    MockEffectWindowHelper helper;
    helper.setOpacity(0.5);
    MockEffectWindow w(&helper);
    WindowPaintData data(&w);
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
    QCOMPARE(data.opacity(), 0.5);
    QCOMPARE(data.decorationOpacity(), 1.0);
    QCOMPARE(data.brightness(), 1.0);
    QCOMPARE(data.saturation(), 1.0);
}

void TestWindowPaintData::testCopyCtor()
{
    MockEffectWindowHelper helper;
    MockEffectWindow w(&helper);
    WindowPaintData data(&w);
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
    QCOMPARE(data2.decorationOpacity(), 1.0);
    QCOMPARE(data2.brightness(), 1.0);
    QCOMPARE(data2.saturation(), 1.0);

    data2.setScale(QVector3D(0.5, 2.0, 3.0));
    data2.translate(0.5, 2.0, 3.0);
    data2.setRotationAngle(45.0);
    data2.setRotationOrigin(QVector3D(1.0, 2.0, 3.0));
    data2.setRotationAxis(QVector3D(1.0, 1.0, 0.0));
    data2.setOpacity(0.1);
    data2.setDecorationOpacity(0.2);
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
    QCOMPARE(data3.decorationOpacity(), 0.2);
    QCOMPARE(data3.brightness(), 0.3);
    QCOMPARE(data3.saturation(), 0.4);
}

void TestWindowPaintData::testOperatorMultiplyAssign()
{
    MockEffectWindowHelper helper;
    MockEffectWindow w(&helper);
    WindowPaintData data(&w);
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
    MockEffectWindowHelper helper;
    MockEffectWindow w(&helper);
    WindowPaintData data(&w);
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
    MockEffectWindowHelper helper;
    MockEffectWindow w(&helper);
    WindowPaintData data(&w);
    QCOMPARE(0.2, data.multiplyBrightness(0.2));
    QCOMPARE(0.2, data.brightness());
    QCOMPARE(0.6, data.multiplyBrightness(3.0));
    QCOMPARE(0.6, data.brightness());
    // just for safety
    QCOMPARE(1.0, data.opacity());
    QCOMPARE(1.0, data.decorationOpacity());
    QCOMPARE(1.0, data.saturation());
}

void TestWindowPaintData::testMultiplyDecorationOpacity()
{
    MockEffectWindowHelper helper;
    MockEffectWindow w(&helper);
    WindowPaintData data(&w);
    QCOMPARE(0.2, data.multiplyDecorationOpacity(0.2));
    QCOMPARE(0.2, data.decorationOpacity());
    QCOMPARE(0.6, data.multiplyDecorationOpacity(3.0));
    QCOMPARE(0.6, data.decorationOpacity());
    // just for safety
    QCOMPARE(1.0, data.brightness());
    QCOMPARE(1.0, data.opacity());
    QCOMPARE(1.0, data.saturation());
}

void TestWindowPaintData::testMultiplyOpacity()
{
    MockEffectWindowHelper helper;
    MockEffectWindow w(&helper);
    WindowPaintData data(&w);
    QCOMPARE(0.2, data.multiplyOpacity(0.2));
    QCOMPARE(0.2, data.opacity());
    QCOMPARE(0.6, data.multiplyOpacity(3.0));
    QCOMPARE(0.6, data.opacity());
    // just for safety
    QCOMPARE(1.0, data.brightness());
    QCOMPARE(1.0, data.decorationOpacity());
    QCOMPARE(1.0, data.saturation());
}

void TestWindowPaintData::testMultiplySaturation()
{
    MockEffectWindowHelper helper;
    MockEffectWindow w(&helper);
    WindowPaintData data(&w);
    QCOMPARE(0.2, data.multiplySaturation(0.2));
    QCOMPARE(0.2, data.saturation());
    QCOMPARE(0.6, data.multiplySaturation(3.0));
    QCOMPARE(0.6, data.saturation());
    // just for safety
    QCOMPARE(1.0, data.brightness());
    QCOMPARE(1.0, data.opacity());
    QCOMPARE(1.0, data.decorationOpacity());
}

QTEST_MAIN(TestWindowPaintData)
#include "test_window_paint_data.moc"
