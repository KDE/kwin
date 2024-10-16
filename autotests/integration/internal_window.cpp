/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "effect/effecthandler.h"
#include "internalwindow.h"
#include "pointer_input.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QPainter>
#include <QRasterWindow>
#include <QSignalSpy>

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>
#include <KWindowSystem>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_internal_window-0");

class InternalWindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testEnterLeave();
    void testPointerPressRelease();
    void testPointerAxis();
    void testTouch();
    void testOpacity();
    void testMove();
    void testSkipCloseAnimation_data();
    void testSkipCloseAnimation();
    void testModifierClickUnrestrictedMove();
    void testModifierScroll();
    void testPopup();
    void testScale();
    void testEffectWindow();
    void testReentrantMoveResize();
    void testDismissPopup();
};

class HelperWindow : public QRasterWindow
{
    Q_OBJECT
public:
    HelperWindow();
    ~HelperWindow() override;

    QPoint latestGlobalMousePos() const
    {
        return m_latestGlobalMousePos;
    }
    Qt::MouseButtons pressedButtons() const
    {
        return m_pressedButtons;
    }

Q_SIGNALS:
    void entered();
    void left();
    void mouseMoved(const QPoint &global);
    void mousePressed();
    void mouseReleased();
    void wheel();
    void keyPressed();
    void keyReleased();
    void touchDown(int id, const QPointF &pos);
    void touchUp(int id);
    void touchMotion(int id, const QPointF &pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void touchEvent(QTouchEvent *event) override;

private:
    QPoint m_latestGlobalMousePos;
    Qt::MouseButtons m_pressedButtons = Qt::MouseButtons();
};

HelperWindow::HelperWindow()
    : QRasterWindow(nullptr)
{
    setFlags(Qt::FramelessWindowHint);
}

HelperWindow::~HelperWindow() = default;

void HelperWindow::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.fillRect(0, 0, width(), height(), Qt::red);
}

bool HelperWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        Q_EMIT entered();
    }
    if (event->type() == QEvent::Leave) {
        Q_EMIT left();
    }
    return QRasterWindow::event(event);
}

void HelperWindow::mouseMoveEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPosition().toPoint();
    Q_EMIT mouseMoved(m_latestGlobalMousePos);
}

void HelperWindow::mousePressEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPosition().toPoint();
    m_pressedButtons = event->buttons();
    Q_EMIT mousePressed();
}

void HelperWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPosition().toPoint();
    m_pressedButtons = event->buttons();
    Q_EMIT mouseReleased();
}

void HelperWindow::wheelEvent(QWheelEvent *event)
{
    Q_EMIT wheel();
}

void HelperWindow::keyPressEvent(QKeyEvent *event)
{
    Q_EMIT keyPressed();
}

void HelperWindow::keyReleaseEvent(QKeyEvent *event)
{
    Q_EMIT keyReleased();
}

void HelperWindow::touchEvent(QTouchEvent *event)
{
    for (int i = 0; i < event->pointCount(); ++i) {
        QEventPoint &point = event->point(i);

        switch (point.state()) {
        case QEventPoint::Unknown:
        case QEventPoint::Stationary:
            break;
        case QEventPoint::Pressed:
            Q_EMIT touchDown(point.id(), point.position());
            break;
        case QEventPoint::Updated:
            Q_EMIT touchMotion(point.id(), point.position());
            break;
        case QEventPoint::Released:
            Q_EMIT touchUp(point.id());
            break;
        }
    }
}

void InternalWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::InternalWindow *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void InternalWindowTest::init()
{
    input()->pointer()->warp(QPoint(512, 512));
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void InternalWindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void InternalWindowTest::testEnterLeave()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    QVERIFY(!workspace()->findInternal(nullptr));
    QVERIFY(!workspace()->findInternal(&win));
    win.setGeometry(0, 0, 100, 100);
    win.show();

    QTRY_COMPARE(windowAddedSpy.count(), 1);
    QVERIFY(!workspace()->activeWindow());
    InternalWindow *window = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(window);
    QVERIFY(window->isInternal());
    QVERIFY(!window->isDecorated());
    QCOMPARE(workspace()->findInternal(&win), window);
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 100, 100));
    QVERIFY(window->isShown());
    QVERIFY(workspace()->stackingOrder().contains(window));

    QSignalSpy enterSpy(&win, &HelperWindow::entered);
    QSignalSpy leaveSpy(&win, &HelperWindow::left);
    QSignalSpy moveSpy(&win, &HelperWindow::mouseMoved);

    quint32 timestamp = 1;
    Test::pointerMotion(QPoint(50, 50), timestamp++);
    QTRY_COMPARE(moveSpy.count(), 1);

    Test::pointerMotion(QPoint(60, 50), timestamp++);
    QTRY_COMPARE(moveSpy.count(), 2);
    QCOMPARE(moveSpy[1].first().toPoint(), QPoint(60, 50));

    Test::pointerMotion(QPoint(101, 50), timestamp++);
    QTRY_COMPARE(leaveSpy.count(), 1);

    // set a mask on the window
    win.setMask(QRegion(10, 20, 30, 40));
    // outside the mask we should not get an enter
    Test::pointerMotion(QPoint(5, 5), timestamp++);
    QVERIFY(!enterSpy.wait(100));
    QCOMPARE(enterSpy.count(), 1);
    // inside the mask we should still get an enter
    Test::pointerMotion(QPoint(25, 27), timestamp++);
    QTRY_COMPARE(enterSpy.count(), 2);
}

