/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/outputbackend.h"
#include "input.h"
#include "pointer_input.h"
#include "tabbox/tabbox.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_tabbox-0");

class TabBoxTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMoveForward();
    void testMoveBackward();
    void testCapsLock();
    void testKeyboardFocus();
    void testActiveClientOutsideModel();
};

void TabBoxTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    KSharedConfigPtr c = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    c->group(QStringLiteral("TabBox")).writeEntry("ShowTabBox", false);
    c->sync();
    kwinApp()->setConfig(c);
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");

    kwinApp()->start();
}

void TabBoxTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::input()->pointer()->warp(QPoint(640, 512));
}

void TabBoxTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void TabBoxTest::testCapsLock()
{
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    // this test verifies that Alt+tab works correctly also when Capslock is on
    // bug 368590

    // first create three windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto c1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    auto c3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // enable capslock
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    Test::keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::NoModifier);

    // press alt+tab
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(workspace()->tabbox()->isGrabbed());

    // release alt
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);

    // release caps lock
    Test::keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    Test::keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::NoModifier);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);
    QCOMPARE(workspace()->activeWindow(), c2);

    surface3.reset();
    QVERIFY(Test::waitForWindowClosed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowClosed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowClosed(c1));
}

void TabBoxTest::testMoveForward()
{
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    // this test verifies that Alt+tab works correctly moving forward

    // first create three windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto c1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    auto c3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // press alt+tab
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(workspace()->tabbox()->isGrabbed());

    // release alt
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);
    QCOMPARE(workspace()->activeWindow(), c2);

    surface3.reset();
    QVERIFY(Test::waitForWindowClosed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowClosed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowClosed(c1));
}

void TabBoxTest::testMoveBackward()
{
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    // this test verifies that Alt+Shift+tab works correctly moving backward

    // first create three windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto c1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    auto c3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // press alt+shift+tab
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    Test::keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier | Qt::ShiftModifier);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(workspace()->tabbox()->isGrabbed());

    // release alt
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 0);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);
    QCOMPARE(workspace()->activeWindow(), c1);

    surface3.reset();
    QVERIFY(Test::waitForWindowClosed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowClosed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowClosed(c1));
}

void TabBoxTest::testKeyboardFocus()
{
    // This test verifies that the keyboard focus will be withdrawn from the currently activated
    // window when the task switcher is active and restored once the task switcher is dismissed.

    QVERIFY(Test::waitForWaylandKeyboard());

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy leftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);

    // add a window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    // the keyboard focus will be moved to the surface after it's mapped
    QVERIFY(enteredSpy.wait());

    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // press alt+tab
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);
    QVERIFY(tabboxAddedSpy.wait());

    // the surface should have no keyboard focus anymore because tabbox grabs input
    QCOMPARE(leftSpy.count(), 1);

    // release alt
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);

    // the surface should regain keyboard focus after the tabbox is dismissed
    QVERIFY(enteredSpy.wait());
}

void TabBoxTest::testActiveClientOutsideModel()
{
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    // This test verifies behaviour when the active client is outside the
    // client list model:
    //
    // 1) reset() should correctly set the index to 0 if the active window is
    //    not part of the client list.
    // 2) the selection should not be advanced initially if the active window
    //    is not part of the client list.

    const auto outputs = kwinApp()->outputBackend()->outputs();

    // Initially, set up MultiScreenMode such that alt+tab will only switch
    // within windows on the same screen.
    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("TabBox"));
    group.writeEntry("MultiScreenMode", "1");
    group.sync();
    workspace()->slotReconfigure();

    // Create a window on the left output
    std::unique_ptr<KWayland::Client::Surface> leftSurface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> leftShellSurface1(Test::createXdgToplevelSurface(leftSurface1.get()));
    auto l1 = Test::renderAndWaitForShown(leftSurface1.get(), QSize(100, 50), Qt::blue);
    l1->move(QPointF(50, 100));
    QVERIFY(l1);
    QVERIFY(l1->isActive());
    QCOMPARE(l1->output(), outputs[0]);

    // Create three windows on the right output
    std::unique_ptr<KWayland::Client::Surface> rightSurface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> rightShellSurface1(Test::createXdgToplevelSurface(rightSurface1.get()));
    auto r1 = Test::renderAndWaitForShown(rightSurface1.get(), QSize(100, 50), Qt::blue);
    r1->move(QPointF(1280 + 50, 100));
    QVERIFY(r1);
    QVERIFY(r1->isActive());
    QCOMPARE(r1->output(), outputs[1]);
    std::unique_ptr<KWayland::Client::Surface> rightSurface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> rightShellSurface2(Test::createXdgToplevelSurface(rightSurface2.get()));
    auto r2 = Test::renderAndWaitForShown(rightSurface2.get(), QSize(100, 50), Qt::red);
    r2->move(QPointF(1280 + 50, 100));
    QVERIFY(r2);
    QVERIFY(r2->isActive());
    QCOMPARE(r2->output(), outputs[1]);
    std::unique_ptr<KWayland::Client::Surface> rightSurface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> rightShellSurface3(Test::createXdgToplevelSurface(rightSurface3.get()));
    auto r3 = Test::renderAndWaitForShown(rightSurface3.get(), QSize(100, 50), Qt::red);
    r3->move(QPointF(1280 + 50, 100));
    QVERIFY(r3);
    QVERIFY(r3->isActive());
    QCOMPARE(r3->output(), outputs[1]);

    // Focus r3 such that we're on the right output
    input()->pointer()->warp(r3->frameGeometry().center());
    QCOMPARE(workspace()->activeOutput(), outputs[1]);

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // Press Alt+Tab, this will only show clients on the same output
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(workspace()->tabbox()->isGrabbed());

    // Release Alt+Tab. This will have moved our index to 1 and focused r2 (the
    // previously created window)
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);
    QCOMPARE(workspace()->activeWindow(), r2);

    // Now reconfigure MultiScreenMode such that alt+tab will only switch
    // between windows on the other screen
    group.writeEntry("MultiScreenMode", 2);
    group.sync();
    workspace()->slotReconfigure();

    // Activate and focus l1 to switch to the left output
    workspace()->activateWindow(l1);
    QCOMPARE(workspace()->activeWindow(), l1);
    input()->pointer()->warp(l1->frameGeometry().center());
    QCOMPARE(workspace()->activeOutput(), outputs[0]);

    // Press Alt+Tab, this will show only clients on the other output. Our old
    // index from the last invocation of tabbox should be reset to 0 since the
    // active window (l1) cannot be located in the current client list
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(workspace()->tabbox()->isGrabbed());

    // Release Alt. With a correctly reset index we should start from the
    // beginning, skip advancing one window and focus r2 - the last window in
    // focus on the other output
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 2);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);
    QCOMPARE(workspace()->activeWindow(), r2);

    rightSurface3.reset();
    QVERIFY(Test::waitForWindowClosed(r3));
    rightSurface2.reset();
    QVERIFY(Test::waitForWindowClosed(r2));
    rightSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(r1));
    leftSurface1.reset();
    QVERIFY(Test::waitForWindowClosed(l1));
}

WAYLANDTEST_MAIN(TabBoxTest)
#include "tabbox_test.moc"
