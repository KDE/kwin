/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_libinput.h"

#include "backends/libinput/device.h"
#include "backends/libinput/events.h"

#include <QtTest>

#include <linux/input.h>

Q_DECLARE_METATYPE(libinput_event_type)
Q_DECLARE_METATYPE(libinput_button_state)

using namespace KWin::LibInput;
using namespace std::literals;

class TestLibinputPointerEvent : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testType_data();
    void testType();
    void testButton_data();
    void testButton();
    void testScrollWheel_data();
    void testScrollWheel();
    void testScrollFinger_data();
    void testScrollFinger();
    void testScrollContinuous_data();
    void testScrollContinuous();
    void testMotion();
    void testAbsoluteMotion();

private:
    libinput_device *m_nativeDevice = nullptr;
    Device *m_device = nullptr;
};

void TestLibinputPointerEvent::init()
{
    m_nativeDevice = new libinput_device;
    m_nativeDevice->pointer = true;
    m_nativeDevice->deviceSize = QSizeF(12.5, 13.8);
    m_device = new Device(m_nativeDevice);
}

void TestLibinputPointerEvent::cleanup()
{
    delete m_device;
    m_device = nullptr;

    delete m_nativeDevice;
    m_nativeDevice = nullptr;
}

void TestLibinputPointerEvent::testType_data()
{
    QTest::addColumn<libinput_event_type>("type");

    QTest::newRow("motion") << LIBINPUT_EVENT_POINTER_MOTION;
    QTest::newRow("absolute motion") << LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE;
    QTest::newRow("button") << LIBINPUT_EVENT_POINTER_BUTTON;
    QTest::newRow("scroll wheel") << LIBINPUT_EVENT_POINTER_SCROLL_WHEEL;
    QTest::newRow("scroll finger") << LIBINPUT_EVENT_POINTER_SCROLL_FINGER;
    QTest::newRow("scroll continuous") << LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS;
}

void TestLibinputPointerEvent::testType()
{
    // this test verifies the initialization of a PointerEvent and the parent Event class
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    QFETCH(libinput_event_type, type);
    pointerEvent->type = type;
    pointerEvent->device = m_nativeDevice;

    std::unique_ptr<Event> event(Event::create(pointerEvent));
    // API of event
    QCOMPARE(event->type(), type);
    QCOMPARE(event->device(), m_device);
    QCOMPARE(event->nativeDevice(), m_nativeDevice);
    QCOMPARE((libinput_event *)(*event.get()), pointerEvent);
    // verify it's a pointer event
    QVERIFY(dynamic_cast<PointerEvent *>(event.get()));
    QCOMPARE((libinput_event_pointer *)(*dynamic_cast<PointerEvent *>(event.get())), pointerEvent);
}

void TestLibinputPointerEvent::testButton_data()
{
    QTest::addColumn<libinput_button_state>("buttonState");
    QTest::addColumn<KWin::InputRedirection::PointerButtonState>("expectedButtonState");
    QTest::addColumn<quint32>("button");
    QTest::addColumn<quint32>("time");

    QTest::newRow("pressed") << LIBINPUT_BUTTON_STATE_RELEASED << KWin::InputRedirection::PointerButtonReleased << quint32(BTN_RIGHT) << 100u;
    QTest::newRow("released") << LIBINPUT_BUTTON_STATE_PRESSED << KWin::InputRedirection::PointerButtonPressed << quint32(BTN_LEFT) << 200u;
}

void TestLibinputPointerEvent::testButton()
{
    // this test verifies the button press/release
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_BUTTON;
    QFETCH(libinput_button_state, buttonState);
    pointerEvent->buttonState = buttonState;
    QFETCH(quint32, button);
    pointerEvent->button = button;
    QFETCH(quint32, time);
    pointerEvent->time = std::chrono::milliseconds(time);

    std::unique_ptr<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent *>(event.get());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_BUTTON);
    QTEST(pe->buttonState(), "expectedButtonState");
    QCOMPARE(pe->button(), button);
    QCOMPARE(pe->time(), pointerEvent->time);
}

void TestLibinputPointerEvent::testScrollWheel_data()
{
    QTest::addColumn<bool>("horizontal");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<QPoint>("valueV120");
    QTest::addColumn<quint32>("time");

    QTest::newRow("wheel/horizontal") << true << false << QPointF(3.0, 0.0) << QPoint(120, 0) << 100u;
    QTest::newRow("wheel/vertical") << false << true << QPointF(0.0, 2.5) << QPoint(0, 120) << 200u;
    QTest::newRow("wheel/both") << true << true << QPointF(1.1, 4.2) << QPoint(120, 120) << 300u;
}

void TestLibinputPointerEvent::testScrollWheel()
{
    // this test verifies pointer axis functionality
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_SCROLL_WHEEL;
    QFETCH(bool, horizontal);
    QFETCH(bool, vertical);
    QFETCH(QPointF, value);
    QFETCH(QPoint, valueV120);
    QFETCH(quint32, time);
    pointerEvent->horizontalAxis = horizontal;
    pointerEvent->verticalAxis = vertical;
    pointerEvent->horizontalScrollValue = value.x();
    pointerEvent->verticalScrollValue = value.y();
    pointerEvent->horizontalScrollValueV120 = valueV120.x();
    pointerEvent->verticalScrollValueV120 = valueV120.y();
    pointerEvent->time = std::chrono::milliseconds(time);

    std::unique_ptr<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent *>(event.get());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_SCROLL_WHEEL);
    QCOMPARE(pe->axis().contains(KWin::InputRedirection::PointerAxisHorizontal), horizontal);
    QCOMPARE(pe->axis().contains(KWin::InputRedirection::PointerAxisVertical), vertical);
    QCOMPARE(pe->scrollValue(KWin::InputRedirection::PointerAxisHorizontal), value.x());
    QCOMPARE(pe->scrollValue(KWin::InputRedirection::PointerAxisVertical), value.y());
    QCOMPARE(pe->scrollValueV120(KWin::InputRedirection::PointerAxisHorizontal), valueV120.x());
    QCOMPARE(pe->scrollValueV120(KWin::InputRedirection::PointerAxisVertical), valueV120.y());
    QCOMPARE(pe->time(), pointerEvent->time);
}

