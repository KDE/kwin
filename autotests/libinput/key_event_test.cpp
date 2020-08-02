/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_libinput.h"
#include "../../libinput/device.h"
#include "../../libinput/events.h"

#include <QtTest>

#include <linux/input.h>

Q_DECLARE_METATYPE(libinput_key_state)

using namespace KWin::LibInput;

class TestLibinputKeyEvent : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testEvent_data();
    void testEvent();

private:
    libinput_device *m_nativeDevice = nullptr;
    Device *m_device = nullptr;
};

void TestLibinputKeyEvent::init()
{
    m_nativeDevice = new libinput_device;
    m_nativeDevice->keyboard = true;
    m_device = new Device(m_nativeDevice);
}

void TestLibinputKeyEvent::cleanup()
{
    delete m_device;
    m_device = nullptr;

    delete m_nativeDevice;
    m_nativeDevice = nullptr;
}

void TestLibinputKeyEvent::testCreate()
{
    // this test verifies the initialisation of a KeyEvent and the parent Event class
    libinput_event_keyboard *keyEvent = new libinput_event_keyboard;
    keyEvent->device = m_nativeDevice;

    QScopedPointer<Event> event(Event::create(keyEvent));
    // API of event
    QCOMPARE(event->type(), LIBINPUT_EVENT_KEYBOARD_KEY);
    QCOMPARE(event->device(), m_device);
    QCOMPARE(event->nativeDevice(), m_nativeDevice);
    QCOMPARE((libinput_event*)(*event.data()), keyEvent);
    // verify it's a key event
    QVERIFY(dynamic_cast<KeyEvent*>(event.data()));
    QCOMPARE((libinput_event_keyboard*)(*dynamic_cast<KeyEvent*>(event.data())), keyEvent);

    // verify that a nullptr passed to Event::create returns a nullptr
    QVERIFY(!Event::create(nullptr));
}

void TestLibinputKeyEvent::testEvent_data()
{
    QTest::addColumn<libinput_key_state>("keyState");
    QTest::addColumn<KWin::InputRedirection::KeyboardKeyState>("expectedKeyState");
    QTest::addColumn<quint32>("key");
    QTest::addColumn<quint32>("time");

    QTest::newRow("pressed") << LIBINPUT_KEY_STATE_PRESSED << KWin::InputRedirection::KeyboardKeyPressed << quint32(KEY_A) << 100u;
    QTest::newRow("released") << LIBINPUT_KEY_STATE_RELEASED << KWin::InputRedirection::KeyboardKeyReleased << quint32(KEY_B) << 200u;
}

void TestLibinputKeyEvent::testEvent()
{
    // this test verifies the key press/release
    libinput_event_keyboard *keyEvent = new libinput_event_keyboard;
    keyEvent->device = m_nativeDevice;
    QFETCH(libinput_key_state, keyState);
    keyEvent->state = keyState;
    QFETCH(quint32, key);
    keyEvent->key = key;
    QFETCH(quint32, time);
    keyEvent->time = time;

    QScopedPointer<Event> event(Event::create(keyEvent));
    auto ke = dynamic_cast<KeyEvent*>(event.data());
    QVERIFY(ke);
    QTEST(ke->state(), "expectedKeyState");
    QCOMPARE(ke->key(), key);
    QCOMPARE(ke->time(), time);
}

QTEST_GUILESS_MAIN(TestLibinputKeyEvent)
#include "key_event_test.moc"
