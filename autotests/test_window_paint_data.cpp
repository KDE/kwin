/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <kwineffects.h>
#include "../virtualdesktops.h"

#include <QVector2D>
#include <QGraphicsRotation>
#include <QGraphicsScale>

#include <QtTest>

using namespace KWin;

class MockEffectWindow : public EffectWindow
{
    Q_OBJECT
public:
    MockEffectWindow(QObject *parent = nullptr);
    WindowQuadList buildQuads(bool force = false) const override;
    QVariant data(int role) const override;
    QRect decorationInnerRect() const override;
    void deleteProperty(long int atom) const override;
    void disablePainting(int reason) override;
    void enablePainting(int reason) override;
    void addRepaint(const QRect &r) override;
    void addRepaint(int x, int y, int w, int h) override;
    void addRepaintFull() override;
    void addLayerRepaint(const QRect &r) override;
    void addLayerRepaint(int x, int y, int w, int h) override;
    EffectWindow *findModal() override;
    EffectWindow *transientFor() override;
    const EffectWindowGroup *group() const override;
    bool isPaintingEnabled() override;
    EffectWindowList mainWindows() const override;
    QByteArray readProperty(long int atom, long int type, int format) const override;
    void refWindow() override;
    void unrefWindow() override;
    QRegion shape() const override;
    void setData(int role, const QVariant &data) override;
    void minimize() override;
    void unminimize() override;
    void closeWindow() override;
    void referencePreviousWindowPixmap() override {}
    void unreferencePreviousWindowPixmap() override {}
    QWindow *internalWindow() const override {
        return nullptr;
    }
    bool isDeleted() const override {
        return false;
    }
    bool isMinimized() const override {
        return false;
    }
    double opacity() const override {
        return m_opacity;
    }
    void setOpacity(qreal opacity) {
        m_opacity = opacity;
    }
    bool hasAlpha() const override {
        return true;
    }
    QStringList activities() const override {
        return QStringList();
    }
    int desktop() const override {
        return 0;
    }
    QVector<uint> desktops() const override {
        return {};
    }
    int x() const override {
        return 0;
    }
    int y() const override {
        return 0;
    }
    int width() const override {
        return 100;
    }
    int height() const override {
        return 100;
    }
    QSize basicUnit() const override {
        return QSize();
    }
    QRect geometry() const override {
        return QRect();
    }
    QRect expandedGeometry() const override {
        return QRect();
    }
    QRect frameGeometry() const override {
        return QRect();
    }
    QRect bufferGeometry() const override {
        return QRect();
    }
    int screen() const override {
        return 0;
    }
    bool hasOwnShape() const override {
        return false;
    }
    QPoint pos() const override {
        return QPoint();
    }
    QSize size() const override {
        return QSize(100,100);
    }
    QRect rect() const override {
        return QRect(0,0,100,100);
    }
    bool isMovable() const override {
        return true;
    }
    bool isMovableAcrossScreens() const override {
        return true;
    }
    bool isUserMove() const override {
        return false;
    }
    bool isUserResize() const override {
        return false;
    }
    QRect iconGeometry() const override {
        return QRect();
    }
    bool isDesktop() const override {
        return false;
    }
    bool isDock() const override {
        return false;
    }
    bool isToolbar() const override {
        return false;
    }
    bool isMenu() const override {
        return false;
    }
    bool isNormalWindow() const override {
        return true;
    }
    bool isSpecialWindow() const override {
        return false;
    }
    bool isDialog() const override {
        return false;
    }
    bool isSplash() const override {
        return false;
    }
    bool isUtility() const override {
        return false;
    }
    bool isDropdownMenu() const override {
        return false;
    }
    bool isPopupMenu() const override {
        return false;
    }
    bool isTooltip() const override {
        return false;
    }
    bool isNotification() const override {
        return false;
    }
    bool isCriticalNotification() const override {
        return false;
    }
    bool isOnScreenDisplay() const override  {
        return false;
    }
    bool isComboBox() const override {
        return false;
    }
    bool isDNDIcon() const override {
        return false;
    }
    QRect contentsRect() const override {
        return QRect();
    }
    bool decorationHasAlpha() const override {
        return false;
    }
    QString caption() const override {
        return QString();
    }
    QIcon icon() const override {
        return QIcon();
    }
    QString windowClass() const override {
        return QString();
    }
    QString windowRole() const override {
        return QString();
    }
    NET::WindowType windowType() const override {
        return NET::Normal;
    }
    bool acceptsFocus() const override {
        return true;
    }
    bool keepAbove() const override {
        return false;
    }
    bool keepBelow() const override {
        return false;
    }
    bool isModal() const override {
        return false;
    }
    bool isSkipSwitcher() const override {
        return false;
    }
    bool isCurrentTab() const override {
        return true;
    }
    bool skipsCloseAnimation() const override {
        return false;
    }
    KWaylandServer::SurfaceInterface *surface() const override {
        return nullptr;
    }
    bool isFullScreen() const override {
        return false;
    }
    bool isUnresponsive() const override {
        return false;
    }
    bool isPopupWindow() const override {
        return false;
    }
    bool isManaged() const override {
        return true;
    }
    bool isWaylandClient() const override {
        return true;
    }
    bool isX11Client() const override {
        return false;
    }
    bool isOutline() const override {
        return false;
    }
    pid_t pid() const override {
        return 0;
    }

private:
    qreal m_opacity = 1.0;
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

void MockEffectWindow::addRepaint(const QRect &r)
{
    Q_UNUSED(r)
}

void MockEffectWindow::addRepaint(int x, int y, int w, int h)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(w)
    Q_UNUSED(h)
}

void MockEffectWindow::addRepaintFull()
{
}

void MockEffectWindow::addLayerRepaint(const QRect &r)
{
    Q_UNUSED(r)
}

void MockEffectWindow::addLayerRepaint(int x, int y, int w, int h)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(w)
    Q_UNUSED(h)
}

EffectWindow *MockEffectWindow::findModal()
{
    return nullptr;
}

EffectWindow *MockEffectWindow::transientFor()
{
    return nullptr;
}

const EffectWindowGroup *MockEffectWindow::group() const
{
    return nullptr;
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

void MockEffectWindow::minimize()
{
}

void MockEffectWindow::unminimize()
{
}

void MockEffectWindow::closeWindow()
{
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
    MockEffectWindow w;
    w.setOpacity(0.5);
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
    QCOMPARE(data.brightness(), 1.0);
    QCOMPARE(data.saturation(), 1.0);
}

void TestWindowPaintData::testCopyCtor()
{
    MockEffectWindow w;
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
    MockEffectWindow w;
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
    MockEffectWindow w;
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
    MockEffectWindow w;
    WindowPaintData data(&w);
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
    MockEffectWindow w;
    WindowPaintData data(&w);
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
    MockEffectWindow w;
    WindowPaintData data(&w);
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
