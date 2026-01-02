/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsinsepulveda@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "idledetector.h"
#include "input.h"
#include "kwin_wayland_test.h"
#include "pluginmanager.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KIdleTime>
#include <KSharedConfig>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>

#include <QPluginLoader>
#include <QTemporaryDir>

#include <fcntl.h>
#include <libevdev-1.0/libevdev/libevdev-uinput.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <unistd.h>

Q_IMPORT_PLUGIN(KWinIdleTimePoller)

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_gamecontroller-0");
static const QString s_pluginName = QStringLiteral("gamecontroller");

class TestGameController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // NOTE: Hot plug test would require reference to instance of the plugin
    // TODO: void testHotPlugDetection();

    void testResetIdleTime();

    void testKeyboardMapping_data();
    void testKeyboardMapping();

    void testPointerMapping_data();
    void testPointerMapping();

    void testAnalogStickPointerMovement_data();
    void testAnalogStickPointerMovement();

private:
    libevdev *m_evdev;
    libevdev_uinput *m_uinput;
    int direction = 0;
};

void TestGameController::initTestCase()
{
    qputenv("KWIN_GAMECONTROLLER_TEST", "true");

    // Skip if /dev/uinput isn't present
    if (!QFile::exists("/dev/uinput")) {
        QSKIP("/dev/uinput not available; skipping uinput-based tests");
    }

    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
}

void TestGameController::cleanupTestCase()
{
    if (m_evdev) {
        libevdev_free(m_evdev);
        m_evdev = nullptr;
    }
    if (m_uinput) {
        libevdev_uinput_destroy(m_uinput);
        m_uinput = nullptr;
    }
}

void TestGameController::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());

    m_evdev = libevdev_new();
    QVERIFY(m_evdev != nullptr);
}

void TestGameController::cleanup()
{
    if (m_uinput) {
        libevdev_uinput_destroy(m_uinput);
        m_uinput = nullptr;
    }
    if (m_evdev) {
        libevdev_free(m_evdev);
        m_evdev = nullptr;
    }

    Test::destroyWaylandConnection();
}

void TestGameController::testResetIdleTime()
{
    //  This test verifies that game controller activity resets KWin's idle detectors.
    //
    //  We test state transitions directly using IdleDetector:
    //
    //    1. Create an IdleDetector with a short timeout (1s).
    //    2. Wait for idle() signal to confirm the system has actually gone idle.
    //    3. Simulate game controller activity (BTN_SOUTH press/release events).
    //    4. Verify that resumed() signal is emitted, proving that controller activity
    //       successfully reset idle state and woke the idle detector.
    //
    //  In other words, we assert the correct transition:
    //      Active → Idle (after timeout) → Active again (after controller input).
    //
    // Reload plugin instance
    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    libevdev_set_name(m_evdev, "Test Virtual Game Controller");
    libevdev_enable_event_type(m_evdev, EV_KEY);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_GAMEPAD, nullptr);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_JOYSTICK, nullptr);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_SOUTH, nullptr);

    int rc = libevdev_uinput_create_from_device(m_evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &m_uinput);
    if (rc == -EACCES) {
        QSKIP("Permission denied opening /dev/uinput; skipping test");
    }
    QVERIFY(rc == 0);

    QString path = QString::fromUtf8(libevdev_uinput_get_devnode(m_uinput));
    QVERIFY(path.startsWith("/dev/input/"));

    const auto idleTimeout = std::chrono::milliseconds(1000);
    KWin::IdleDetector idleDetector(idleTimeout, KWin::IdleDetector::OperatingMode::FollowsInhibitors);

    QSignalSpy idleSpy(&idleDetector, &KWin::IdleDetector::idle);
    QSignalSpy resumeSpy(&idleDetector, &KWin::IdleDetector::resumed);

    // Wait until we actually become idle
    // Give it up to ~3s to fire (dev sessions can be a bit noisy)
    QVERIFY2(idleSpy.wait(100), "KIdleTime::timeoutReached() did not fire");

    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_KEY, BTN_SOUTH, 1) == 0); // press
    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);

    QVERIFY2(resumeSpy.wait(), "KIdleTime::resumingFromIdle() did not fire after activity");

    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_KEY, BTN_SOUTH, 0) == 0); // release
    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);
}

