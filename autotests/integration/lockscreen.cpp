/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "abstract_client.h"
#include "composite.h"
#include "cursor.h"
#include "scene.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include <kwineffects.h>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>
#include <KWaylandServer/seat_interface.h>

//screenlocker
#include <KScreenLocker/KsldApp>

#include <KGlobalAccel>

#include <linux/input.h>

Q_DECLARE_METATYPE(Qt::Orientation)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_lock_screen-0");

class LockScreenTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testStackingOrder();
    void testPointer();
    void testPointerButton();
    void testPointerAxis();
    void testKeyboard();
    void testScreenEdge();
    void testEffects();
    void testEffectsKeyboard();
    void testEffectsKeyboardAutorepeat();
    void testMoveWindow();
    void testPointerShortcut();
    void testAxisShortcut_data();
    void testAxisShortcut();
    void testKeyboardShortcut();
    void testTouch();

private:
    void unlock();
    AbstractClient *showWindow();
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
};

class HelperEffect : public Effect
{
    Q_OBJECT
public:
    HelperEffect() {}
    ~HelperEffect() override {}

    void windowInputMouseEvent(QEvent*) override {
        emit inputEvent();
    }
    void grabbedKeyboardEvent(QKeyEvent *e) override {
        emit keyEvent(e->text());
    }

Q_SIGNALS:
    void inputEvent();
    void keyEvent(const QString&);
};

#define LOCK \
    QVERIFY(!waylandServer()->isScreenLocked()); \
    QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged); \
    QVERIFY(lockStateChangedSpy.isValid()); \
    ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate); \
    QCOMPARE(lockStateChangedSpy.count(), 1); \
    QVERIFY(waylandServer()->isScreenLocked());

#define UNLOCK \
    int expectedLockCount = 1; \
    if (ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::Locked) { \
        expectedLockCount = 2; \
    } \
    QCOMPARE(lockStateChangedSpy.count(), expectedLockCount); \
    unlock(); \
    if (lockStateChangedSpy.count() < expectedLockCount + 1) { \
        QVERIFY(lockStateChangedSpy.wait()); \
    } \
    QCOMPARE(lockStateChangedSpy.count(), expectedLockCount + 1); \
    QVERIFY(!waylandServer()->isScreenLocked());

#define MOTION(target) \
    kwinApp()->platform()->pointerMotion(target, timestamp++)

#define PRESS \
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++)

#define RELEASE \
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++)

#define KEYPRESS( key ) \
    kwinApp()->platform()->keyboardKeyPressed(key, timestamp++)

#define KEYRELEASE( key ) \
    kwinApp()->platform()->keyboardKeyReleased(key, timestamp++)

void LockScreenTest::unlock()
{
    using namespace ScreenLocker;
    const auto children = KSldApp::self()->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
}

AbstractClient *LockScreenTest::showWindow()
{
    using namespace KWayland::Client;
#define VERIFY(statement) \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
        return nullptr;
#define COMPARE(actual, expected) \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return nullptr;

    Surface *surface = Test::createSurface(m_compositor);
    VERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    VERIFY(shellSurface);
    // let's render
    auto c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);

    VERIFY(c);
    COMPARE(workspace()->activeClient(), c);

#undef VERIFY
#undef COMPARE

    return c;
}

void LockScreenTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();

    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);
    QCOMPARE(scene->compositingType(), KWin::OpenGL2Compositing);
}

void LockScreenTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());
    m_connection = Test::waylandConnection();
    m_compositor = Test::waylandCompositor();
    m_shm = Test::waylandShmPool();
    m_seat = Test::waylandSeat();

    screens()->setCurrent(0);
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void LockScreenTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void LockScreenTest::testStackingOrder()
{
    // This test verifies that the lockscreen greeter is placed above other windows.

    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());

    LOCK
    QVERIFY(clientAddedSpy.wait());

    AbstractClient *client = clientAddedSpy.first().first().value<AbstractClient *>();
    QVERIFY(client);
    QVERIFY(client->isLockScreen());
    QCOMPARE(client->layer(), UnmanagedLayer);

    UNLOCK
}

