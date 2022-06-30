/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "deleted.h"
#include "effects.h"
#include "internalwindow.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QPainter>
#include <QRasterWindow>

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>
#include <KWindowSystem>

#include <linux/input.h>

Q_DECLARE_METATYPE(NET::WindowType);

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
    void testKeyboard_data();
    void testKeyboard();
    void testKeyboardShowWithoutActivating();
    void testKeyboardTriggersLeave();
    void testTouch();
    void testOpacity();
    void testMove();
    void testSkipCloseAnimation_data();
    void testSkipCloseAnimation();
    void testModifierClickUnrestrictedMove();
    void testModifierScroll();
    void testPopup();
    void testScale();
    void testWindowType_data();
    void testWindowType();
    void testChangeWindowType_data();
    void testChangeWindowType();
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

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

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
    m_latestGlobalMousePos = event->globalPos();
    Q_EMIT mouseMoved(event->globalPos());
}

void HelperWindow::mousePressEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPos();
    m_pressedButtons = event->buttons();
    Q_EMIT mousePressed();
}

void HelperWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPos();
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

void InternalWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::InternalWindow *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));
    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void InternalWindowTest::init()
{
    Cursors::self()->mouse()->setPos(QPoint(512, 512));
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void InternalWindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void InternalWindowTest::testEnterLeave()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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

void InternalWindowTest::testKeyboard_data()
{
    QTest::addColumn<QPoint>("cursorPos");

    QTest::newRow("on Window") << QPoint(50, 50);
    QTest::newRow("outside Window") << QPoint(250, 250);
}

void InternalWindowTest::testKeyboard()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::keyPressed);
    QSignalSpy releaseSpy(&win, &HelperWindow::keyReleased);
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->isInternal());
    QVERIFY(internalWindow->readyForPainting());

    quint32 timestamp = 1;
    QFETCH(QPoint, cursorPos);
    Test::pointerMotion(cursorPos, timestamp++);

    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    QCOMPARE(releaseSpy.count(), 0);
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);
    QCOMPARE(pressSpy.count(), 1);
}

void InternalWindowTest::testKeyboardShowWithoutActivating()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setProperty("_q_showWithoutActivating", true);
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::keyPressed);
    QSignalSpy releaseSpy(&win, &HelperWindow::keyReleased);
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->isInternal());
    QVERIFY(internalWindow->readyForPainting());

    quint32 timestamp = 1;
    const QPoint cursorPos = QPoint(50, 50);
    Test::pointerMotion(cursorPos, timestamp++);

    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QCOMPARE(pressSpy.count(), 0);
    QVERIFY(!pressSpy.wait(100));
    QCOMPARE(releaseSpy.count(), 0);
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QCOMPARE(releaseSpy.count(), 0);
    QVERIFY(!releaseSpy.wait(100));
    QCOMPARE(pressSpy.count(), 0);
}

void InternalWindowTest::testKeyboardTriggersLeave()
{
    // this test verifies that a leave event is sent to a window when an internal window
    // gets a key event
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QVERIFY(keyboard != nullptr);
    QVERIFY(keyboard->isValid());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy leftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    // now let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QVERIFY(!window->isInternal());

    if (enteredSpy.isEmpty()) {
        QVERIFY(enteredSpy.wait());
    }
    QCOMPARE(enteredSpy.count(), 1);

    // create internal window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::keyPressed);
    QSignalSpy releaseSpy(&win, &HelperWindow::keyReleased);
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->isInternal());
    QVERIFY(internalWindow->readyForPainting());

    QVERIFY(leftSpy.isEmpty());
    QVERIFY(!leftSpy.wait(100));

    // now let's trigger a key, which should result in a leave
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(leftSpy.wait());
    QCOMPARE(pressSpy.count(), 1);

    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);

    // after hiding the internal window, next key press should trigger an enter
    win.hide();
    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(enteredSpy.wait());
    Test::keyboardKeyReleased(KEY_A, timestamp++);

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void InternalWindowTest::testTouch()
{
    // touch events for internal windows are emulated through mouse events
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);

    QSignalSpy pressSpy(&win, &HelperWindow::mousePressed);
    QSignalSpy releaseSpy(&win, &HelperWindow::mouseReleased);
    QSignalSpy moveSpy(&win, &HelperWindow::mouseMoved);

    quint32 timestamp = 1;
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());
    Test::touchDown(0, QPointF(50, 50), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // further touch down should not trigger
    Test::touchDown(1, QPointF(75, 75), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    Test::touchUp(1, timestamp++);
    QCOMPARE(releaseSpy.count(), 0);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // another press
    Test::touchDown(1, QPointF(10, 10), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // simulate the move
    QCOMPARE(moveSpy.count(), 0);
    Test::touchMotion(0, QPointF(80, 90), timestamp++);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // move on other ID should not do anything
    Test::touchMotion(1, QPointF(20, 30), timestamp++);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // now up our main point
    Test::touchUp(0, timestamp++);
    QCOMPARE(releaseSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());

    // and up the additional point
    Test::touchUp(1, timestamp++);
    QCOMPARE(releaseSpy.count(), 1);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());
}

void InternalWindowTest::testOpacity()
{
    // this test verifies that opacity is properly synced from QWindow to InternalClient
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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

    // now move with a Geometry update blocker
    {
        GeometryUpdatesBlocker blocker(internalWindow);
        internalWindow->move(QPoint(5, 10));
        // not synced!
        QCOMPARE(win.geometry(), QRect(10, 20, 100, 100));
    }
    // after destroying the blocker it should be synced
    QTRY_COMPARE(win.geometry(), QRect(5, 10, 100, 100));
}

void InternalWindowTest::testSkipCloseAnimation_data()
{
    QTest::addColumn<bool>("initial");

    QTest::newRow("set") << true;
    QTest::newRow("not set") << false;
}

void InternalWindowTest::testSkipCloseAnimation()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() & ~Qt::FramelessWindowHint);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->isDecorated());

    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
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
    Cursors::self()->mouse()->setPos(internalWindow->frameGeometry().center());

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
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() & ~Qt::FramelessWindowHint);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QVERIFY(internalWindow->isDecorated());

    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();

    // move cursor on window
    Cursors::self()->mouse()->setPos(internalWindow->frameGeometry().center());

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
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() | Qt::Popup);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QCOMPARE(internalWindow->isPopupWindow(), true);
}

