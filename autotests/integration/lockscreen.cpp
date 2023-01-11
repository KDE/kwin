/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "composite.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "cursor.h"
#include "screenedge.h"
#include "wayland/keyboard_interface.h"
#include "wayland/seat_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include <kwineffects.h>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

// screenlocker
#include <KScreenLocker/KsldApp>

#include <KGlobalAccel>

#include <QAction>

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
    std::pair<Window *, std::unique_ptr<KWayland::Client::Surface>> showWindow();
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
};

class HelperEffect : public Effect
{
    Q_OBJECT
public:
    HelperEffect()
    {
    }
    ~HelperEffect() override
    {
    }

    void windowInputMouseEvent(QEvent *) override
    {
        Q_EMIT inputEvent();
    }
    void grabbedKeyboardEvent(QKeyEvent *e) override
    {
        Q_EMIT keyEvent(e->text());
    }

Q_SIGNALS:
    void inputEvent();
    void keyEvent(const QString &);
};

#define LOCK                                                                                                     \
    do {                                                                                                         \
        QVERIFY(!waylandServer()->isScreenLocked());                                                             \
        QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged); \
        ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);                             \
        QTRY_COMPARE(ScreenLocker::KSldApp::self()->lockState(), ScreenLocker::KSldApp::Locked);                 \
        QVERIFY(waylandServer()->isScreenLocked());                                                              \
    } while (false)

#define UNLOCK                                                                                                   \
    do {                                                                                                         \
        QSignalSpy lockStateChangedSpy(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged); \
        unlock();                                                                                                \
        if (lockStateChangedSpy.count() != 1) {                                                                  \
            QVERIFY(lockStateChangedSpy.wait());                                                                 \
        }                                                                                                        \
        QCOMPARE(lockStateChangedSpy.count(), 1);                                                                \
        QVERIFY(!waylandServer()->isScreenLocked());                                                             \
    } while (false)

#define MOTION(target) Test::pointerMotion(target, timestamp++)

#define PRESS Test::pointerButtonPressed(BTN_LEFT, timestamp++)

#define RELEASE Test::pointerButtonReleased(BTN_LEFT, timestamp++)

#define KEYPRESS(key) Test::keyboardKeyPressed(key, timestamp++)

#define KEYRELEASE(key) Test::keyboardKeyReleased(key, timestamp++)

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

std::pair<Window *, std::unique_ptr<KWayland::Client::Surface>> LockScreenTest::showWindow()
{
#define VERIFY(statement)                                                 \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__)) \
        return {nullptr, nullptr};
#define COMPARE(actual, expected)                                                   \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
        return {nullptr, nullptr};

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    VERIFY(surface.get());
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    VERIFY(shellSurface);
    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    VERIFY(window);
    COMPARE(workspace()->activeWindow(), window);

#undef VERIFY
#undef COMPARE

    return {window, std::move(surface)};
}

void LockScreenTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::ElectricBorder>("ElectricBorder");

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);

    QCOMPARE(Compositor::self()->backend()->compositingType(), KWin::OpenGLCompositing);
}

void LockScreenTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());
    m_connection = Test::waylandConnection();
    m_compositor = Test::waylandCompositor();
    m_shm = Test::waylandShmPool();
    m_seat = Test::waylandSeat();

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void LockScreenTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void LockScreenTest::testStackingOrder()
{
    // This test verifies that the lockscreen greeter is placed above other windows.

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);

    LOCK;
    QVERIFY(windowAddedSpy.wait());

    Window *window = windowAddedSpy.first().first().value<Window *>();
    QVERIFY(window);
    QVERIFY(window->isLockScreen());
    QCOMPARE(window->layer(), UnmanagedLayer);

    UNLOCK;
}

void LockScreenTest::testPointer()
{
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());
    QVERIFY(pointer != nullptr);
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer.get(), &KWayland::Client::Pointer::left);

    auto [window, surface] = showWindow();
    QVERIFY(window);

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    MOTION(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());

    LOCK;

    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);

    // simulate moving out in and out again
    MOTION(window->frameGeometry().center());
    MOTION(window->frameGeometry().bottomRight() + QPoint(100, 100));
    MOTION(window->frameGeometry().bottomRight() + QPoint(100, 100));
    QVERIFY(!leftSpy.wait(10));
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(enteredSpy.count(), 1);

    // go back on the window
    MOTION(window->frameGeometry().center());
    // and unlock
    UNLOCK;

    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    // move on the window
    MOTION(window->frameGeometry().center() + QPoint(100, 100));
    QVERIFY(leftSpy.wait());
    MOTION(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 3);
}

