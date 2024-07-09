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
    void testStick_data();
    void testLock();
    void testLock_data();
    void testMouse();
    void testMouse_data();
    void testDisableTwoKeys();
};

void StickyKeysTest::initTestCase()
{
    KConfig kaccessConfig("kaccessrc");
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("StickyKeys", true);
    kaccessConfig.group(QStringLiteral("Keyboard")).writeEntry("StickyKeysAutoOff", true);
    kaccessConfig.sync();

    // Use a keyboard layout where right alt triggers Mod5/AltGr
    KConfig kxkbrc("kxkbrc");
    kxkbrc.group(QStringLiteral("Layout")).writeEntry("LayoutList", "us");
    kxkbrc.group(QStringLiteral("Layout")).writeEntry("VariantList", "altgr-intl");
    kxkbrc.sync();

    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
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

void StickyKeysTest::testStick_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<int>("expectedMods");

    QTest::addRow("Shift") << KEY_LEFTSHIFT << 1;
    QTest::addRow("Ctrl") << KEY_LEFTCTRL << 4;
    QTest::addRow("Alt") << KEY_LEFTALT << 8;
    QTest::addRow("AltGr") << KEY_RIGHTALT << 128;
}

void StickyKeysTest::testStick()
{
    QFETCH(int, modifierKey);
    QFETCH(int, expectedMods);

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

    // press mod to latch it
    Test::keyboardKeyPressed(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    // arguments are: quint32 depressed, quint32 latched, quint32 locked, quint32 group
    QCOMPARE(modifierSpy.first()[0], expectedMods); // verify that mod is depressed
    QCOMPARE(modifierSpy.first()[1], expectedMods); // verify that mod is latched

    modifierSpy.clear();
    // release mod, the modifier should still be latched
    Test::keyboardKeyReleased(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that mod is not depressed
    QCOMPARE(modifierSpy.first()[1], expectedMods); // verify that mod is still latched

    // press and release a letter, this unlatches the modifier
    modifierSpy.clear();
    Test::keyboardKeyPressed(KEY_A, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that mod is not depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that mod is not latched any more

    Test::keyboardKeyReleased(KEY_A, ++timestamp);
}

void StickyKeysTest::testLock_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<int>("expectedMods");

    QTest::addRow("Shift") << KEY_LEFTSHIFT << 1;
    QTest::addRow("Ctrl") << KEY_LEFTCTRL << 4;
    QTest::addRow("Alt") << KEY_LEFTALT << 8;
    QTest::addRow("AltGr") << KEY_RIGHTALT << 128;
}

void StickyKeysTest::testLock()
{
    QFETCH(int, modifierKey);
    QFETCH(int, expectedMods);

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

    // press mod to latch it
    Test::keyboardKeyPressed(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    // arguments are: quint32 depressed, quint32 latched, quint32 locked, quint32 group
    QCOMPARE(modifierSpy.first()[0], expectedMods); // verify that mod is depressed
    QCOMPARE(modifierSpy.first()[1], expectedMods); // verify that mod is latched

    modifierSpy.clear();
    // release mod, the modifier should still be latched
    Test::keyboardKeyReleased(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that mod is not depressed
    QCOMPARE(modifierSpy.first()[1], expectedMods); // verify that mod is still latched

    // press mod again to lock it
    modifierSpy.clear();
    Test::keyboardKeyPressed(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], expectedMods); // verify that mod is depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that mod is unlatched
    QCOMPARE(modifierSpy.first()[2], expectedMods); // verify that mod is locked

    // release mod, modifier should still be locked
    modifierSpy.clear();
    Test::keyboardKeyReleased(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    // TODO

    // press and release a letter, this does not unlock the modifier
    modifierSpy.clear();
    Test::keyboardKeyPressed(KEY_A, ++timestamp);
    QVERIFY(!modifierSpy.wait(10));

    Test::keyboardKeyReleased(KEY_A, ++timestamp);
    QVERIFY(!modifierSpy.wait(10));

    // press mod again to unlock it
    Test::keyboardKeyPressed(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], expectedMods); // verify that mod is depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that mod is unlatched
    QCOMPARE(modifierSpy.first()[2], 0); // verify that mod is not locked

    Test::keyboardKeyReleased(modifierKey, ++timestamp);
}

void StickyKeysTest::testDisableTwoKeys()
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

    // press mod to latch it
    Test::keyboardKeyPressed(KEY_LEFTSHIFT, ++timestamp);
    QVERIFY(modifierSpy.wait());
    // arguments are: quint32 depressed, quint32 latched, quint32 locked, quint32 group
    QCOMPARE(modifierSpy.first()[0], 1); // verify that mod is depressed
    QCOMPARE(modifierSpy.first()[1], 1); // verify that mod is latched
    modifierSpy.clear();

    // press key while modifier is pressed, this disables sticky keys
    Test::keyboardKeyPressed(KEY_A, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 1); // verify that mod is depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that mod is not latched any more
    modifierSpy.clear();

    Test::keyboardKeyReleased(KEY_A, ++timestamp);

    // release mod, the modifier should not be depressed or latched
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that mod is not depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that mod is not latched
    modifierSpy.clear();

    // verify that sticky keys are not enabled any more

    // press mod, should be depressed but not latched
    Test::keyboardKeyPressed(KEY_LEFTCTRL, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 4); // verify that mod is depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that mod is not latched
    modifierSpy.clear();

    // release mod, should not be depressed any more
    Test::keyboardKeyReleased(KEY_LEFTCTRL, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that mod is not depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that mod is not latched
    modifierSpy.clear();

    Test::keyboardKeyPressed(KEY_A, ++timestamp);
    QVERIFY(!modifierSpy.wait(10));
    Test::keyboardKeyReleased(KEY_A, ++timestamp);
    QVERIFY(!modifierSpy.wait(10));
}

void StickyKeysTest::testMouse_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<int>("expectedMods");

    QTest::addRow("Shift") << KEY_LEFTSHIFT << 1;
    QTest::addRow("Ctrl") << KEY_LEFTCTRL << 4;
    QTest::addRow("Alt") << KEY_LEFTALT << 8;
    QTest::addRow("AltGr") << KEY_RIGHTALT << 128;
}

void StickyKeysTest::testMouse()
{
    QFETCH(int, modifierKey);
    QFETCH(int, expectedMods);

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

    // press mod to latch it
    Test::keyboardKeyPressed(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    // arguments are: quint32 depressed, quint32 latched, quint32 locked, quint32 group
    QCOMPARE(modifierSpy.first()[0], expectedMods); // verify that mod is depressed
    QCOMPARE(modifierSpy.first()[1], expectedMods); // verify that mod is latched

    modifierSpy.clear();
    // release mod, the modifier should still be latched
    Test::keyboardKeyReleased(modifierKey, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that mod is not depressed
    QCOMPARE(modifierSpy.first()[1], expectedMods); // verify that mod is still latched

    // press and release a mouse button, this unlatches the modifier
    modifierSpy.clear();
    Test::pointerButtonPressed(BTN_LEFT, ++timestamp);
    QVERIFY(!modifierSpy.wait(10));
    Test::pointerButtonReleased(BTN_LEFT, ++timestamp);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.first()[0], 0); // verify that mod is not depressed
    QCOMPARE(modifierSpy.first()[1], 0); // verify that mod is not latched any more
}
}

WAYLANDTEST_MAIN(KWin::StickyKeysTest)
#include "sticky_keys_test.moc"
