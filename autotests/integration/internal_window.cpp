/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "cursor.h"
#include "effects.h"
#include "internal_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QPainter>
#include <QRasterWindow>

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/seat.h>
#include <KWindowSystem>

#include <KWaylandServer/surface_interface.h>

#include <linux/input.h>

using namespace KWayland::Client;

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
    void testReentrantSetFrameGeometry();
};

class HelperWindow : public QRasterWindow
{
    Q_OBJECT
public:
    HelperWindow();
    ~HelperWindow() override;

    QPoint latestGlobalMousePos() const {
        return m_latestGlobalMousePos;
    }
    Qt::MouseButtons pressedButtons() const {
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
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(0, 0, width(), height(), Qt::red);
}

bool HelperWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        emit entered();
    }
    if (event->type() == QEvent::Leave) {
        emit left();
    }
    return QRasterWindow::event(event);
}

void HelperWindow::mouseMoveEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPos();
    emit mouseMoved(event->globalPos());
}

void HelperWindow::mousePressEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPos();
    m_pressedButtons = event->buttons();
    emit mousePressed();
}

void HelperWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPos();
    m_pressedButtons = event->buttons();
    emit mouseReleased();
}

void HelperWindow::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    emit wheel();
}

void HelperWindow::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    emit keyPressed();
}

void HelperWindow::keyReleaseEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    emit keyReleased();
}

void InternalWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWin::InternalClient *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));
    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void InternalWindowTest::init()
{
    Cursors::self()->mouse()->setPos(QPoint(1280, 512));
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void InternalWindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void InternalWindowTest::testEnterLeave()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    QVERIFY(!workspace()->findInternal(nullptr));
    QVERIFY(!workspace()->findInternal(&win));
    win.setGeometry(0, 0, 100, 100);
    win.show();

    QTRY_COMPARE(clientAddedSpy.count(), 1);
    QVERIFY(!workspace()->activeClient());
    InternalClient *c = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(c);
    QVERIFY(c->isInternal());
    QVERIFY(!c->isDecorated());
    QCOMPARE(workspace()->findInternal(&win), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 100));
    QVERIFY(c->isShown(false));
    QVERIFY(workspace()->xStackingOrder().contains(c));

    QSignalSpy enterSpy(&win, &HelperWindow::entered);
    QVERIFY(enterSpy.isValid());
    QSignalSpy leaveSpy(&win, &HelperWindow::left);
    QVERIFY(leaveSpy.isValid());
    QSignalSpy moveSpy(&win, &HelperWindow::mouseMoved);
    QVERIFY(moveSpy.isValid());

    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(QPoint(50, 50), timestamp++);
    QTRY_COMPARE(moveSpy.count(), 1);

    kwinApp()->platform()->pointerMotion(QPoint(60, 50), timestamp++);
    QTRY_COMPARE(moveSpy.count(), 2);
    QCOMPARE(moveSpy[1].first().toPoint(), QPoint(60, 50));

    kwinApp()->platform()->pointerMotion(QPoint(101, 50), timestamp++);
    QTRY_COMPARE(leaveSpy.count(), 1);

    // set a mask on the window
    win.setMask(QRegion(10, 20, 30, 40));
    // outside the mask we should not get an enter
    kwinApp()->platform()->pointerMotion(QPoint(5, 5), timestamp++);
    QVERIFY(!enterSpy.wait(100));
    QCOMPARE(enterSpy.count(), 1);
    // inside the mask we should still get an enter
    kwinApp()->platform()->pointerMotion(QPoint(25, 27), timestamp++);
    QTRY_COMPARE(enterSpy.count(), 2);
}

void InternalWindowTest::testPointerPressRelease()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::mousePressed);
    QVERIFY(pressSpy.isValid());
    QSignalSpy releaseSpy(&win, &HelperWindow::mouseReleased);
    QVERIFY(releaseSpy.isValid());

    QTRY_COMPARE(clientAddedSpy.count(), 1);

    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(QPoint(50, 50), timestamp++);

    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);
}

void InternalWindowTest::testPointerAxis()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy wheelSpy(&win, &HelperWindow::wheel);
    QVERIFY(wheelSpy.isValid());
    QTRY_COMPARE(clientAddedSpy.count(), 1);

    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(QPoint(50, 50), timestamp++);

    kwinApp()->platform()->pointerAxisVertical(5.0, timestamp++);
    QTRY_COMPARE(wheelSpy.count(), 1);
    kwinApp()->platform()->pointerAxisHorizontal(5.0, timestamp++);
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
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::keyPressed);
    QVERIFY(pressSpy.isValid());
    QSignalSpy releaseSpy(&win, &HelperWindow::keyReleased);
    QVERIFY(releaseSpy.isValid());
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QVERIFY(internalClient->isInternal());
    QVERIFY(internalClient->readyForPainting());

    quint32 timestamp = 1;
    QFETCH(QPoint, cursorPos);
    kwinApp()->platform()->pointerMotion(cursorPos, timestamp++);

    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    QCOMPARE(releaseSpy.count(), 0);
    kwinApp()->platform()->keyboardKeyReleased(KEY_A, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);
    QCOMPARE(pressSpy.count(), 1);
}

