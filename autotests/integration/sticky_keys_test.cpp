/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include "keyboard_input.h"

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_pointer_input-0");

class StickyKeysTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testStick();

private:
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
};

void StickyKeysTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    KConfig kaccessConfig("kaccessrc");
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("StickyKeys", true);
    kaccessConfig.sync();

    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void StickyKeysTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat /*| Test::AdditionalWaylandInterface::XdgDecorationV1 | Test::AdditionalWaylandInterface::CursorShapeV1*/));
    QVERIFY(Test::waitForWaylandPointer());
    m_compositor = Test::waylandCompositor();
    m_seat = Test::waylandSeat();
}

void StickyKeysTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void StickyKeysTest::testStick()
{
    QVERIFY(Test::waylandSeat()->hasKeyboard());
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(10, 10), Qt::blue);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPoint(0, 0));

    QSignalSpy modifierSpy(keyboard.get(), &KWayland::Client::Keyboard::modifiersChanged);
    QVERIFY(modifierSpy.wait());
    modifierSpy.clear();

    QSignalSpy keySpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    quint32 timestamp = 0;

    // press Ctrl to latch it
    Test::keyboardKeyPressed(KEY_LEFTCTRL, ++timestamp);
    QVERIFY(modifierSpy.wait());
    // arguments are: quint32 depressed, quint32 latched, quint32 locked, quint32 group
    QCOMPARE(modifierSpy.first()[0], 4); // verify that Ctrl is depressed
    QCOMPARE(modifierSpy.first()[1], 4); // verify that Ctrl is latched

    modifierSpy.clear();
    // release Ctrl, the modified should still be latched
    Test::keyboardKeyReleased(KEY_LEFTCTRL, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that Ctrl is not depressed
    QCOMPARE(modifierSpy.first()[1], 4); // verify that Ctrl is still latched

    // press and release a letter, this unlatches the modifier
    modifierSpy.clear();
    Test::keyboardKeyPressed(KEY_A, ++timestamp);
    // QVERIFY(keySpy.wait());

    modifierSpy.clear();
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that Ctrl is not depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that Ctrl is not latched any more

    Test::keyboardKeyReleased(KEY_A, ++timestamp);
    // QVERIFY(keySpy.wait());
}
}

WAYLANDTEST_MAIN(KWin::StickyKeysTest)
#include "sticky_keys_test.moc"