void TestGameController::testKeyboardMapping_data()
{
    QTest::addColumn<int>("evdevCode");
    QTest::addColumn<int>("inputValue");
    QTest::addColumn<QList<quint32>>("keyboardKey");

    QTest::newRow("BTN_SOUTH -> Enter") << BTN_SOUTH << 1 << QList<quint32>{KEY_ENTER};
    QTest::newRow("BTN_EAST -> Escape") << BTN_EAST << 1 << QList<quint32>{KEY_ESC};
    QTest::newRow("BTN_WEST -> Space") << BTN_WEST << 1 << QList<quint32>{KEY_SPACE};
    QTest::newRow("BTN_TL -> Control") << BTN_TL << 1 << QList<quint32>{KEY_LEFTCTRL};
    QTest::newRow("BTN_TR -> Alt") << BTN_TR << 1 << QList<quint32>{KEY_LEFTALT};
    QTest::newRow("BTN_START -> Meta") << BTN_START << 1 << QList<quint32>{KEY_LEFTMETA};

    // D-pad hat is axes,
    // inputValue is -1 (left/up) or +1 (right/down)
    QTest::newRow("ABS_HAT0X Left") << ABS_HAT0X << -1 << QList<quint32>{KEY_LEFT};
    QTest::newRow("ABS_HAT0X Right") << ABS_HAT0X << 1 << QList<quint32>{KEY_RIGHT};
    QTest::newRow("ABS_HAT0Y Up") << ABS_HAT0Y << -1 << QList<quint32>{KEY_UP};
    QTest::newRow("ABS_HAT0Y Down") << ABS_HAT0Y << 1 << QList<quint32>{KEY_DOWN};
}

void TestGameController::testKeyboardMapping()
{
    // This test verifies that game controller button presses are correctly mapped to
    // their corresponding keyboard keys.
    //
    // The test validates the core button-to-key mappings defined in
    // EmulatedInputDevice::evkeyMapping():
    //   - BTN_SOUTH (A button) → Enter key
    //   - BTN_EAST (B button) → Escape key
    //   - BTN_WEST (Y button) → Space key
    //   - BTN_TL (L shoulder) → Left Control key
    //   - BTN_TR (R shoulder) → Left Alt key
    //   - BTN_START → Left Meta key
    //
    // For each button mapping:
    //   1. Create a virtual game controller with the specific button enabled
    //   2. Create a Wayland surface to receive keyboard focus
    //   3. Set up a keyboard spy to capture remapped key events
    //   4. Send the game controller button press via libevdev_uinput
    //   5. Verify the correct keyboard key press event is generated
    //   6. Send the button release to complete the input cycle

    QFETCH(int, evdevCode);
    QFETCH(int, inputValue);

    // Reload plugin instance
    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(keyboardEnteredSpy.wait());

    libevdev_set_name(m_evdev, "Test Virtual Game Controller");
    libevdev_enable_event_type(m_evdev, EV_KEY);
    libevdev_enable_event_type(m_evdev, EV_ABS);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_GAMEPAD, nullptr);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_JOYSTICK, nullptr);

    input_absinfo hat{};
    hat.minimum = -1;
    hat.maximum = 1;
    hat.fuzz = 0;
    hat.flat = 0;
    hat.resolution = 0;

    if (evdevCode == ABS_HAT0X || evdevCode == ABS_HAT0Y) {
        libevdev_enable_event_code(m_evdev, EV_ABS, evdevCode, &hat);
    } else {
        libevdev_enable_event_code(m_evdev, EV_KEY, evdevCode, nullptr);
    }

    int rc = libevdev_uinput_create_from_device(m_evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &m_uinput);
    if (rc == -EACCES) {
        QSKIP("Permission denied opening /dev/uinput; skipping test");
    }
    QVERIFY(rc == 0);

    QString path = QString::fromUtf8(libevdev_uinput_get_devnode(m_uinput));
    QVERIFY(path.startsWith("/dev/input/"));

    if (evdevCode == ABS_HAT0X || evdevCode == ABS_HAT0Y) {
        QVERIFY(libevdev_uinput_write_event(m_uinput, EV_ABS, evdevCode, inputValue) == 0);
    } else {
        QVERIFY(libevdev_uinput_write_event(m_uinput, EV_KEY, evdevCode, inputValue) == 0);
    }
    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);

    QVERIFY(keyChangedSpy.wait());
    QFETCH(QList<quint32>, keyboardKey);
    QCOMPARE(keyChangedSpy.count(), keyboardKey.count());
    for (int i = 0; i < keyChangedSpy.count(); i++) {
        QCOMPARE(keyChangedSpy.at(i).at(0).value<quint32>(), keyboardKey.at(i));
        QCOMPARE(keyChangedSpy.at(i).at(1).value<KWayland::Client::Keyboard::KeyState>(),
                 KWayland::Client::Keyboard::KeyState::Pressed);
    }

    keyChangedSpy.clear();

    // Release the button
    if (evdevCode == ABS_HAT0X || evdevCode == ABS_HAT0Y) {
        QVERIFY(libevdev_uinput_write_event(m_uinput, EV_ABS, evdevCode, 0) == 0);
        QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);
    } else {
        QVERIFY(libevdev_uinput_write_event(m_uinput, EV_KEY, evdevCode, 0) == 0);
        QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);
    }

    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), keyboardKey.count());
    for (int i = 0; i < keyChangedSpy.count(); i++) {
        QCOMPARE(keyChangedSpy.at(i).at(0).value<quint32>(), keyboardKey.at(i));
        QCOMPARE(keyChangedSpy.at(i).at(1).value<KWayland::Client::Keyboard::KeyState>(),
                 KWayland::Client::Keyboard::KeyState::Released);
    }
}

