/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "keyboard_input.h"
#include "pluginmanager.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/seat.h>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_bounce_keys-0");

class BounceKeysTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testBounce();
};

void BounceKeysTest::initTestCase()
{
    KConfig kaccessConfig("kaccessrc");
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("BounceKeys", true);
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("BounceKeysDelay", 200);
    kaccessConfig.sync();

    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void BounceKeysTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void BounceKeysTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void BounceKeysTest::testBounce()
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

    // Press a key, verify that it goes through
    Test::keyboardKeyPressed(KEY_A, timestamp);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[0], KEY_A);
    QCOMPARE(keySpy.first()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    keySpy.clear();

    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[0], KEY_A);
    QCOMPARE(keySpy.first()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    keySpy.clear();

    // Press it again within the bounce interval, verify that it does *not* go through
    timestamp += 100;
    Test::keyboardKeyPressed(KEY_A, timestamp);
    QVERIFY(!keySpy.wait(100));
    keySpy.clear();

    // Press it again after the bouce interval, verify that it does go through
    timestamp += 1000;
    Test::keyboardKeyPressed(KEY_A, timestamp);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[0], KEY_A);
    QCOMPARE(keySpy.first()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    keySpy.clear();

    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[0], KEY_A);
    QCOMPARE(keySpy.first()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    keySpy.clear();
}
}

WAYLANDTEST_MAIN(KWin::BounceKeysTest)
#include "bounce_keys_test.moc"
