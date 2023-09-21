/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/inputdevice.h"
#include "input.h"
#include "main.h"
#include "wayland_server.h"

#include <linux/input-event-codes.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_fakeinput-0");

class FakeInputTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testPointerMotion();
    void testMotionAbsolute();
    void testPointerButton_data();
    void testPointerButton();
    void testPointerVerticalAxis();
    void testPointerHorizontalAxis();
    void testTouch();
    void testKeyboardKey_data();
    void testKeyboardKey();

private:
    InputDevice *m_inputDevice = nullptr;
};

void FakeInputTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void FakeInputTest::init()
{
    QSignalSpy deviceAddedSpy(input(), &InputRedirection::deviceAdded);
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::FakeInput));

    QVERIFY(deviceAddedSpy.wait());
    m_inputDevice = deviceAddedSpy.last().at(0).value<InputDevice *>();
}

void FakeInputTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void FakeInputTest::testPointerMotion()
{
    Test::FakeInput *fakeInput = Test::waylandFakeInput();

    // without an authentication we shouldn't get the signals
    QSignalSpy pointerMotionSpy(m_inputDevice, &InputDevice::pointerMotion);
    fakeInput->pointer_motion(wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(Test::waylandSync());
    QVERIFY(pointerMotionSpy.isEmpty());

    // now let's authenticate the interface
    fakeInput->authenticate(QStringLiteral("org.kde.foobar"), QStringLiteral("foobar"));
    fakeInput->pointer_motion(wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.last().first().toPointF(), QPointF(1, 2));

    // just for the fun: once more
    fakeInput->pointer_motion(wl_fixed_from_double(0), wl_fixed_from_double(0));
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.last().first().toPointF(), QPointF(0, 0));
}