void TestGameController::testPointerMapping_data()
{
    QTest::addColumn<int>("analogAxis");
    QTest::addColumn<int>("expectedButton");

    QTest::newRow("ABS_Z -> Left Click") << ABS_Z << BTN_LEFT;
    QTest::newRow("ABS_RZ -> Right Click") << ABS_RZ << BTN_RIGHT;
}

void TestGameController::testPointerMapping()
{
    // This test verifies that game controller analog inputs (triggers, D-pad) are correctly
    // mapped to their corresponding keyboard keys or mouse buttons.
    //
    // The test covers two main categories of analog mappings:
    //   1. Triggers (ABS_Z, ABS_RZ) → Mouse buttons (BTN_LEFT, BTN_RIGHT)
    //   2. D-pad hats (ABS_HAT0X, ABS_HAT0Y) → Arrow keys (KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN)
    //
    // For each mapping:
    //   1. Create a virtual game controller with the specific analog axis enabled
    //   2. Set up appropriate input_absinfo ranges for the axis type
    //   3. Create a Wayland surface to receive the remapped input events
    //   4. Send the analog input event via libevdev_uinput
    //   5. Verify the correct output event is generated:
    //      - For triggers: Check mouse button press events via pointer spy
    //      - For D-pad: Check keyboard key press events via keyboard spy

    QFETCH(int, analogAxis);
    QFETCH(int, expectedButton);

    // Reload plugin instance
    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);
    QVERIFY(window != nullptr);

    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy buttonChangedSpy(pointer.get(), &KWayland::Client::Pointer::buttonStateChanged);

    const QRectF startGeometry = window->frameGeometry();
    input()->pointer()->warp(startGeometry.center());
    QVERIFY(enteredSpy.wait());

    libevdev_set_name(m_evdev, "Test Virtual Game Controller");
    libevdev_enable_event_type(m_evdev, EV_KEY);
    libevdev_enable_event_type(m_evdev, EV_ABS);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_GAMEPAD, nullptr);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_JOYSTICK, nullptr);

    struct input_absinfo absinfo{};
    if (analogAxis == ABS_Z || analogAxis == ABS_RZ) {
        absinfo.minimum = 0;
        absinfo.maximum = 255;
    } else {
        absinfo.minimum = -32768;
        absinfo.maximum = 32767;
    }
    libevdev_enable_event_code(m_evdev, EV_ABS, analogAxis, &absinfo);

    int rc = libevdev_uinput_create_from_device(m_evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &m_uinput);
    if (rc == -EACCES) {
        QSKIP("Permission denied opening /dev/uinput; skipping test");
    }
    QVERIFY(rc == 0);

    QString path = QString::fromUtf8(libevdev_uinput_get_devnode(m_uinput));
    QVERIFY(path.startsWith("/dev/input/"));

    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_ABS, analogAxis, 1) == 0);
    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);

    QVERIFY(buttonChangedSpy.wait());

    QCOMPARE(buttonChangedSpy.count(), 1);
    QCOMPARE(buttonChangedSpy.at(0).at(2).value<qint32>(), expectedButton);
    QCOMPARE(buttonChangedSpy.at(0).at(0).value<KWayland::Client::Pointer::ButtonState>(),
             KWayland::Client::Pointer::ButtonState::Pressed);

    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_ABS, analogAxis, 0) == 0);
    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);

    buttonChangedSpy.clear();
    QCOMPARE(buttonChangedSpy.count(), 1);
    QCOMPARE(buttonChangedSpy.at(0).at(2).value<qint32>(), 0);
    QCOMPARE(buttonChangedSpy.at(0).at(0).value<KWayland::Client::Pointer::ButtonState>(),
             KWayland::Client::Pointer::ButtonState::Released);
}

