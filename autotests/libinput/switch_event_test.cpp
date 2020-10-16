/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_libinput.h"
#include "../../libinput/device.h"
#include "../../libinput/events.h"

#include <QtTest>

#include <memory>

Q_DECLARE_METATYPE(KWin::LibInput::SwitchEvent::State)

using namespace KWin::LibInput;

class TestLibinputSwitchEvent : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testToggled_data();
    void testToggled();

private:
    std::unique_ptr<libinput_device> m_nativeDevice;
    std::unique_ptr<Device> m_device;
};

void TestLibinputSwitchEvent::init()
{
    m_nativeDevice = std::make_unique<libinput_device>();
    m_nativeDevice->switchDevice = true;
    m_device = std::make_unique<Device>(m_nativeDevice.get());
}

void TestLibinputSwitchEvent::cleanup()
{
    m_device.reset();
    m_nativeDevice.reset();
}

void TestLibinputSwitchEvent::testToggled_data()
{
    QTest::addColumn<KWin::LibInput::SwitchEvent::State>("state");

    QTest::newRow("on") << KWin::LibInput::SwitchEvent::State::On;
    QTest::newRow("off") << KWin::LibInput::SwitchEvent::State::Off;
}

void TestLibinputSwitchEvent::testToggled()
{
    libinput_event_switch *nativeEvent = new libinput_event_switch;
    nativeEvent->type = LIBINPUT_EVENT_SWITCH_TOGGLE;
    nativeEvent->device = m_nativeDevice.get();
    QFETCH(KWin::LibInput::SwitchEvent::State, state);
    switch (state) {
    case SwitchEvent::State::Off:
        nativeEvent->state = libinput_event_switch::State::Off;
        break;
    case SwitchEvent::State::On:
        nativeEvent->state = libinput_event_switch::State::On;
        break;
    default:
        Q_UNREACHABLE();
    }
    nativeEvent->time = 23;
    nativeEvent->timeMicroseconds = 23456789;

    QScopedPointer<Event> event(Event::create(nativeEvent));
    auto se = dynamic_cast<SwitchEvent*>(event.data());
    QVERIFY(se);
    QCOMPARE(se->device(), m_device.get());
    QCOMPARE(se->nativeDevice(), m_nativeDevice.get());
    QCOMPARE(se->type(), LIBINPUT_EVENT_SWITCH_TOGGLE);
    QCOMPARE(se->state(), state);
    QCOMPARE(se->time(), 23u);
    QCOMPARE(se->timeMicroseconds(), 23456789u);
}

QTEST_GUILESS_MAIN(TestLibinputSwitchEvent)
#include "switch_event_test.moc"