void LockScreenTest::testPointer()
{
    using namespace KWayland::Client;

    QScopedPointer<Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    QSignalSpy enteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer.data(), &Pointer::left);
    QVERIFY(leftSpy.isValid());

    AbstractClient *c = showWindow();
    QVERIFY(c);

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    MOTION(c->frameGeometry().center());
    QVERIFY(enteredSpy.wait());

    LOCK

    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);

    // simulate moving out in and out again
    MOTION(c->frameGeometry().center());
    MOTION(c->frameGeometry().bottomRight() + QPoint(100, 100));
    MOTION(c->frameGeometry().bottomRight() + QPoint(100, 100));
    QVERIFY(!leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(enteredSpy.count(), 1);

    // go back on the window
    MOTION(c->frameGeometry().center());
    // and unlock
    UNLOCK

    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    // move on the window
    MOTION(c->frameGeometry().center() + QPoint(100, 100));
    QVERIFY(leftSpy.wait());
    MOTION(c->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 3);
}

void LockScreenTest::testPointerButton()
{
    using namespace KWayland::Client;

    QScopedPointer<Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    QSignalSpy enteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy buttonChangedSpy(pointer.data(), &Pointer::buttonStateChanged);
    QVERIFY(buttonChangedSpy.isValid());

    AbstractClient *c = showWindow();
    QVERIFY(c);

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    MOTION(c->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // and simulate a click
    PRESS;
    QVERIFY(buttonChangedSpy.wait());
    RELEASE;
    QVERIFY(buttonChangedSpy.wait());

    LOCK

    // and simulate a click
    PRESS;
    QVERIFY(!buttonChangedSpy.wait());
    RELEASE;
    QVERIFY(!buttonChangedSpy.wait());

    UNLOCK
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);

    // and click again
    PRESS;
    QVERIFY(buttonChangedSpy.wait());
    RELEASE;
    QVERIFY(buttonChangedSpy.wait());
}

void LockScreenTest::testPointerAxis()
{
    using namespace KWayland::Client;

    QScopedPointer<Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    QSignalSpy axisChangedSpy(pointer.data(), &Pointer::axisChanged);
    QVERIFY(axisChangedSpy.isValid());
    QSignalSpy enteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    AbstractClient *c = showWindow();
    QVERIFY(c);

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    MOTION(c->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // and simulate axis
    kwinApp()->platform()->pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());

    LOCK

    // and simulate axis
    kwinApp()->platform()->pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(!axisChangedSpy.wait(100));
    kwinApp()->platform()->pointerAxisVertical(5.0, timestamp++);
    QVERIFY(!axisChangedSpy.wait(100));

    // and unlock
    UNLOCK
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);

    // and move axis again
    kwinApp()->platform()->pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());
    kwinApp()->platform()->pointerAxisVertical(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());
}

void LockScreenTest::testKeyboard()
{
    using namespace KWayland::Client;

    QScopedPointer<Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(!keyboard.isNull());
    QSignalSpy enteredSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy keyChangedSpy(keyboard.data(), &Keyboard::keyChanged);
    QVERIFY(keyChangedSpy.isValid());

    AbstractClient *c = showWindow();
    QVERIFY(c);
    QVERIFY(enteredSpy.wait());
    QTRY_COMPARE(enteredSpy.count(), 1);

    quint32 timestamp = 1;
    KEYPRESS(KEY_A);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 1);
    QCOMPARE(keyChangedSpy.at(0).at(0).value<quint32>(), quint32(KEY_A));
    QCOMPARE(keyChangedSpy.at(0).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Pressed);
    QCOMPARE(keyChangedSpy.at(0).at(2).value<quint32>(), quint32(1));
    KEYRELEASE(KEY_A);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 2);
    QCOMPARE(keyChangedSpy.at(1).at(0).value<quint32>(), quint32(KEY_A));
    QCOMPARE(keyChangedSpy.at(1).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(1).at(2).value<quint32>(), quint32(2));

    LOCK
    QVERIFY(leftSpy.wait());
    KEYPRESS(KEY_B);
    KEYRELEASE(KEY_B);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(keyChangedSpy.count(), 2);

    UNLOCK
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    KEYPRESS(KEY_C);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 3);
    KEYRELEASE(KEY_C);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 4);
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(keyChangedSpy.at(2).at(0).value<quint32>(), quint32(KEY_C));
    QCOMPARE(keyChangedSpy.at(3).at(0).value<quint32>(), quint32(KEY_C));
    QCOMPARE(keyChangedSpy.at(2).at(2).value<quint32>(), quint32(5));
    QCOMPARE(keyChangedSpy.at(3).at(2).value<quint32>(), quint32(6));
    QCOMPARE(keyChangedSpy.at(2).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Pressed);
    QCOMPARE(keyChangedSpy.at(3).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Released);
}

