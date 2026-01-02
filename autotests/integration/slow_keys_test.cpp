/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Martin Riethmayer <ripper@freakmail.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "keyboard_input.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/seat.h>

#include <linux/input.h>
#include <qtestsupport_core.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_slow_keys-0");

class SlowKeysTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testSlow();
};

void SlowKeysTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    KConfig kaccessConfig("kaccessrc");
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("SlowKeys", true);
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("SlowKeysDelay", 500);
    kaccessConfig.sync();

    qRegisterMetaType<Window *>();
    QVERIFY(waylandServer()->init(s_socketName));

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
}

void SlowKeysTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void SlowKeysTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void SlowKeysTest::testSlow()
{
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(10, 10), Qt::blue);
    QVERIFY(waylandWindow);

    QVERIFY(keyboard);
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QVERIFY(enteredSpy.wait());

    QSignalSpy keySpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(keySpy.isValid());

    quint32 timestamp = 0;

    // Press a key, verify that it does *not* go through with a time less than the interval...
    Test::keyboardKeyPressed(KEY_A, timestamp);
    QVERIFY(!keySpy.wait(250));
    // Since we rejected the "key pressed" event, we should *not* get a "key released" event
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(!keySpy.wait(750));
    keySpy.clear();

    // Press a key, verify that it *does* go through with a time greater than the interval
    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(keySpy.wait(750));
    QCOMPARE(keySpy.first()[0], KEY_A);
    QCOMPARE(keySpy.first()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    keySpy.clear();

    // Since we accepted the "key pressed" event, we *should* get a "key released" event
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[0], KEY_A);
    QCOMPARE(keySpy.first()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    keySpy.clear();
}
}

WAYLANDTEST_MAIN(KWin::SlowKeysTest)
#include "slow_keys_test.moc"