void FakeInputTest::testMotionAbsolute()
{
    Test::FakeInput *fakeInput = Test::waylandFakeInput();

    // without an authentication we shouldn't get the signals
    QSignalSpy pointerMotionAbsoluteSpy(m_inputDevice, &InputDevice::pointerMotionAbsolute);
    fakeInput->pointer_motion_absolute(wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(Test::waylandSync());
    QVERIFY(pointerMotionAbsoluteSpy.isEmpty());

    // now let's authenticate the interface
    fakeInput->authenticate(QStringLiteral("org.kde.foobar"), QStringLiteral("foobar"));
    fakeInput->pointer_motion_absolute(wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(pointerMotionAbsoluteSpy.wait());
    QCOMPARE(pointerMotionAbsoluteSpy.last().first().toPointF(), QPointF(1, 2));
}

void FakeInputTest::testPointerButton_data()
{
    QTest::addColumn<quint32>("linuxButton");

    QTest::newRow("left") << quint32(BTN_LEFT);
    QTest::newRow("right") << quint32(BTN_RIGHT);
    QTest::newRow("middle") << quint32(BTN_MIDDLE);
    QTest::newRow("side") << quint32(BTN_SIDE);
    QTest::newRow("extra") << quint32(BTN_EXTRA);
    QTest::newRow("forward") << quint32(BTN_FORWARD);
    QTest::newRow("back") << quint32(BTN_BACK);
    QTest::newRow("task") << quint32(BTN_TASK);
}

void FakeInputTest::testPointerButton()
{
    Test::FakeInput *fakeInput = Test::waylandFakeInput();

    // without an authentication we shouldn't get the signals
    QSignalSpy pointerButtonSpy(m_inputDevice, &InputDevice::pointerButtonChanged);
    QFETCH(quint32, linuxButton);
    fakeInput->button(linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
    QVERIFY(Test::waylandSync());
    QVERIFY(pointerButtonSpy.isEmpty());

    // now authenticate
    fakeInput->authenticate(QStringLiteral("org.kde.foobar"), QStringLiteral("foobar"));
    fakeInput->button(linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
    QVERIFY(pointerButtonSpy.wait());
    QCOMPARE(pointerButtonSpy.last().at(0).value<quint32>(), linuxButton);
    QCOMPARE(pointerButtonSpy.last().at(1).value<InputRedirection::PointerButtonState>(), InputRedirection::PointerButtonPressed);

    // and release
    fakeInput->button(linuxButton, WL_POINTER_BUTTON_STATE_RELEASED);
    QVERIFY(pointerButtonSpy.wait());
    QCOMPARE(pointerButtonSpy.last().at(0).value<quint32>(), linuxButton);
    QCOMPARE(pointerButtonSpy.last().at(1).value<InputRedirection::PointerButtonState>(), InputRedirection::PointerButtonReleased);
}

void FakeInputTest::testPointerVerticalAxis()
{
    Test::FakeInput *fakeInput = Test::waylandFakeInput();

    // without an authentication we shouldn't get the signals
    QSignalSpy pointerAxisSpy(m_inputDevice, &InputDevice::pointerAxisChanged);
    fakeInput->axis(WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_double(15));
    QVERIFY(Test::waylandSync());
    QVERIFY(pointerAxisSpy.isEmpty());

    // now authenticate
    fakeInput->authenticate(QStringLiteral("org.kde.foobar"), QStringLiteral("foobar"));
    fakeInput->axis(WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_double(15));
    QVERIFY(pointerAxisSpy.wait());
    QCOMPARE(pointerAxisSpy.last().at(0).value<InputRedirection::PointerAxis>(), InputRedirection::PointerAxisVertical);
    QCOMPARE(pointerAxisSpy.last().at(1).value<qreal>(), 15);
}

void FakeInputTest::testPointerHorizontalAxis()
{
    Test::FakeInput *fakeInput = Test::waylandFakeInput();

    // without an authentication we shouldn't get the signals
    QSignalSpy pointerAxisSpy(m_inputDevice, &InputDevice::pointerAxisChanged);
    fakeInput->axis(WL_POINTER_AXIS_HORIZONTAL_SCROLL, wl_fixed_from_double(15));
    QVERIFY(Test::waylandSync());
    QVERIFY(pointerAxisSpy.isEmpty());

    // now authenticate
    fakeInput->authenticate(QStringLiteral("org.kde.foobar"), QStringLiteral("foobar"));
    fakeInput->axis(WL_POINTER_AXIS_HORIZONTAL_SCROLL, wl_fixed_from_double(15));
    QVERIFY(pointerAxisSpy.wait());
    QCOMPARE(pointerAxisSpy.last().at(0).value<InputRedirection::PointerAxis>(), InputRedirection::PointerAxisHorizontal);
    QCOMPARE(pointerAxisSpy.last().at(1).value<qreal>(), 15);
}

void FakeInputTest::testTouch()
{
    Test::FakeInput *fakeInput = Test::waylandFakeInput();

    // without an authentication we shouldn't get the signals
    QSignalSpy touchDownSpy(m_inputDevice, &InputDevice::touchDown);
    QSignalSpy touchUpSpy(m_inputDevice, &InputDevice::touchUp);
    QSignalSpy touchMotionSpy(m_inputDevice, &InputDevice::touchMotion);
    QSignalSpy touchFrameSpy(m_inputDevice, &InputDevice::touchFrame);
    QSignalSpy touchCanceledSpy(m_inputDevice, &InputDevice::touchCanceled);
    fakeInput->touch_down(0, wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(Test::waylandSync());
    QVERIFY(touchDownSpy.isEmpty());

    fakeInput->touch_motion(0, wl_fixed_from_double(3), wl_fixed_from_double(4));
    QVERIFY(Test::waylandSync());
    QVERIFY(touchMotionSpy.isEmpty());

    fakeInput->touch_up(0);
    QVERIFY(Test::waylandSync());
    QVERIFY(touchUpSpy.isEmpty());

    fakeInput->touch_down(1, wl_fixed_from_double(5), wl_fixed_from_double(6));
    QVERIFY(Test::waylandSync());
    QVERIFY(touchDownSpy.isEmpty());

    fakeInput->touch_frame();
    QVERIFY(Test::waylandSync());
    QVERIFY(touchFrameSpy.isEmpty());

    fakeInput->touch_cancel();
    QVERIFY(Test::waylandSync());
    QVERIFY(touchCanceledSpy.isEmpty());

    // now let's authenticate the interface
    fakeInput->authenticate(QStringLiteral("org.kde.foobar"), QStringLiteral("foobar"));
    fakeInput->touch_down(0, wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(touchDownSpy.wait());
    QCOMPARE(touchDownSpy.count(), 1);
    QCOMPARE(touchDownSpy.last().at(0).value<quint32>(), quint32(0));
    QCOMPARE(touchDownSpy.last().at(1).toPointF(), QPointF(1, 2));

    // Same id should not trigger another touchDown.
    fakeInput->touch_down(0, wl_fixed_from_double(5), wl_fixed_from_double(6));
    QVERIFY(Test::waylandSync());
    QCOMPARE(touchDownSpy.count(), 1);

    fakeInput->touch_motion(0, wl_fixed_from_double(3), wl_fixed_from_double(4));
    QVERIFY(touchMotionSpy.wait());
    QCOMPARE(touchMotionSpy.count(), 1);
    QCOMPARE(touchMotionSpy.last().at(0).value<quint32>(), quint32(0));
    QCOMPARE(touchMotionSpy.last().at(1).toPointF(), QPointF(3, 4));

    // touchMotion with bogus id should not trigger signal.
    fakeInput->touch_motion(1, wl_fixed_from_double(3), wl_fixed_from_double(4));
    QVERIFY(Test::waylandSync());
    QCOMPARE(touchMotionSpy.count(), 1);

    fakeInput->touch_up(0);
    QVERIFY(touchUpSpy.wait());
    QCOMPARE(touchUpSpy.count(), 1);
    QCOMPARE(touchUpSpy.last().at(0).value<quint32>(), quint32(0));

    // touchUp with bogus id should not trigger signal.
    fakeInput->touch_up(1);
    QVERIFY(Test::waylandSync());
    QCOMPARE(touchUpSpy.count(), 1);

    fakeInput->touch_down(1, wl_fixed_from_double(5), wl_fixed_from_double(6));
    QVERIFY(touchDownSpy.wait());
    QCOMPARE(touchDownSpy.count(), 2);
    QCOMPARE(touchDownSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(touchDownSpy.last().at(1).toPointF(), QPointF(5, 6));

    fakeInput->touch_frame();
    QVERIFY(touchFrameSpy.wait());
    QCOMPARE(touchFrameSpy.count(), 1);

    fakeInput->touch_cancel();
    QVERIFY(touchCanceledSpy.wait());
    QCOMPARE(touchCanceledSpy.count(), 1);
}

void FakeInputTest::testKeyboardKey_data()
{
    QTest::addColumn<quint32>("linuxKey");

    QTest::newRow("A") << quint32(KEY_A);
    QTest::newRow("S") << quint32(KEY_S);
    QTest::newRow("D") << quint32(KEY_D);
    QTest::newRow("F") << quint32(KEY_F);
}

void FakeInputTest::testKeyboardKey()
{
    Test::FakeInput *fakeInput = Test::waylandFakeInput();

    // without an authentication we shouldn't get the signals
    QSignalSpy keyboardKeySpy(m_inputDevice, &InputDevice::keyChanged);
    QFETCH(quint32, linuxKey);
    fakeInput->keyboard_key(linuxKey, WL_KEYBOARD_KEY_STATE_PRESSED);
    QVERIFY(Test::waylandSync());
    QVERIFY(keyboardKeySpy.isEmpty());

    // now authenticate
    fakeInput->authenticate(QStringLiteral("org.kde.foobar"), QStringLiteral("foobar"));
    fakeInput->keyboard_key(linuxKey, WL_KEYBOARD_KEY_STATE_PRESSED);
    QVERIFY(keyboardKeySpy.wait());
    QCOMPARE(keyboardKeySpy.last().at(0).value<quint32>(), linuxKey);
    QCOMPARE(keyboardKeySpy.last().at(1).value<InputRedirection::KeyboardKeyState>(), InputRedirection::KeyboardKeyPressed);

    // and release
    fakeInput->keyboard_key(linuxKey, WL_KEYBOARD_KEY_STATE_RELEASED);
    QVERIFY(keyboardKeySpy.wait());
    QCOMPARE(keyboardKeySpy.last().at(0).value<quint32>(), linuxKey);
    QCOMPARE(keyboardKeySpy.last().at(1).value<InputRedirection::KeyboardKeyState>(), InputRedirection::KeyboardKeyReleased);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::FakeInputTest)
#include "fakeinput_test.moc"