void LockScreenTest::testPointerButton()
{
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());
    QVERIFY(pointer != nullptr);
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy buttonChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);

    auto [window, surface] = showWindow();
    QVERIFY(window);

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    MOTION(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // and simulate a click
    PRESS;
    QVERIFY(buttonChangedSpy.wait());
    RELEASE;
    QVERIFY(buttonChangedSpy.wait());

    LOCK;

    // and simulate a click
    PRESS;
    QVERIFY(!buttonChangedSpy.wait(10));
    RELEASE;
    QVERIFY(!buttonChangedSpy.wait(10));

    UNLOCK;
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
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());
    QVERIFY(pointer != nullptr);
    QSignalSpy axisChangedSpy(pointer.get(), &KWayland::Client::Pointer::axisChanged);
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);

    auto [window, surface] = showWindow();
    QVERIFY(window);

    // first move cursor into the center of the window
    quint32 timestamp = 1;
    MOTION(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // and simulate axis
    Test::pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());

    LOCK;

    // and simulate axis
    Test::pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(!axisChangedSpy.wait(10));
    Test::pointerAxisVertical(5.0, timestamp++);
    QVERIFY(!axisChangedSpy.wait(10));

    // and unlock
    UNLOCK;
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);

    // and move axis again
    Test::pointerAxisHorizontal(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());
    Test::pointerAxisVertical(5.0, timestamp++);
    QVERIFY(axisChangedSpy.wait());
}

