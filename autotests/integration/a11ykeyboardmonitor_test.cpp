/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>

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

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_a11ykeyboardmonitor-0");

class A11yKeyboardWatcher : public QObject
{
    Q_OBJECT
public:
    A11yKeyboardWatcher()
    {
        QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.a11y.Manager", "/org/freedesktop/a11y/Manager", "org.freedesktop.a11y.KeyboardMonitor", "WatchKeyboard");

        QDBusReply<void> reply = QDBusConnection::sessionBus().call(msg);
        QVERIFY(reply.isValid());

        const bool ret = QDBusConnection::sessionBus().connect("org.freedesktop.a11y.Manager", "/org/freedesktop/a11y/Manager", "org.freedesktop.a11y.KeyboardMonitor", "KeyEvent", this, SIGNAL(keyEvent(bool, quint32, quint32, quint32, quint16)));
        QVERIFY(ret);
    }
    ~A11yKeyboardWatcher()
    {
        QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.a11y.Manager", "/org/freedesktop/a11y/Manager", "org.freedesktop.a11y.KeyboardMonitor", "UnwatchKeyboard");

        QDBusReply<void> reply = QDBusConnection::sessionBus().call(msg);
        QVERIFY(reply.isValid());
    }

Q_SIGNALS:
    void keyEvent(bool released, quint32 state, quint32 keysym, quint32 unichar, quint16 keycode);
};

class A11yKeyboardMonitorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testWatchKeyboard();
};

void A11yKeyboardMonitorTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));

    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
    });

    QDBusConnection::sessionBus().registerService("org.gnome.Orca.KeyboardMonitor");
}

void A11yKeyboardMonitorTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void A11yKeyboardMonitorTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void A11yKeyboardMonitorTest::testWatchKeyboard()
{
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(10, 10), Qt::blue);
    QVERIFY(waylandWindow);

    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_A, ++timestamp);

    A11yKeyboardWatcher watcher;

    QSignalSpy clientKeySpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QSignalSpy a11ySpy(&watcher, &A11yKeyboardWatcher::keyEvent);
    QVERIFY(a11ySpy.wait());
    QCOMPARE(a11ySpy.first()[0], false);
    QCOMPARE(a11ySpy.first()[1], 0);
    QCOMPARE(a11ySpy.first()[2], XKB_KEY_a);
    QCOMPARE(a11ySpy.first()[3], XKB_KEY_a);
    QCOMPARE(a11ySpy.first()[4], KEY_A + 8);
    a11ySpy.clear();

    if (clientKeySpy.isEmpty()) {
        QVERIFY(clientKeySpy.wait());
    }

    QCOMPARE(clientKeySpy.count(), 1);
    QCOMPARE(clientKeySpy.first()[0], KEY_A);
    QCOMPARE(clientKeySpy.first()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    clientKeySpy.clear();

    Test::keyboardKeyReleased(KEY_A, ++timestamp);
    QVERIFY(a11ySpy.wait());
    QCOMPARE(a11ySpy.first()[0], true);
    QCOMPARE(a11ySpy.first()[1], 0);
    QCOMPARE(a11ySpy.first()[2], XKB_KEY_a);
    QCOMPARE(a11ySpy.first()[3], XKB_KEY_a);
    QCOMPARE(a11ySpy.first()[4], KEY_A + 8);
    a11ySpy.clear();

    QCOMPARE(clientKeySpy.count(), 1);
    QCOMPARE(clientKeySpy.first()[0], KEY_A);
    QCOMPARE(clientKeySpy.first()[1].value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    clientKeySpy.clear();
}
}

WAYLANDTEST_MAIN(KWin::A11yKeyboardMonitorTest)
#include "a11ykeyboardmonitor_test.moc"
