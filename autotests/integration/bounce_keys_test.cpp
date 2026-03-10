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

class BounceKeysTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testBounce();
    void testBounceKeyRepeat();

private:
    std::unique_ptr<Test::Connection> m_connection;
};

void BounceKeysTest::initTestCase()
{
    KConfig kaccessConfig("kaccessrc");
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("BounceKeys", true);
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("BounceKeysDelay", 200);
    kaccessConfig.sync();

    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(qAppName()));

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
        Rect(1280, 0, 1280, 1024),
    });
}

void BounceKeysTest::init()
{
    m_connection = Test::Connection::setup(Test::AdditionalWaylandInterface::Seat);
    QVERIFY(Test::waitForWaylandKeyboard(m_connection->seat));
}

void BounceKeysTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void BounceKeysTest::testBounce()
{
    const auto keyboard = m_connection->kwinSeat->getKeyboard();
    QVERIFY(keyboard);
    QSignalSpy enteredSpy(keyboard.get(), &Test::WlKeyboard::enter);
    QSignalSpy keySpy(keyboard.get(), &Test::WlKeyboard::key);

    Test::XdgToplevelWindow window{m_connection.get()};
    QVERIFY(window.show());
    QVERIFY(enteredSpy.wait());

    quint32 timestamp = 0;

    // Press a key, verify that it goes through
    Test::keyboardKeyPressed(KEY_A, timestamp);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_A);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_pressed);
    keySpy.clear();

    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_A);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_released);
    keySpy.clear();

    // Press it again within the bounce interval, verify that it does *not* go through
    timestamp += 100;
    Test::keyboardKeyPressed(KEY_A, timestamp);
    QVERIFY(!keySpy.wait(100));
    keySpy.clear();

    // Press it again after the bounce interval, verify that it does go through
    timestamp += 1000;
    Test::keyboardKeyPressed(KEY_A, timestamp);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_A);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_pressed);
    keySpy.clear();

    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_A);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_released);
    keySpy.clear();
}

void BounceKeysTest::testBounceKeyRepeat()
{
    const auto keyboard = m_connection->kwinSeat->getKeyboard();
    QVERIFY(keyboard);
    QSignalSpy enteredSpy(keyboard.get(), &Test::WlKeyboard::enter);
    QSignalSpy keySpy(keyboard.get(), &Test::WlKeyboard::key);

    Test::XdgToplevelWindow window{m_connection.get()};
    QVERIFY(window.show());
    QVERIFY(enteredSpy.wait());

    quint32 timestamp = 0;

    // Press and repeat a key within the bounce interval, make sure the repeat goes through
    Test::keyboardKeyPressed(KEY_B, timestamp);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_B);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_pressed);
    keySpy.clear();

    // wait for the repeat
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_B);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_repeated);
    keySpy.clear();

    Test::keyboardKeyReleased(KEY_B, timestamp++);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_B);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_released);
    keySpy.clear();

    // Press and repeat it again within the bounce interval, verify that the repeat does *not* go through
    timestamp += 100;
    Test::keyboardKeyPressed(KEY_B, timestamp++);
    QVERIFY(!keySpy.wait(100));
    keySpy.clear();

    // the repeat should get blocked
    QVERIFY(!keySpy.wait(100));
    keySpy.clear();

    // Press and repeat it again after the bounce interval, verify that it does go through
    timestamp += 1000;
    Test::keyboardKeyPressed(KEY_B, timestamp);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_B);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_pressed);
    keySpy.clear();

    // wait for the repeat again
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_B);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_repeated);
    keySpy.clear();

    Test::keyboardKeyReleased(KEY_B, timestamp++);
    QVERIFY(keySpy.wait());
    QCOMPARE(keySpy.first()[2], KEY_B);
    QCOMPARE(keySpy.first().last().value<Test::WlKeyboard::key_state>(), Test::WlKeyboard::key_state::key_state_released);
    keySpy.clear();
}

}

WAYLANDTEST_MAIN(KWin::BounceKeysTest)
#include "bounce_keys_test.moc"