void TestGameController::testAnalogStickPointerMovement_data()
{
    QTest::addColumn<int>("analogAxis");
    QTest::addColumn<int>("inputValue");
    QTest::addColumn<QString>("expectedDirection");

    QTest::newRow("ABS_X Left") << ABS_X << -32768 << QStringLiteral("left");
    QTest::newRow("ABS_X Right") << ABS_X << 32767 << QStringLiteral("right");
    QTest::newRow("ABS_Y Up") << ABS_Y << -32768 << QStringLiteral("up");
    QTest::newRow("ABS_Y Down") << ABS_Y << 32767 << QStringLiteral("down");

    QTest::newRow("ABS_RX Left") << ABS_RX << -32768 << QStringLiteral("left");
    QTest::newRow("ABS_RX Right") << ABS_RX << 32767 << QStringLiteral("right");
    QTest::newRow("ABS_RY Up") << ABS_RY << -32768 << QStringLiteral("up");
    QTest::newRow("ABS_RY Down") << ABS_RY << 32767 << QStringLiteral("down");

    // Test center/neutral position
    QTest::newRow("ABS_X Center") << ABS_X << 0 << QStringLiteral("center");
    QTest::newRow("ABS_Y Center") << ABS_Y << 0 << QStringLiteral("center");
    QTest::newRow("ABS_RX Center") << ABS_RX << 0 << QStringLiteral("center");
    QTest::newRow("ABS_RY Center") << ABS_RY << 0 << QStringLiteral("center");

    // Test deadzone threshold values (anything below 8000)
    QTest::newRow("ABS_X Small") << ABS_X << 5000 << QStringLiteral("deadzone");
    QTest::newRow("ABS_Y Small") << ABS_Y << -5000 << QStringLiteral("deadzone");
}