void LockScreenTest::testKeyboard()
{
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard != nullptr);
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy leftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    auto [window, surface] = showWindow();
    QVERIFY(window);
    QVERIFY(enteredSpy.wait());
    QTRY_COMPARE(enteredSpy.count(), 1);

    quint32 timestamp = 1;
    KEYPRESS(KEY_A);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 1);
    QCOMPARE(keyChangedSpy.at(0).at(0).value<quint32>(), quint32(KEY_A));
    QCOMPARE(keyChangedSpy.at(0).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    QCOMPARE(keyChangedSpy.at(0).at(2).value<quint32>(), quint32(1));
    KEYRELEASE(KEY_A);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 2);
    QCOMPARE(keyChangedSpy.at(1).at(0).value<quint32>(), quint32(KEY_A));
    QCOMPARE(keyChangedSpy.at(1).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(1).at(2).value<quint32>(), quint32(2));

    LOCK;
    QVERIFY(leftSpy.wait());
    KEYPRESS(KEY_B);
    KEYRELEASE(KEY_B);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(keyChangedSpy.count(), 2);

    UNLOCK;
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
    QCOMPARE(keyChangedSpy.at(2).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    QCOMPARE(keyChangedSpy.at(3).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
}

void LockScreenTest::testScreenEdge()
{
    QSignalSpy screenEdgeSpy(workspace()->screenEdges(), &ScreenEdges::approaching);
    QCOMPARE(screenEdgeSpy.count(), 0);

    quint32 timestamp = 1;
    MOTION(QPoint(5, 5));
    QCOMPARE(screenEdgeSpy.count(), 1);

    LOCK;

    MOTION(QPoint(4, 4));
    QCOMPARE(screenEdgeSpy.count(), 1);

    // and unlock
    UNLOCK;

    MOTION(QPoint(5, 5));
    QCOMPARE(screenEdgeSpy.count(), 2);
}

void LockScreenTest::testEffects()
{
    std::unique_ptr<HelperEffect> effect(new HelperEffect);
    QSignalSpy inputSpy(effect.get(), &HelperEffect::inputEvent);
    effects->startMouseInterception(effect.get(), Qt::ArrowCursor);

    quint32 timestamp = 1;
    QCOMPARE(inputSpy.count(), 0);
    MOTION(QPoint(5, 5));
    QCOMPARE(inputSpy.count(), 1);
    // simlate click
    PRESS;
    QCOMPARE(inputSpy.count(), 2);
    RELEASE;
    QCOMPARE(inputSpy.count(), 3);

    LOCK;

    MOTION(QPoint(6, 6));
    QCOMPARE(inputSpy.count(), 3);
    // simlate click
    PRESS;
    QCOMPARE(inputSpy.count(), 3);
    RELEASE;
    QCOMPARE(inputSpy.count(), 3);

    UNLOCK;

    MOTION(QPoint(5, 5));
    QCOMPARE(inputSpy.count(), 4);
    // simlate click
    PRESS;
    QCOMPARE(inputSpy.count(), 5);
    RELEASE;
    QCOMPARE(inputSpy.count(), 6);

    effects->stopMouseInterception(effect.get());
}

void LockScreenTest::testEffectsKeyboard()
{
    std::unique_ptr<HelperEffect> effect(new HelperEffect);
    QSignalSpy inputSpy(effect.get(), &HelperEffect::keyEvent);
    effects->grabKeyboard(effect.get());

    quint32 timestamp = 1;
    KEYPRESS(KEY_A);
    QCOMPARE(inputSpy.count(), 1);
    QCOMPARE(inputSpy.first().first().toString(), QStringLiteral("a"));
    KEYRELEASE(KEY_A);
    QCOMPARE(inputSpy.count(), 2);
    QCOMPARE(inputSpy.first().first().toString(), QStringLiteral("a"));
    QCOMPARE(inputSpy.at(1).first().toString(), QStringLiteral("a"));

    LOCK;
    KEYPRESS(KEY_B);
    QCOMPARE(inputSpy.count(), 2);
    KEYRELEASE(KEY_B);
    QCOMPARE(inputSpy.count(), 2);

    UNLOCK;
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
    std::unique_ptr<HelperEffect> effect(new HelperEffect);
    QSignalSpy inputSpy(effect.get(), &HelperEffect::keyEvent);
    effects->grabKeyboard(effect.get());

    // we need to configure the key repeat first. It is only enabled on libinput
    waylandServer()->seat()->keyboard()->setRepeatInfo(25, 300);

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
    LOCK;
    KEYPRESS(KEY_B);
    QVERIFY(!inputSpy.wait(10));
    KEYRELEASE(KEY_B);
    QVERIFY(!inputSpy.wait(10));

    UNLOCK;
    // don't test again, that's covered by testEffectsKeyboard

    effects->ungrabKeyboard();
}

void LockScreenTest::testMoveWindow()
{
    auto [window, surface] = showWindow();
    QVERIFY(window);
    QSignalSpy clientStepUserMovedResizedSpy(window, &Window::clientStepUserMovedResized);
    quint32 timestamp = 1;

    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QVERIFY(window->isInteractiveMove());
    Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
    Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    // TODO adjust once the expected fail is fixed
    Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
    Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    // while locking our window should continue to be in move resize
    LOCK;
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QVERIFY(window->isInteractiveMove());
    Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
    Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    UNLOCK;
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QVERIFY(window->isInteractiveMove());
    Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
    Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    Test::keyboardKeyPressed(KEY_ESC, timestamp++);
    Test::keyboardKeyReleased(KEY_ESC, timestamp++);
    QVERIFY(!window->isInteractiveMove());
}

void LockScreenTest::testPointerShortcut()
{
    std::unique_ptr<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.get(), &QAction::triggered);
    input()->registerPointerShortcut(Qt::MetaModifier, Qt::LeftButton, action.get());

    // try to trigger the shortcut
    quint32 timestamp = 1;
#define PERFORM(expectedCount)                                \
    do {                                                      \
        Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);  \
        PRESS;                                                \
        QCoreApplication::instance()->processEvents();        \
        QCOMPARE(actionSpy.count(), expectedCount);           \
        RELEASE;                                              \
        Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++); \
        QCoreApplication::instance()->processEvents();        \
        QCOMPARE(actionSpy.count(), expectedCount);           \
    } while (false)

    PERFORM(1);

    // now the same thing with a locked screen
    LOCK;
    PERFORM(1);

    // and as unlocked
    UNLOCK;
    PERFORM(2);
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
    std::unique_ptr<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.get(), &QAction::triggered);
    QFETCH(Qt::Orientation, direction);
    QFETCH(int, sign);
    PointerAxisDirection axisDirection = PointerAxisUp;
    if (direction == Qt::Vertical) {
        axisDirection = sign > 0 ? PointerAxisUp : PointerAxisDown;
    } else {
        axisDirection = sign > 0 ? PointerAxisLeft : PointerAxisRight;
    }
    input()->registerAxisShortcut(Qt::MetaModifier, axisDirection, action.get());

    // try to trigger the shortcut
    quint32 timestamp = 1;
