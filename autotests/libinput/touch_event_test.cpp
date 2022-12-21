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

using namespace KWin::LibInput;
using namespace std::literals;

class TestLibinputTouchEvent : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testType_data();
    void testType();
    void testAbsoluteMotion_data();
    void testAbsoluteMotion();

private:
    libinput_device *m_nativeDevice = nullptr;
    Device *m_device = nullptr;
};

void TestLibinputTouchEvent::init()
{
    m_nativeDevice = new libinput_device;
    m_nativeDevice->touch = true;
    m_nativeDevice->deviceSize = QSizeF(12.5, 13.8);
    m_device = new Device(m_nativeDevice);
}

void TestLibinputTouchEvent::cleanup()
{
    delete m_device;
    m_device = nullptr;

    delete m_nativeDevice;
    m_nativeDevice = nullptr;
}

void TestLibinputTouchEvent::testType_data()
{
    QTest::addColumn<libinput_event_type>("type");
    QTest::addColumn<bool>("hasId");

    QTest::newRow("down") << LIBINPUT_EVENT_TOUCH_DOWN << true;
    QTest::newRow("up") << LIBINPUT_EVENT_TOUCH_UP << true;
    QTest::newRow("motion") << LIBINPUT_EVENT_TOUCH_MOTION << true;
    QTest::newRow("cancel") << LIBINPUT_EVENT_TOUCH_CANCEL << false;
    QTest::newRow("frame") << LIBINPUT_EVENT_TOUCH_FRAME << false;
}

void TestLibinputTouchEvent::testType()
{
    // this test verifies the initialization of a PointerEvent and the parent Event class
    libinput_event_touch *touchEvent = new libinput_event_touch;
    QFETCH(libinput_event_type, type);
    touchEvent->type = type;
    touchEvent->device = m_nativeDevice;
    touchEvent->slot = 0;

    std::unique_ptr<Event> event(Event::create(touchEvent));
    // API of event
    QCOMPARE(event->type(), type);
    QCOMPARE(event->device(), m_device);
    QCOMPARE(event->nativeDevice(), m_nativeDevice);
    QCOMPARE((libinput_event *)(*event.get()), touchEvent);
    // verify it's a pointer event
    QVERIFY(dynamic_cast<TouchEvent *>(event.get()));
    QCOMPARE((libinput_event_touch *)(*dynamic_cast<TouchEvent *>(event.get())), touchEvent);
    QFETCH(bool, hasId);
    if (hasId) {
        QCOMPARE(dynamic_cast<TouchEvent *>(event.get())->id(), 0);
    }
}

void TestLibinputTouchEvent::testAbsoluteMotion_data()
{
    QTest::addColumn<libinput_event_type>("type");
    QTest::newRow("down") << LIBINPUT_EVENT_TOUCH_DOWN;
    QTest::newRow("motion") << LIBINPUT_EVENT_TOUCH_MOTION;
}

void TestLibinputTouchEvent::testAbsoluteMotion()
{
    // this test verifies absolute touch points (either down or motion)
    libinput_event_touch *touchEvent = new libinput_event_touch;
    touchEvent->device = m_nativeDevice;
    QFETCH(libinput_event_type, type);
    touchEvent->type = type;
    touchEvent->absolutePos = QPointF(6.25, 6.9);
    touchEvent->time = 500ms;
    touchEvent->slot = 1;

    std::unique_ptr<Event> event(Event::create(touchEvent));
    auto te = dynamic_cast<TouchEvent *>(event.get());
    QVERIFY(te);
    QCOMPARE(te->type(), type);
    QCOMPARE(te->time(), touchEvent->time);
    QCOMPARE(te->absolutePos(), QPointF(6.25, 6.9));
    QCOMPARE(te->absolutePos(QSize(1280, 1024)), QPointF(640, 512));
}

QTEST_GUILESS_MAIN(TestLibinputTouchEvent)
#include "touch_event_test.moc"
