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

static const QString s_socketName = QStringLiteral("wayland_test_kwin_sticky_keys-0");

class StickyKeysTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testStick();
    void testLock();
};

void StickyKeysTest::initTestCase()
{
    KConfig kaccessConfig("kaccessrc");
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("StickyKeys", true);
    kaccessConfig.sync();

    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void StickyKeysTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandKeyboard());
}

void StickyKeysTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void StickyKeysTest::testStick()
{
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(10, 10), Qt::blue);
    QVERIFY(waylandWindow);

    QSignalSpy modifierSpy(keyboard.get(), &KWayland::Client::Keyboard::modifiersChanged);
    QVERIFY(modifierSpy.wait());
    modifierSpy.clear();

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
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that Ctrl is not depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that Ctrl is not latched any more

    Test::keyboardKeyReleased(KEY_A, ++timestamp);
}

void StickyKeysTest::testLock()
{
    KConfig kaccessConfig("kaccessrc");
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("StickyKeysLatch", true);
    kaccessConfig.sync();

    // reload the plugin to pick up the new config
    kwinApp()->pluginManager()->unloadPlugin("StickyKeysPlugin");
    kwinApp()->pluginManager()->loadPlugin("StickyKeysPlugin");

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

    // press Ctrl again to lock it
    modifierSpy.clear();
    Test::keyboardKeyPressed(KEY_LEFTCTRL, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 4); // verify that Ctrl is depressed
    // TODO should it be latched?
    QCOMPARE(modifierSpy.first()[2], 4); // verify that Ctrl is locked

    // press and release a letter, this does not unlock the modifier
    modifierSpy.clear();
    Test::keyboardKeyPressed(KEY_A, ++timestamp);
    QVERIFY(!modifierSpy.wait(10));

    Test::keyboardKeyReleased(KEY_A, ++timestamp);
    QVERIFY(!modifierSpy.wait(10));

    // press Ctrl again to unlock it
    Test::keyboardKeyPressed(KEY_LEFTCTRL, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 4); // verify that Ctrl is depressed
    QCOMPARE(modifierSpy.first()[2], 0); // verify that Ctrl is locked

    Test::keyboardKeyReleased(KEY_LEFTCTRL, ++timestamp);
}
}

WAYLANDTEST_MAIN(KWin::StickyKeysTest)
#include "sticky_keys_test.moc"