void TestGameController::testAnalogStickPointerMovement()
{
    // This test verifies that analog stick movements correctly control the pointer position.
    //
    // The test validates:
    //   - Left and Right analog stick (ABS_X, ABS_RX, ABS_Y, ABS_RY ) → Pointer movement
    //   - Center position stops pointer movement
    //   - Small movements within deadzone are ignored
    //
    // For each analog axis:
    //   1. Send stick input via virtual controller
    //   2. Verify pointer moves in expected direction
    //   3. Test deadzone and center position behavior

    QFETCH(int, analogAxis);
    QFETCH(int, inputValue);
    QFETCH(QString, expectedDirection);

    // Reload plugin instance
    kwinApp()->pluginManager()->unloadPlugin(s_pluginName);
    kwinApp()->pluginManager()->loadPlugin(s_pluginName);

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(500, 500), Qt::blue);
    QVERIFY(window != nullptr);

    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy motionSpy(pointer.get(), &KWayland::Client::Pointer::motion);

    // Position pointer in center of window
    const QRectF startGeometry = window->frameGeometry();
    const QPointF centerPos = startGeometry.center();
    input()->pointer()->warp(centerPos);
    QVERIFY(enteredSpy.wait());

    libevdev_set_name(m_evdev, "Test Virtual Game Controller");
    libevdev_enable_event_type(m_evdev, EV_KEY);
    libevdev_enable_event_type(m_evdev, EV_ABS);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_GAMEPAD, nullptr);
    libevdev_enable_event_code(m_evdev, EV_KEY, BTN_JOYSTICK, nullptr);

    struct input_absinfo absinfo{};
    absinfo.minimum = -32768;
    absinfo.maximum = 32767;
    absinfo.fuzz = 0;
    absinfo.flat = 0;
    absinfo.resolution = 0;
    libevdev_enable_event_code(m_evdev, EV_ABS, analogAxis, &absinfo);

    int rc = libevdev_uinput_create_from_device(m_evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &m_uinput);
    if (rc == -EACCES) {
        QSKIP("Permission denied opening /dev/uinput; skipping test");
    }
    QVERIFY(rc == 0);

    QString path = QString::fromUtf8(libevdev_uinput_get_devnode(m_uinput));
    QVERIFY(path.startsWith("/dev/input/"));

    const QPointF initialPos = input()->pointer()->pos();

    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_ABS, analogAxis, inputValue) == 0);
    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);

    if (expectedDirection == QStringLiteral("deadzone") || expectedDirection == QStringLiteral("center")) {
        QVERIFY(!motionSpy.wait(10));

        const QPointF currentPos = input()->pointer()->pos();
        const qreal distance = QLineF(initialPos, currentPos).length();

        QVERIFY(distance < 1.0);
    } else {
        // For directional input, we expect pointer motion
        // The pointer updates via timer, so wait for motion events
        QVERIFY(motionSpy.wait());
        QCOMPARE(motionSpy.count(), 0.0);

        const QPointF finalPos = input()->pointer()->pos();
        const qreal deltaX = finalPos.x() - initialPos.x();
        const qreal deltaY = finalPos.y() - initialPos.y();

        const qreal threshold = 2.0;
        if (expectedDirection == QStringLiteral("left")) {
            QVERIFY2(deltaX < -threshold, qPrintable(QString("Expected left movement, got deltaX=%1").arg(deltaX)));
        } else if (expectedDirection == QStringLiteral("right")) {
            QVERIFY2(deltaX > threshold, qPrintable(QString("Expected right movement, got deltaX=%1").arg(deltaX)));
        } else if (expectedDirection == QStringLiteral("up")) {
            QVERIFY2(deltaY < -threshold, qPrintable(QString("Expected up movement, got deltaY=%1").arg(deltaY)));
        } else if (expectedDirection == QStringLiteral("down")) {
            QVERIFY2(deltaY > threshold, qPrintable(QString("Expected down movement, got deltaY=%1").arg(deltaY)));
        }
    }

    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_ABS, analogAxis, 0) == 0);
    QVERIFY(libevdev_uinput_write_event(m_uinput, EV_SYN, SYN_REPORT, 0) == 0);

    QVERIFY(!motionSpy.wait(10));
    const QPointF currentPos = input()->pointer()->pos();
    const qreal distance = QLineF(initialPos, currentPos).length();

    QVERIFY(distance > 0.0);
}

WAYLANDTEST_MAIN(TestGameController)
#include "gamecontroller_test.moc"