#define PERFORM(expectedCount)                                    \
    do {                                                          \
        Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);      \
        if (direction == Qt::Vertical)                            \
            Test::pointerAxisVertical(sign * 5.0, timestamp++);   \
        else                                                      \
            Test::pointerAxisHorizontal(sign * 5.0, timestamp++); \
        QCoreApplication::instance()->processEvents();            \
        QCOMPARE(actionSpy.count(), expectedCount);               \
        Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);     \
        QCoreApplication::instance()->processEvents();            \
        QCOMPARE(actionSpy.count(), expectedCount);               \
    } while (false)

    PERFORM(1);

    // now the same thing with a locked screen
    LOCK;
    PERFORM(1);

    // and as unlocked
    UNLOCK;
    PERFORM(2);
#undef PERFORM
}

void LockScreenTest::testKeyboardShortcut()
{
    std::unique_ptr<QAction> action(new QAction(nullptr));
    QSignalSpy actionSpy(action.get(), &QAction::triggered);
    action->setProperty("componentName", QStringLiteral("kwin"));
    action->setObjectName("LockScreenTest::testKeyboardShortcut");
    KGlobalAccel::self()->setDefaultShortcut(action.get(), QList<QKeySequence>{Qt::CTRL | Qt::META | Qt::ALT | Qt::Key_Space});
    KGlobalAccel::self()->setShortcut(action.get(), QList<QKeySequence>{Qt::CTRL | Qt::META | Qt::ALT | Qt::Key_Space},
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
    QVERIFY(!actionSpy.wait(10));
    QCOMPARE(actionSpy.count(), 1);

    LOCK;
    KEYPRESS(KEY_SPACE);
    QVERIFY(!actionSpy.wait(10));
    QCOMPARE(actionSpy.count(), 1);
    KEYRELEASE(KEY_SPACE);
    QVERIFY(!actionSpy.wait(10));
    QCOMPARE(actionSpy.count(), 1);

    UNLOCK;
    KEYPRESS(KEY_SPACE);
    QVERIFY(actionSpy.wait());
    QCOMPARE(actionSpy.count(), 2);
    KEYRELEASE(KEY_SPACE);
    QVERIFY(!actionSpy.wait(10));
    QCOMPARE(actionSpy.count(), 2);
    KEYRELEASE(KEY_LEFTCTRL);
    KEYRELEASE(KEY_LEFTMETA);
    KEYRELEASE(KEY_LEFTALT);
}

void LockScreenTest::testTouch()
{
    auto touch = m_seat->createTouch(m_seat);
    QVERIFY(touch);
    QVERIFY(touch->isValid());
    auto [window, surface] = showWindow();
    QVERIFY(window);
    QSignalSpy sequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy cancelSpy(touch, &KWayland::Client::Touch::sequenceCanceled);
    QSignalSpy pointRemovedSpy(touch, &KWayland::Client::Touch::pointRemoved);

    quint32 timestamp = 1;
    Test::touchDown(1, QPointF(25, 25), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    LOCK;
    QVERIFY(cancelSpy.wait());

    Test::touchUp(1, timestamp++);
    QVERIFY(!pointRemovedSpy.wait(10));
    Test::touchDown(1, QPointF(25, 25), timestamp++);
    Test::touchMotion(1, QPointF(26, 26), timestamp++);
    Test::touchUp(1, timestamp++);

    UNLOCK;
    Test::touchDown(1, QPointF(25, 25), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 2);
    Test::touchUp(1, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 1);
}

}

WAYLANDTEST_MAIN(KWin::LockScreenTest)
#include "lockscreen.moc"