void InternalWindowTest::testPointerPressRelease()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::mousePressed);
    QSignalSpy releaseSpy(&win, &HelperWindow::mouseReleased);

    QTRY_COMPARE(windowAddedSpy.count(), 1);

    quint32 timestamp = 1;
    Test::pointerMotion(QPoint(50, 50), timestamp++);

    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);
}

void InternalWindowTest::testPointerAxis()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy wheelSpy(&win, &HelperWindow::wheel);
    QTRY_COMPARE(windowAddedSpy.count(), 1);

    quint32 timestamp = 1;
    Test::pointerMotion(QPoint(50, 50), timestamp++);

    Test::pointerAxisVertical(5.0, timestamp++);
    QTRY_COMPARE(wheelSpy.count(), 1);
    Test::pointerAxisHorizontal(5.0, timestamp++);
    QTRY_COMPARE(wheelSpy.count(), 2);
}

void InternalWindowTest::testTouch()
{
    // touch events for internal windows are emulated through mouse events
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);

    QSignalSpy touchDownSpy(&win, &HelperWindow::touchDown);
    QSignalSpy touchUpSpy(&win, &HelperWindow::touchUp);
    QSignalSpy touchMotionSpy(&win, &HelperWindow::touchMotion);

    quint32 timestamp = 1;
    Test::touchDown(1, QPointF(50, 50), timestamp++);
    QCOMPARE(touchDownSpy.count(), 1);
    QCOMPARE(touchDownSpy.last().at(0).toInt(), 1);
    QCOMPARE(touchDownSpy.last().at(1).toPointF(), QPointF(50, 50));

    Test::touchMotion(1, QPointF(90, 80), timestamp++);
    QCOMPARE(touchMotionSpy.count(), 1);
    QCOMPARE(touchMotionSpy.last().at(0).toInt(), 1);
    QCOMPARE(touchMotionSpy.last().at(1).toPointF(), QPointF(90, 80));

    Test::touchUp(1, timestamp++);
    QCOMPARE(touchUpSpy.count(), 1);
    QCOMPARE(touchUpSpy.last().at(0).toInt(), 1);
}

void InternalWindowTest::testOpacity()
{
    // this test verifies that opacity is properly synced from QWindow to InternalClient
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setOpacity(0.5);
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->isInternal());
    QCOMPARE(internalWindow->opacity(), 0.5);

    QSignalSpy opacityChangedSpy(internalWindow, &InternalWindow::opacityChanged);
    win.setOpacity(0.75);
    QCOMPARE(opacityChangedSpy.count(), 1);
    QCOMPARE(internalWindow->opacity(), 0.75);
}

void InternalWindowTest::testMove()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setOpacity(0.5);
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QCOMPARE(internalWindow->frameGeometry(), QRect(0, 0, 100, 100));

    // normal move should be synced
    internalWindow->move(QPoint(5, 10));
    QCOMPARE(internalWindow->frameGeometry(), QRect(5, 10, 100, 100));
    QTRY_COMPARE(win.geometry(), QRect(5, 10, 100, 100));
    // another move should also be synced
    internalWindow->move(QPoint(10, 20));
    QCOMPARE(internalWindow->frameGeometry(), QRect(10, 20, 100, 100));
    QTRY_COMPARE(win.geometry(), QRect(10, 20, 100, 100));
}

void InternalWindowTest::testSkipCloseAnimation_data()
{
    QTest::addColumn<bool>("initial");

    QTest::newRow("set") << true;
    QTest::newRow("not set") << false;
}

void InternalWindowTest::testSkipCloseAnimation()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setOpacity(0.5);
    win.setGeometry(0, 0, 100, 100);
    QFETCH(bool, initial);
    win.setProperty("KWIN_SKIP_CLOSE_ANIMATION", initial);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QCOMPARE(internalWindow->skipsCloseAnimation(), initial);
    QSignalSpy skipCloseChangedSpy(internalWindow, &Window::skipCloseAnimationChanged);
    win.setProperty("KWIN_SKIP_CLOSE_ANIMATION", !initial);
    QCOMPARE(skipCloseChangedSpy.count(), 1);
    QCOMPARE(internalWindow->skipsCloseAnimation(), !initial);
    win.setProperty("KWIN_SKIP_CLOSE_ANIMATION", initial);
    QCOMPARE(skipCloseChangedSpy.count(), 2);
    QCOMPARE(internalWindow->skipsCloseAnimation(), initial);
}