void LockScreenTest::testScreenEdge()
{
    QSignalSpy screenEdgeSpy(ScreenEdges::self(), &ScreenEdges::approaching);
    QVERIFY(screenEdgeSpy.isValid());
    QCOMPARE(screenEdgeSpy.count(), 0);

    quint32 timestamp = 1;
    MOTION(QPoint(5, 5));
    QCOMPARE(screenEdgeSpy.count(), 1);

    LOCK

    MOTION(QPoint(4, 4));
    QCOMPARE(screenEdgeSpy.count(), 1);

    // and unlock
    UNLOCK

    MOTION(QPoint(5, 5));
    QCOMPARE(screenEdgeSpy.count(), 2);
}

void LockScreenTest::testEffects()
{
    QScopedPointer<HelperEffect> effect(new HelperEffect);
    QSignalSpy inputSpy(effect.data(), &HelperEffect::inputEvent);
    QVERIFY(inputSpy.isValid());
    effects->startMouseInterception(effect.data(), Qt::ArrowCursor);

    quint32 timestamp = 1;
    QCOMPARE(inputSpy.count(), 0);
    MOTION(QPoint(5, 5));
    QCOMPARE(inputSpy.count(), 1);
    // simlate click
    PRESS;
    QCOMPARE(inputSpy.count(), 2);
    RELEASE;
    QCOMPARE(inputSpy.count(), 3);

    LOCK

    MOTION(QPoint(6, 6));
    QCOMPARE(inputSpy.count(), 3);
    // simlate click
    PRESS;
    QCOMPARE(inputSpy.count(), 3);
    RELEASE;
    QCOMPARE(inputSpy.count(), 3);

    UNLOCK

    MOTION(QPoint(5, 5));
    QCOMPARE(inputSpy.count(), 4);
    // simlate click
    PRESS;
    QCOMPARE(inputSpy.count(), 5);
    RELEASE;
    QCOMPARE(inputSpy.count(), 6);

    effects->stopMouseInterception(effect.data());
}

void LockScreenTest::testEffectsKeyboard()
{
    QScopedPointer<HelperEffect> effect(new HelperEffect);
    QSignalSpy inputSpy(effect.data(), &HelperEffect::keyEvent);
    QVERIFY(inputSpy.isValid());
    effects->grabKeyboard(effect.data());

    quint32 timestamp = 1;
    KEYPRESS(KEY_A);
    QCOMPARE(inputSpy.count(), 1);
    QCOMPARE(inputSpy.first().first().toString(), QStringLiteral("a"));
    KEYRELEASE(KEY_A);
    QCOMPARE(inputSpy.count(), 2);
    QCOMPARE(inputSpy.first().first().toString(), QStringLiteral("a"));
    QCOMPARE(inputSpy.at(1).first().toString(), QStringLiteral("a"));

    LOCK
    KEYPRESS(KEY_B);
    QCOMPARE(inputSpy.count(), 2);
    KEYRELEASE(KEY_B);
    QCOMPARE(inputSpy.count(), 2);

    UNLOCK
    KEYPRESS(KEY_C);
    QCOMPARE(inputSpy.count(), 3);
    QCOMPARE(inputSpy.first().first().toString(), QStringLiteral("a"));
    QCOMPARE(inputSpy.at(1).first().toString(), QStringLiteral("a"));
    QCOMPARE(inputSpy.at(2).first().toString(), QStringLiteral("c"));
    KEYRELEASE(KEY_C);
    QCOMPARE(inputSpy.count(), 4);
    QCOMPARE(inputSpy.first().first().toString(), QStringLiteral("a"));
    QCOMPARE(inputSpy.at(1).first().toString(), QStringLiteral("a"));
    QCOMPARE(inputSpy.at(2).first().toString(), QStringLiteral("c"));
    QCOMPARE(inputSpy.at(3).first().toString(), QStringLiteral("c"));

    effects->ungrabKeyboard();
}