void InternalWindowTest::testScale()
{
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection,
                              Q_ARG(QVector<QRect>, QVector<QRect>({QRect(0, 0, 1280, 1024), QRect(1280 / 2, 0, 1280, 1024)})),
                              Q_ARG(QVector<qreal>, QVector<qreal>({2, 2})));

    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() | Qt::Popup);
    win.show();
    QCOMPARE(win.devicePixelRatio(), 2.0);
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QCOMPARE(internalWindow->bufferScale(), 2);
}

void InternalWindowTest::testWindowType_data()
{
    QTest::addColumn<NET::WindowType>("windowType");

    QTest::newRow("normal") << NET::Normal;
    QTest::newRow("desktop") << NET::Desktop;
    QTest::newRow("Dock") << NET::Dock;
    QTest::newRow("Toolbar") << NET::Toolbar;
    QTest::newRow("Menu") << NET::Menu;
    QTest::newRow("Dialog") << NET::Dialog;
    QTest::newRow("Utility") << NET::Utility;
    QTest::newRow("Splash") << NET::Splash;
    QTest::newRow("DropdownMenu") << NET::DropdownMenu;
    QTest::newRow("PopupMenu") << NET::PopupMenu;
    QTest::newRow("Tooltip") << NET::Tooltip;
    QTest::newRow("Notification") << NET::Notification;
    QTest::newRow("ComboBox") << NET::ComboBox;
    QTest::newRow("OnScreenDisplay") << NET::OnScreenDisplay;
    QTest::newRow("CriticalNotification") << NET::CriticalNotification;
    QTest::newRow("AppletPopup") << NET::AppletPopup;
}

void InternalWindowTest::testWindowType()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    QFETCH(NET::WindowType, windowType);
    KWindowSystem::setType(win.winId(), windowType);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QCOMPARE(internalWindow->windowType(), windowType);
}

void InternalWindowTest::testChangeWindowType_data()
{
    QTest::addColumn<NET::WindowType>("windowType");

    QTest::newRow("desktop") << NET::Desktop;
    QTest::newRow("Dock") << NET::Dock;
    QTest::newRow("Toolbar") << NET::Toolbar;
    QTest::newRow("Menu") << NET::Menu;
    QTest::newRow("Dialog") << NET::Dialog;
    QTest::newRow("Utility") << NET::Utility;
    QTest::newRow("Splash") << NET::Splash;
    QTest::newRow("DropdownMenu") << NET::DropdownMenu;
    QTest::newRow("PopupMenu") << NET::PopupMenu;
    QTest::newRow("Tooltip") << NET::Tooltip;
    QTest::newRow("Notification") << NET::Notification;
    QTest::newRow("ComboBox") << NET::ComboBox;
    QTest::newRow("OnScreenDisplay") << NET::OnScreenDisplay;
    QTest::newRow("CriticalNotification") << NET::CriticalNotification;
    QTest::newRow("AppletPopup") << NET::AppletPopup;
}

void InternalWindowTest::testChangeWindowType()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(windowAddedSpy.count(), 1);
    auto internalWindow = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internalWindow);
    QCOMPARE(internalWindow->windowType(), NET::Normal);

    QFETCH(NET::WindowType, windowType);
    KWindowSystem::setType(win.winId(), windowType);
    QTRY_COMPARE(internalWindow->windowType(), windowType);

    KWindowSystem::setType(win.winId(), NET::Normal);
    QTRY_COMPARE(internalWindow->windowType(), NET::Normal);
}

void InternalWindowTest::testEffectWindow()
{
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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
    QSignalSpy windowAddedSpy(workspace(), &Workspace::internalWindowAdded);
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
    QSignalSpy popupClosedSpy(serverPopup, &InternalWindow::windowClosed);
    quint32 timestamp = 0;
    Test::pointerMotion(serverOtherToplevel->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QTRY_COMPARE(popupClosedSpy.count(), 1);
}

}

WAYLANDTEST_MAIN(KWin::InternalWindowTest)
#include "internal_window.moc"