void TestLibinputPointerEvent::testScrollFinger_data()
{
    QTest::addColumn<bool>("horizontal");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<quint32>("time");

    QTest::newRow("finger/horizontal") << true << false << QPointF(3.0, 0.0) << 400u;
    QTest::newRow("stop finger/horizontal") << true << false << QPointF(0.0, 0.0) << 500u;
    QTest::newRow("finger/vertical") << false << true << QPointF(0.0, 2.5) << 600u;
    QTest::newRow("stop finger/vertical") << false << true << QPointF(0.0, 0.0) << 700u;
    QTest::newRow("finger/both") << true << true << QPointF(1.1, 4.2) << 800u;
    QTest::newRow("stop finger/both") << true << true << QPointF(0.0, 0.0) << 900u;
}

void TestLibinputPointerEvent::testScrollFinger()
{
    // this test verifies pointer axis functionality
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_SCROLL_FINGER;
    QFETCH(bool, horizontal);
    QFETCH(bool, vertical);
    QFETCH(QPointF, value);
    QFETCH(quint32, time);
    pointerEvent->horizontalAxis = horizontal;
    pointerEvent->verticalAxis = vertical;
    pointerEvent->horizontalScrollValue = value.x();
    pointerEvent->verticalScrollValue = value.y();
    pointerEvent->time = std::chrono::milliseconds(time);

    std::unique_ptr<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent *>(event.get());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_SCROLL_FINGER);
    QCOMPARE(pe->axis().contains(KWin::InputRedirection::PointerAxisHorizontal), horizontal);
    QCOMPARE(pe->axis().contains(KWin::InputRedirection::PointerAxisVertical), vertical);
    QCOMPARE(pe->scrollValue(KWin::InputRedirection::PointerAxisHorizontal), value.x());
    QCOMPARE(pe->scrollValue(KWin::InputRedirection::PointerAxisVertical), value.y());
    QCOMPARE(pe->time(), pointerEvent->time);
}

void TestLibinputPointerEvent::testScrollContinuous_data()
{
    QTest::addColumn<bool>("horizontal");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<QPointF>("value");
    QTest::addColumn<quint32>("time");

    QTest::newRow("continuous/horizontal") << true << false << QPointF(3.0, 0.0) << 1000u;
    QTest::newRow("continuous/vertical") << false << true << QPointF(0.0, 2.5) << 1100u;
    QTest::newRow("continuous/both") << true << true << QPointF(1.1, 4.2) << 1200u;
}

void TestLibinputPointerEvent::testScrollContinuous()
{
    // this test verifies pointer axis functionality
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS;
    QFETCH(bool, horizontal);
    QFETCH(bool, vertical);
    QFETCH(QPointF, value);
    QFETCH(quint32, time);
    pointerEvent->horizontalAxis = horizontal;
    pointerEvent->verticalAxis = vertical;
    pointerEvent->horizontalScrollValue = value.x();
    pointerEvent->verticalScrollValue = value.y();
    pointerEvent->time = std::chrono::milliseconds(time);

    std::unique_ptr<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent *>(event.get());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS);
    QCOMPARE(pe->axis().contains(KWin::InputRedirection::PointerAxisHorizontal), horizontal);
    QCOMPARE(pe->axis().contains(KWin::InputRedirection::PointerAxisVertical), vertical);
    QCOMPARE(pe->scrollValue(KWin::InputRedirection::PointerAxisHorizontal), value.x());
    QCOMPARE(pe->scrollValue(KWin::InputRedirection::PointerAxisVertical), value.y());
    QCOMPARE(pe->time(), pointerEvent->time);
}

void TestLibinputPointerEvent::testMotion()
{
    // this test verifies pointer motion (delta)
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_MOTION;
    pointerEvent->delta = QPointF(2.1, 4.5);
    pointerEvent->time = 500ms;

    std::unique_ptr<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent *>(event.get());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_MOTION);
    QCOMPARE(pe->time(), pointerEvent->time);
    QCOMPARE(pe->delta(), QPointF(2.1, 4.5));
}

void TestLibinputPointerEvent::testAbsoluteMotion()
{
    // this test verifies absolute pointer motion
    libinput_event_pointer *pointerEvent = new libinput_event_pointer;
    pointerEvent->device = m_nativeDevice;
    pointerEvent->type = LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE;
    pointerEvent->absolutePos = QPointF(6.25, 6.9);
    pointerEvent->time = 500ms;

    std::unique_ptr<Event> event(Event::create(pointerEvent));
    auto pe = dynamic_cast<PointerEvent *>(event.get());
    QVERIFY(pe);
    QCOMPARE(pe->type(), LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE);
    QCOMPARE(pe->time(), pointerEvent->time);
    QCOMPARE(pe->absolutePos(), QPointF(6.25, 6.9));
    QCOMPARE(pe->absolutePos(QSize(1280, 1024)), QPointF(640, 512));
}

QTEST_GUILESS_MAIN(TestLibinputPointerEvent)
#include "pointer_event_test.moc"