void LockScreenTest::testEffectsKeyboardAutorepeat()
{
    // this test is just like testEffectsKeyboard, but tests auto repeat key events
    // while the key is pressed the Effect should get auto repeated events
    // but the lock screen should filter them out
    QScopedPointer<HelperEffect> effect(new HelperEffect);
    QSignalSpy inputSpy(effect.data(), &HelperEffect::keyEvent);
    QVERIFY(inputSpy.isValid());
    effects->grabKeyboard(effect.data());

    // we need to configure the key repeat first. It is only enabled on libinput
    waylandServer()->seat()->setKeyRepeatInfo(25, 300);

    quint32 timestamp = 1;
    KEYPRESS(KEY_A);
    QCOMPARE(inputSpy.count(), 1);
    QCOMPARE(inputSpy.first().first().toString(), QStringLiteral("a"));
    QVERIFY(inputSpy.wait());
    QVERIFY(inputSpy.count() > 1);
    // and still more events
    QVERIFY(inputSpy.wait());
    QCOMPARE(inputSpy.at(1).first().toString(), QStringLiteral("a"));

    // now release
    inputSpy.clear();
    KEYRELEASE(KEY_A);
    QCOMPARE(inputSpy.count(), 1);

    // while locked key repeat should not pass any events to the Effect
    LOCK
    KEYPRESS(KEY_B);
    QVERIFY(!inputSpy.wait(200));
    KEYRELEASE(KEY_B);
    QVERIFY(!inputSpy.wait(200));

    UNLOCK
    // don't test again, that's covered by testEffectsKeyboard

    effects->ungrabKeyboard();
}

void LockScreenTest::testMoveWindow()
{
    using namespace KWayland::Client;
    AbstractClient *c = showWindow();
    QVERIFY(c);
    QSignalSpy clientStepUserMovedResizedSpy(c, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    quint32 timestamp = 1;

    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), c);
    QVERIFY(c->isMove());
    kwinApp()->platform()->keyboardKeyPressed(KEY_RIGHT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    // TODO adjust once the expected fail is fixed
    kwinApp()->platform()->keyboardKeyPressed(KEY_RIGHT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    // while locking our window should continue to be in move resize
    LOCK
    QCOMPARE(workspace()->moveResizeClient(), c);
    QVERIFY(c->isMove());
    kwinApp()->platform()->keyboardKeyPressed(KEY_RIGHT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    UNLOCK
    QCOMPARE(workspace()->moveResizeClient(), c);
    QVERIFY(c->isMove());
    kwinApp()->platform()->keyboardKeyPressed(KEY_RIGHT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    kwinApp()->platform()->keyboardKeyPressed(KEY_ESC, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_ESC, timestamp++);
    QVERIFY(!c->isMove());
}

void LockScreenTest::testPointerShortcut()
{
    using namespace KWayland::Client;
    QScopedPointer<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.data(), &QAction::triggered);
    QVERIFY(actionSpy.isValid());
    input()->registerPointerShortcut(Qt::MetaModifier, Qt::LeftButton, action.data());

    // try to trigger the shortcut
    quint32 timestamp = 1;
#define PERFORM(expectedCount) \
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++); \
    PRESS; \
    QCoreApplication::instance()->processEvents(); \
    QCOMPARE(actionSpy.count(), expectedCount); \
    RELEASE; \
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++); \
    QCoreApplication::instance()->processEvents(); \
    QCOMPARE(actionSpy.count(), expectedCount);

    PERFORM(1)

    // now the same thing with a locked screen
    LOCK
    PERFORM(1)

    // and as unlocked
    UNLOCK
    PERFORM(2)
#undef PERFORM
}


void LockScreenTest::testAxisShortcut_data()
{
    QTest::addColumn<Qt::Orientation>("direction");
    QTest::addColumn<int>("sign");

    QTest::newRow("up") << Qt::Vertical << 1;
    QTest::newRow("down") << Qt::Vertical << -1;
    QTest::newRow("left") << Qt::Horizontal << 1;
    QTest::newRow("right") << Qt::Horizontal << -1;
}

void LockScreenTest::testAxisShortcut()
{
    using namespace KWayland::Client;
    QScopedPointer<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.data(), &QAction::triggered);
    QVERIFY(actionSpy.isValid());
    QFETCH(Qt::Orientation, direction);
    QFETCH(int, sign);
    PointerAxisDirection axisDirection = PointerAxisUp;
    if (direction == Qt::Vertical) {
        axisDirection = sign > 0 ? PointerAxisUp : PointerAxisDown;
    } else {
        axisDirection = sign > 0 ? PointerAxisLeft : PointerAxisRight;
    }
    input()->registerAxisShortcut(Qt::MetaModifier, axisDirection, action.data());

    // try to trigger the shortcut
    quint32 timestamp = 1;