void InternalWindowTest::testKeyboardShowWithoutActivating()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setProperty("_q_showWithoutActivating", true);
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::keyPressed);
    QVERIFY(pressSpy.isValid());
    QSignalSpy releaseSpy(&win, &HelperWindow::keyReleased);
    QVERIFY(releaseSpy.isValid());
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QVERIFY(internalClient->isInternal());
    QVERIFY(internalClient->readyForPainting());

    quint32 timestamp = 1;
    const QPoint cursorPos = QPoint(50, 50);
    kwinApp()->platform()->pointerMotion(cursorPos, timestamp++);

    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    QCOMPARE(pressSpy.count(), 0);
    QVERIFY(!pressSpy.wait(100));
    QCOMPARE(releaseSpy.count(), 0);
    kwinApp()->platform()->keyboardKeyReleased(KEY_A, timestamp++);
    QCOMPARE(releaseSpy.count(), 0);
    QVERIFY(!releaseSpy.wait(100));
    QCOMPARE(pressSpy.count(), 0);
}

void InternalWindowTest::testKeyboardTriggersLeave()
{
    // this test verifies that a leave event is sent to a client when an internal window
    // gets a key event
    QScopedPointer<Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QVERIFY(!keyboard.isNull());
    QVERIFY(keyboard->isValid());
    QSignalSpy enteredSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(leftSpy.isValid());
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));

    // now let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());
    QVERIFY(!c->isInternal());

    if (enteredSpy.isEmpty()) {
        QVERIFY(enteredSpy.wait());
    }
    QCOMPARE(enteredSpy.count(), 1);

    // create internal window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::keyPressed);
    QVERIFY(pressSpy.isValid());
    QSignalSpy releaseSpy(&win, &HelperWindow::keyReleased);
    QVERIFY(releaseSpy.isValid());
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QVERIFY(internalClient->isInternal());
    QVERIFY(internalClient->readyForPainting());

    QVERIFY(leftSpy.isEmpty());
    QVERIFY(!leftSpy.wait(100));

    // now let's trigger a key, which should result in a leave
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(leftSpy.wait());
    QCOMPARE(pressSpy.count(), 1);

    kwinApp()->platform()->keyboardKeyReleased(KEY_A, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);

    // after hiding the internal window, next key press should trigger an enter
    win.hide();
    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(enteredSpy.wait());
    kwinApp()->platform()->keyboardKeyReleased(KEY_A, timestamp++);

    // Destroy the test client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
}

void InternalWindowTest::testTouch()
{
    // touch events for internal windows are emulated through mouse events
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);

    QSignalSpy pressSpy(&win, &HelperWindow::mousePressed);
    QVERIFY(pressSpy.isValid());
    QSignalSpy releaseSpy(&win, &HelperWindow::mouseReleased);
    QVERIFY(releaseSpy.isValid());
    QSignalSpy moveSpy(&win, &HelperWindow::mouseMoved);
    QVERIFY(moveSpy.isValid());

    quint32 timestamp = 1;
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());
    kwinApp()->platform()->touchDown(0, QPointF(50, 50), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // further touch down should not trigger
    kwinApp()->platform()->touchDown(1, QPointF(75, 75), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    kwinApp()->platform()->touchUp(1, timestamp++);
    QCOMPARE(releaseSpy.count(), 0);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // another press
    kwinApp()->platform()->touchDown(1, QPointF(10, 10), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // simulate the move
    QCOMPARE(moveSpy.count(), 0);
    kwinApp()->platform()->touchMotion(0, QPointF(80, 90), timestamp++);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // move on other ID should not do anything
    kwinApp()->platform()->touchMotion(1, QPointF(20, 30), timestamp++);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // now up our main point
    kwinApp()->platform()->touchUp(0, timestamp++);
    QCOMPARE(releaseSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());

    // and up the additional point
    kwinApp()->platform()->touchUp(1, timestamp++);
    QCOMPARE(releaseSpy.count(), 1);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());
}

void InternalWindowTest::testOpacity()
{
    // this test verifies that opacity is properly synced from QWindow to InternalClient
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setOpacity(0.5);
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QVERIFY(internalClient->isInternal());
    QCOMPARE(internalClient->opacity(), 0.5);

    QSignalSpy opacityChangedSpy(internalClient, &InternalClient::opacityChanged);
    QVERIFY(opacityChangedSpy.isValid());
    win.setOpacity(0.75);
    QCOMPARE(opacityChangedSpy.count(), 1);
    QCOMPARE(internalClient->opacity(), 0.75);
}

void InternalWindowTest::testMove()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setOpacity(0.5);
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QCOMPARE(internalClient->frameGeometry(), QRect(0, 0, 100, 100));

    // normal move should be synced
    internalClient->move(5, 10);
    QCOMPARE(internalClient->frameGeometry(), QRect(5, 10, 100, 100));
    QTRY_COMPARE(win.geometry(), QRect(5, 10, 100, 100));
    // another move should also be synced
    internalClient->move(10, 20);
    QCOMPARE(internalClient->frameGeometry(), QRect(10, 20, 100, 100));
    QTRY_COMPARE(win.geometry(), QRect(10, 20, 100, 100));

    // now move with a Geometry update blocker
    {
        GeometryUpdatesBlocker blocker(internalClient);
        internalClient->move(5, 10);
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
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setOpacity(0.5);
    win.setGeometry(0, 0, 100, 100);
    QFETCH(bool, initial);
    win.setProperty("KWIN_SKIP_CLOSE_ANIMATION", initial);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QCOMPARE(internalClient->skipsCloseAnimation(), initial);
    QSignalSpy skipCloseChangedSpy(internalClient, &Toplevel::skipCloseAnimationChanged);
    QVERIFY(skipCloseChangedSpy.isValid());
    win.setProperty("KWIN_SKIP_CLOSE_ANIMATION", !initial);
    QCOMPARE(skipCloseChangedSpy.count(), 1);
    QCOMPARE(internalClient->skipsCloseAnimation(), !initial);
    win.setProperty("KWIN_SKIP_CLOSE_ANIMATION", initial);
    QCOMPARE(skipCloseChangedSpy.count(), 2);
    QCOMPARE(internalClient->skipsCloseAnimation(), initial);
}

void InternalWindowTest::testModifierClickUnrestrictedMove()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() & ~Qt::FramelessWindowHint);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QVERIFY(internalClient->isDecorated());

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
    Cursors::self()->mouse()->setPos(internalClient->frameGeometry().center());

    // simulate modifier+click
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(!internalClient->isMove());
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(internalClient->isMove());
    // release modifier should not change it
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(internalClient->isMove());
    // but releasing the key should end move/resize
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(!internalClient->isMove());
}

