/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

// Qt
#include <QHash>
#include <QRasterWindow>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
// WaylandServer
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/seat_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"

#include "gaminginput_v2.h"
#include "generic_scene_opengl_test.h"
#include "workspace.h"

#include <fcntl.h>
#include <linux/uinput.h>
#include <unistd.h>

using namespace KWaylandServer;
using namespace std::literals;

class MockDevice
{
public:
    MockDevice()
    {
        struct uinput_setup usetup;
        m_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (m_fd < 0) {
            qCritical() << "Couldn't open /dev/uinput, make sure the kernel module is loaded";
            return;
        }

        // Register all keys we want to press with this application
        ioctl(m_fd, UI_SET_EVBIT, EV_ABS);
        ioctl(m_fd, UI_SET_EVBIT, EV_KEY);
        ioctl(m_fd, UI_SET_KEYBIT, BTN_EAST);
        ioctl(m_fd, UI_SET_KEYBIT, BTN_WEST);
        ioctl(m_fd, UI_SET_KEYBIT, BTN_DPAD_UP);
        ioctl(m_fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
        ioctl(m_fd, UI_SET_KEYBIT, BTN_MODE);

        memset(&usetup, 0, sizeof(usetup));
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x1234;
        usetup.id.product = 0x5678;
        strcpy(usetup.name, "MockDeviceForGamingInputTest");

        ioctl(m_fd, UI_DEV_SETUP, &usetup);
        ioctl(m_fd, UI_DEV_CREATE);
    }

    void emitKey(int key, bool pressed)
    {
        qWarning() << "key" << key << pressed;
        emitEvent(EV_KEY, key, pressed ? 1 : 0);
        emitEvent(EV_SYN, SYN_REPORT, 0);
    }

    void emitEvent(int type, int code, int val)
    {
        struct input_event ie;

        ie.type = type;
        ie.code = code;
        ie.value = val;
        ie.input_event_sec = 0;
        ie.input_event_usec = 0;

        write(m_fd, &ie, sizeof(ie));
    }
    ~MockDevice()
    {
        if (m_fd) {
            close(m_fd);
        }
    }

private:
    int m_fd = -1;
};

class TestGamepad : public GenericSceneOpenGLTest
{
    Q_OBJECT
public:
    TestGamepad()
        : GenericSceneOpenGLTest("O2")
    {
    }

private Q_SLOTS:
    void init();
    void testAdd();
    void testEvents();

private:
    GamingInputSeat *m_gamepadSeat = nullptr;
};

void TestGamepad::init()
{
    using namespace KWin;
    QVERIFY(KWin::Test::setupWaylandConnection(KWin::Test::AdditionalWaylandInterface::GamingInputV2 | KWin::Test::AdditionalWaylandInterface::Seat));
    QVERIFY(KWin::Test::gamingInputV2());
    auto input = KWin::Test::gamingInputV2();
    m_gamepadSeat = input->seat(*KWin::Test::waylandSeat());
    QVERIFY(m_gamepadSeat);
}

void TestGamepad::testAdd()
{
    QSignalSpy gamepadSpy(m_gamepadSeat, &GamingInputSeat::gamepadAdded);
    auto dev = std::make_unique<MockDevice>();
    QVERIFY(dev);
    QVERIFY(gamepadSpy.wait() || gamepadSpy.count() == 1);
    QCOMPARE(m_gamepadSeat->m_gamepads.count(), 1);
    QSignalSpy gamepadRemovedSpy(m_gamepadSeat->m_gamepads.constFirst(), &GamePad::removed);
    dev.reset();

    QVERIFY(gamepadRemovedSpy.wait() || gamepadRemovedSpy.count() == 1);
    QCOMPARE(m_gamepadSeat->m_gamepads.count(), 0);
}

void TestGamepad::testEvents()
{
    using namespace KWin;

    QSignalSpy gamepadSpy(m_gamepadSeat, &GamingInputSeat::gamepadAdded);
    auto dev = std::make_unique<MockDevice>();
    QVERIFY(dev);
    QVERIFY(gamepadSpy.wait() || gamepadSpy.count() == 1);

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto w = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(w);
    w->setActive(true);
    KWin::workspace()->setActiveWindow(w);

    auto gamepad = m_gamepadSeat->m_gamepads.constFirst();
    QSignalSpy buttonSpy(gamepad, &GamePad::buttonEvent);
    QVERIFY(KWin::workspace()->activeWindow());
    dev->emitKey(BTN_WEST, 1);
    QVERIFY(buttonSpy.wait() || buttonSpy.count() == 1);
    dev->emitKey(BTN_WEST, 0);
    QVERIFY(buttonSpy.wait() || buttonSpy.count() == 2);

    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(w));
}

WAYLANDTEST_MAIN(TestGamepad)
#include "test_gamepadv2.moc"
