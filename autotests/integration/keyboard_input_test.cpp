/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "config-kwin.h"

#include "wayland_server.h"
#include "workspace.h"

#if KWIN_BUILD_GLOBALSHORTCUTS
#include <KGlobalAccel>
#endif

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/seat.h>

#include <linux/input-event-codes.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_kbd_input-0");

class KeyboardInputTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void implicitGrab();
    void implicitGrabByClosedWindow();
    void globalShortcut();

private:
    std::unique_ptr<Test::Connection> m_firstConnection;
    std::unique_ptr<KWayland::Client::Keyboard> m_firstKeyboard;
    std::unique_ptr<KWayland::Client::Surface> m_firstSurface;
    std::unique_ptr<Test::XdgToplevel> m_firstShellSurface;
    QPointer<Window> m_firstWindow;

    std::unique_ptr<Test::Connection> m_secondConnection;
    std::unique_ptr<KWayland::Client::Keyboard> m_secondKeyboard;
    std::unique_ptr<KWayland::Client::Surface> m_secondSurface;
    std::unique_ptr<Test::XdgToplevel> m_secondShellSurface;
    QPointer<Window> m_secondWindow;
};

void KeyboardInputTest::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
}

void KeyboardInputTest::init()
{
    m_firstConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat);
    QVERIFY(Test::waitForWaylandKeyboard(m_firstConnection->seat));
    m_firstKeyboard = std::unique_ptr<KWayland::Client::Keyboard>(m_firstConnection->seat->createKeyboard());
    m_firstSurface = Test::createSurface(m_firstConnection->compositor);
    m_firstShellSurface = Test::createXdgToplevelSurface(m_firstConnection->xdgShell, m_firstSurface.get());
    m_firstWindow = Test::renderAndWaitForShown(m_firstConnection->shm, m_firstSurface.get(), QSize(100, 100), Qt::cyan);
    QSignalSpy firstEnteredSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QVERIFY(firstEnteredSpy.wait());

    m_secondConnection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat);
    QVERIFY(Test::waitForWaylandKeyboard(m_secondConnection->seat));
    m_secondKeyboard = std::unique_ptr<KWayland::Client::Keyboard>(m_secondConnection->seat->createKeyboard());
    m_secondSurface = Test::createSurface(m_secondConnection->compositor);
    m_secondShellSurface = Test::createXdgToplevelSurface(m_secondConnection->xdgShell, m_secondSurface.get());
    m_secondWindow = Test::renderAndWaitForShown(m_secondConnection->shm, m_secondSurface.get(), QSize(100, 100), Qt::cyan);
    QSignalSpy secondEnteredSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QVERIFY(secondEnteredSpy.wait());
}

void KeyboardInputTest::cleanup()
{
    m_firstShellSurface.reset();
    m_firstSurface.reset();
    m_firstKeyboard.reset();
    m_firstConnection.reset();

    m_secondShellSurface.reset();
    m_secondSurface.reset();
    m_secondKeyboard.reset();
    m_secondConnection.reset();
}

void KeyboardInputTest::implicitGrab()
{
    QSignalSpy firstEnteredSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy firstKeyChangedSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QSignalSpy secondEnteredSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy secondKeyChangedSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_Q, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());
    QCOMPARE(secondKeyChangedSpy.last().at(0).value<quint32>(), KEY_Q);
    QCOMPARE(secondKeyChangedSpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);

    workspace()->activateWindow(m_firstWindow);
    QVERIFY(firstEnteredSpy.wait()); // TODO: perhaps we should not receive the enter event until the q key is released
    QCOMPARE(m_firstKeyboard->enteredKeys(), (QList<quint32>{KEY_Q}));

    Test::keyboardKeyReleased(KEY_Q, timestamp++);
    QVERIFY(firstKeyChangedSpy.wait());
    QCOMPARE(firstKeyChangedSpy.last().at(0).value<quint32>(), KEY_Q);
    QCOMPARE(firstKeyChangedSpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
}

void KeyboardInputTest::implicitGrabByClosedWindow()
{
    // This test verifies that an implicit grab is preserved even after the window is closed. Note:
    // currently it is not the case, but it should be.

    QSignalSpy firstEnteredSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy firstKeyChangedSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QSignalSpy secondEnteredSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy secondKeyChangedSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_Q, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());
    QCOMPARE(secondKeyChangedSpy.last().at(0).value<quint32>(), KEY_Q);
    QCOMPARE(secondKeyChangedSpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);

    m_secondShellSurface.reset();
    m_secondSurface.reset();
    QVERIFY(firstEnteredSpy.wait()); // TODO: perhaps we should not receive the enter event until the q key is released
    QCOMPARE(m_firstKeyboard->enteredKeys(), (QList<quint32>{KEY_Q}));

    Test::keyboardKeyReleased(KEY_Q, timestamp++);
    QVERIFY(firstKeyChangedSpy.wait());
    QCOMPARE(firstKeyChangedSpy.last().at(0).value<quint32>(), KEY_Q);
    QCOMPARE(firstKeyChangedSpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
}

void KeyboardInputTest::globalShortcut()
{
    // This test verifies that keys are not leaked to the clients when pressing a global shortcut.

#if KWIN_BUILD_GLOBALSHORTCUTS
    QSignalSpy firstEnteredSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy firstKeyChangedSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QSignalSpy secondEnteredSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy secondKeyChangedSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    auto action = std::make_unique<QAction>();
    action->setObjectName(QStringLiteral("test"));
    action->setProperty("componentName", QStringLiteral("test"));
    KGlobalAccel::self()->setShortcut(action.get(), QList<QKeySequence>{Qt::META | Qt::Key_Space}, KGlobalAccel::NoAutoloading);
    QSignalSpy actionTriggeredSpy(action.get(), &QAction::triggered);

    // the client should not see the space key being pressed or released
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());
    Test::keyboardKeyPressed(KEY_SPACE, timestamp++);
    QVERIFY(!secondKeyChangedSpy.wait(10));
    QCOMPARE(actionTriggeredSpy.count(), 1);
    Test::keyboardKeyReleased(KEY_SPACE, timestamp++);
    QVERIFY(!secondKeyChangedSpy.wait(10));
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());

    // the space key should not be leaked even if the focused surface changes between pressing and releasing the space key
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());
    Test::keyboardKeyPressed(KEY_SPACE, timestamp++);
    QVERIFY(!secondKeyChangedSpy.wait(10));
    QCOMPARE(actionTriggeredSpy.count(), 2);
    m_secondShellSurface.reset();
    m_secondSurface.reset();
    QVERIFY(firstEnteredSpy.wait());
    QCOMPARE(m_firstKeyboard->enteredKeys(), (QList<quint32>{KEY_LEFTMETA}));
    Test::keyboardKeyReleased(KEY_SPACE, timestamp++);
    QVERIFY(!firstKeyChangedSpy.wait(10));
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(firstKeyChangedSpy.wait());
#endif
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::KeyboardInputTest)
#include "keyboard_input_test.moc"