void InternalWindowTest::testModifierScroll()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() & ~Qt::FramelessWindowHint);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QVERIFY(internalClient->isDecorated());

    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();

    // move cursor on window
    Cursors::self()->mouse()->setPos(internalClient->frameGeometry().center());

    // set the opacity to 0.5
    internalClient->setOpacity(0.5);
    QCOMPARE(internalClient->opacity(), 0.5);
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->pointerAxisVertical(-5, timestamp++);
    QCOMPARE(internalClient->opacity(), 0.6);
    kwinApp()->platform()->pointerAxisVertical(5, timestamp++);
    QCOMPARE(internalClient->opacity(), 0.5);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
}

void InternalWindowTest::testPopup()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() | Qt::Popup);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QCOMPARE(internalClient->isPopupWindow(), true);
}

void InternalWindowTest::testScale()
{
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection,
        Q_ARG(int, 2),
        Q_ARG(QVector<QRect>, QVector<QRect>({QRect(0,0,1280, 1024), QRect(1280/2, 0, 1280, 1024)})),
        Q_ARG(QVector<int>, QVector<int>({2,2})));

    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.setFlags(win.flags() | Qt::Popup);
    win.show();
    QCOMPARE(win.devicePixelRatio(), 2.0);
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QCOMPARE(internalClient->bufferScale(), 2);
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
}

void InternalWindowTest::testWindowType()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    QFETCH(NET::WindowType, windowType);
    KWindowSystem::setType(win.winId(), windowType);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QCOMPARE(internalClient->windowType(), windowType);
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
}

void InternalWindowTest::testChangeWindowType()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QCOMPARE(internalClient->windowType(), NET::Normal);

    QFETCH(NET::WindowType, windowType);
    KWindowSystem::setType(win.winId(), windowType);
    QTRY_COMPARE(internalClient->windowType(), windowType);

    KWindowSystem::setType(win.winId(), NET::Normal);
    QTRY_COMPARE(internalClient->windowType(), NET::Normal);
}

void InternalWindowTest::testEffectWindow()
{
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto internalClient = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(internalClient);
    QVERIFY(internalClient->effectWindow());
    QCOMPARE(internalClient->effectWindow()->internalWindow(), &win);

    QCOMPARE(effects->findWindow(&win), internalClient->effectWindow());
    QCOMPARE(effects->findWindow(&win)->internalWindow(), &win);
}

void InternalWindowTest::testReentrantSetFrameGeometry()
{
    // This test verifies that calling setFrameGeometry() from a slot connected directly
    // to the frameGeometryChanged() signal won't cause an infinite recursion.

    // Create an internal window.
    QSignalSpy clientAddedSpy(workspace(), &Workspace::internalClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QTRY_COMPARE(clientAddedSpy.count(), 1);
    auto client = clientAddedSpy.first().first().value<InternalClient *>();
    QVERIFY(client);
    QCOMPARE(client->pos(), QPoint(0, 0));

    // Let's pretend that there is a script that really wants the client to be at (100, 100).
    connect(client, &AbstractClient::frameGeometryChanged, this, [client]() {
        client->setFrameGeometry(QRect(QPoint(100, 100), client->size()));
    });

    // Trigger the lambda above.
    client->move(QPoint(40, 50));

    // Eventually, the client will end up at (100, 100).
    QCOMPARE(client->pos(), QPoint(100, 100));
}

}

WAYLANDTEST_MAIN(KWin::InternalWindowTest)
#include "internal_window.moc"