#define PERFORM(expectedCount) \
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++); \
    if (direction == Qt::Vertical) \
        kwinApp()->platform()->pointerAxisVertical(sign * 5.0, timestamp++); \
    else \
        kwinApp()->platform()->pointerAxisHorizontal(sign * 5.0, timestamp++); \
    QCoreApplication::instance()->processEvents(); \
    QCOMPARE(actionSpy.count(), expectedCount); \
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++); \
    QCoreApplication::instance()->processEvents(); \
    QCOMPARE(actionSpy.count(), expectedCount);

    PERFORM(1)

    // now the same thing with a locked screen
    LOCK
    PERFORM(1)

    // and as unlocked
    UNLOCK
    PERFORM(2)
#undef PERFORM
}

void LockScreenTest::testKeyboardShortcut()
{
    using namespace KWayland::Client;
    QScopedPointer<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.data(), &QAction::triggered);
    QVERIFY(actionSpy.isValid());
    action->setProperty("componentName", QStringLiteral(KWIN_NAME));
    action->setObjectName("LockScreenTest::testKeyboardShortcut");
    KGlobalAccel::self()->setDefaultShortcut(action.data(), QList<QKeySequence>{Qt::CTRL + Qt::META + Qt::ALT + Qt::Key_Space});
    KGlobalAccel::self()->setShortcut(action.data(), QList<QKeySequence>{Qt::CTRL + Qt::META + Qt::ALT + Qt::Key_Space},
                                          KGlobalAccel::NoAutoloading);

    // try to trigger the shortcut
    quint32 timestamp = 1;
    KEYPRESS(KEY_LEFTCTRL);
    KEYPRESS(KEY_LEFTMETA);
    KEYPRESS(KEY_LEFTALT);
    KEYPRESS(KEY_SPACE);
    QVERIFY(actionSpy.wait());
    QCOMPARE(actionSpy.count(), 1);
    KEYRELEASE(KEY_SPACE);
    QVERIFY(!actionSpy.wait());
    QCOMPARE(actionSpy.count(), 1);

    LOCK
    KEYPRESS(KEY_SPACE);
    QVERIFY(!actionSpy.wait());
    QCOMPARE(actionSpy.count(), 1);
    KEYRELEASE(KEY_SPACE);
    QVERIFY(!actionSpy.wait());
    QCOMPARE(actionSpy.count(), 1);

    UNLOCK
    KEYPRESS(KEY_SPACE);
    QVERIFY(actionSpy.wait());
    QCOMPARE(actionSpy.count(), 2);
    KEYRELEASE(KEY_SPACE);
    QVERIFY(!actionSpy.wait());
    QCOMPARE(actionSpy.count(), 2);
    KEYRELEASE(KEY_LEFTCTRL);
    KEYRELEASE(KEY_LEFTMETA);
    KEYRELEASE(KEY_LEFTALT);
}

void LockScreenTest::testTouch()
{
    using namespace KWayland::Client;
    auto touch = m_seat->createTouch(m_seat);
    QVERIFY(touch);
    QVERIFY(touch->isValid());
    AbstractClient *c = showWindow();
    QVERIFY(c);
    QSignalSpy sequenceStartedSpy(touch, &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy cancelSpy(touch, &Touch::sequenceCanceled);
    QVERIFY(cancelSpy.isValid());
    QSignalSpy pointRemovedSpy(touch, &Touch::pointRemoved);
    QVERIFY(pointRemovedSpy.isValid());

    quint32 timestamp = 1;
    kwinApp()->platform()->touchDown(1, QPointF(25, 25), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    LOCK
    QVERIFY(cancelSpy.wait());

    kwinApp()->platform()->touchUp(1, timestamp++);
    QVERIFY(!pointRemovedSpy.wait(100));
    kwinApp()->platform()->touchDown(1, QPointF(25, 25), timestamp++);
    kwinApp()->platform()->touchMotion(1, QPointF(26, 26), timestamp++);
    kwinApp()->platform()->touchUp(1, timestamp++);

    UNLOCK
    kwinApp()->platform()->touchDown(1, QPointF(25, 25), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 2);
    kwinApp()->platform()->touchUp(1, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 1);
}

}

WAYLANDTEST_MAIN(KWin::LockScreenTest)
#include "lockscreen.moc"