void InternalWindowTest::testModifierClickUnrestrictedMove()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() & ~Qt::FramelessWindowHint);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->isDecorated());

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("MouseBindings"));
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAll1", "Move");
    group.writeEntry("CommandAll2", "Move");
    group.writeEntry("CommandAll3", "Move");
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->commandAllModifier(), Qt::MetaModifier);
    QCOMPARE(options->commandAll1(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll2(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll3(), Options::MouseUnrestrictedMove);

    // move cursor on window
    input()->pointer()->warp(internalWindow->frameGeometry().center());

    // simulate modifier+click
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(!internalWindow->isInteractiveMove());
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(internalWindow->isInteractiveMove());
    // release modifier should not change it
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(internalWindow->isInteractiveMove());
    // but releasing the key should end move/resize
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(!internalWindow->isInteractiveMove());
}

void InternalWindowTest::testModifierScroll()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() & ~Qt::FramelessWindowHint);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->isDecorated());

    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("MouseBindings"));
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();

    // move cursor on window
    input()->pointer()->warp(internalWindow->frameGeometry().center());

    // set the opacity to 0.5
    internalWindow->setOpacity(0.5);
    QCOMPARE(internalWindow->opacity(), 0.5);
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    Test::pointerAxisVertical(-5, timestamp++);
    QCOMPARE(internalWindow->opacity(), 0.6);
    Test::pointerAxisVertical(5, timestamp++);
    QCOMPARE(internalWindow->opacity(), 0.5);
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
}

void InternalWindowTest::testPopup()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() | Qt::Popup);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QCOMPARE(internalWindow->isPopupWindow(), true);

    QSignalSpy pressSpy(&win, &HelperWindow::keyPressed);
    QSignalSpy releaseSpy(&win, &HelperWindow::keyReleased);

    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    QCOMPARE(releaseSpy.count(), 0);
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);
    QCOMPARE(pressSpy.count(), 1);
}

void InternalWindowTest::testScale()
{
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .scale = 2.0,
        },
        Test::OutputInfo{
            .geometry = QRect(1280, 0, 1280, 1024),
            .scale = 2.0,
        },
    });

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() | Qt::Popup);
    win.show();
    QCOMPARE(win.devicePixelRatio(), 2.0);
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QCOMPARE(internalWindow->bufferScale(), 2);
}

void InternalWindowTest::testEffectWindow()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->effectWindow());
    QCOMPARE(internalWindow->effectWindow()->internalWindow(), &win);

    QCOMPARE(effects->findWindow(&win), internalWindow->effectWindow());
    QCOMPARE(effects->findWindow(&win)->internalWindow(), &win);
}

void InternalWindowTest::testReentrantMoveResize()
{
    // This test verifies that calling moveResize() from a slot connected directly
    // to the frameGeometryChanged() signal won't cause an infinite recursion.

    // Create an internal window.
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto window = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(window);
    QCOMPARE(window->pos(), QPoint(0, 0));

    // Let's pretend that there is a script that really wants the window to be at (100, 100).
    connect(window, &Window::frameGeometryChanged, this, [window]() {
        window->moveResize(QRectF(QPointF(100, 100), window->size()));
    });

    // Trigger the lambda above.
    window->move(QPoint(40, 50));

    // Eventually, the window will end up at (100, 100).
    QCOMPARE(window->pos(), QPoint(100, 100));
}

void InternalWindowTest::testDismissPopup()
{
    // This test verifies that a popup window created by the compositor will be dismissed
    // when user clicks another window.

    // Create a toplevel window.
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    HelperWindow clientToplevel;
    clientToplevel.setGeometry(0, 0, 100, 100);
    clientToplevel.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto serverToplevel = windowAddedSpy.last().first().value<InternalWindow *>();
    QVERIFY(serverToplevel);

    // Create a popup window.
    QRasterWindow clientPopup;
    clientPopup.setFlag(Qt::Popup);
    clientPopup.setTransientParent(&clientToplevel);
    clientPopup.setGeometry(0, 0, 50, 50);
    clientPopup.show();
    QTRY_COMPARE(windowAddedSpy.count(), 2);
    auto serverPopup = windowAddedSpy.last().first().value<InternalWindow *>();
    QVERIFY(serverPopup);

    // Create the other window to click
    HelperWindow otherClientToplevel;
    otherClientToplevel.setGeometry(100, 100, 100, 100);
    otherClientToplevel.show();
    QTRY_COMPARE(windowAddedSpy.count(), 3);
    auto serverOtherToplevel = windowAddedSpy.last().first().value<InternalWindow *>();
    QVERIFY(serverOtherToplevel);

    // Click somewhere outside the popup window.
    QSignalSpy popupClosedSpy(serverPopup, &InternalWindow::closed);
    quint32 timestamp = 0;
    Test::pointerMotion(serverOtherToplevel->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QTRY_COMPARE(popupClosedSpy.count(), 1);
}

}

WAYLANDTEST_MAIN(KWin::InternalWindowTest)
#include "internal_window.moc"
