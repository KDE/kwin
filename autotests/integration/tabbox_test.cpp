/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "cursor.h"
#include "input.h"
#include "platform.h"
#include "screens.h"
#include "tabbox/tabbox.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>
#include <KConfigGroup>

#include <linux/input.h>

using namespace KWin;
using namespace KWayland::Client;

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
};

void TabBoxTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    KSharedConfigPtr c = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    c->group("TabBox").writeEntry("ShowTabBox", false);
    c->sync();
    kwinApp()->setConfig(c);
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    waylandServer()->initWorkspace();
}

void TabBoxTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
    screens()->setCurrent(0);
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void TabBoxTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void TabBoxTest::testCapsLock()
{
    // this test verifies that Alt+tab works correctly also when Capslock is on
    // bug 368590

    // first create three windows
    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface1(Test::createXdgShellStableSurface(surface1.data()));
    auto c1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    QScopedPointer<Surface> surface3(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface3(Test::createXdgShellStableSurface(surface3.data()));
    auto c3 = Test::renderAndWaitForShown(surface3.data(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(TabBox::TabBox::self(), &TabBox::TabBox::tabBoxAdded);
    QVERIFY(tabboxAddedSpy.isValid());
    QSignalSpy tabboxClosedSpy(TabBox::TabBox::self(), &TabBox::TabBox::tabBoxClosed);
    QVERIFY(tabboxClosedSpy.isValid());

    // enable capslock
    quint32 timestamp = 0;
    kwinApp()->platform()->keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier);

    // press alt+tab
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier | Qt::AltModifier);
    kwinApp()->platform()->keyboardKeyPressed(KEY_TAB, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(TabBox::TabBox::self()->isGrabbed());

    // release alt
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(TabBox::TabBox::self()->isGrabbed(), false);

    // release caps lock
    kwinApp()->platform()->keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::NoModifier);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(TabBox::TabBox::self()->isGrabbed(), false);
    QCOMPARE(workspace()->activeClient(), c2);

    surface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(c1));
}

void TabBoxTest::testMoveForward()
{
    // this test verifies that Alt+tab works correctly moving forward

    // first create three windows
    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface1(Test::createXdgShellStableSurface(surface1.data()));
    auto c1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    QScopedPointer<Surface> surface3(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface3(Test::createXdgShellStableSurface(surface3.data()));
    auto c3 = Test::renderAndWaitForShown(surface3.data(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(TabBox::TabBox::self(), &TabBox::TabBox::tabBoxAdded);
    QVERIFY(tabboxAddedSpy.isValid());
    QSignalSpy tabboxClosedSpy(TabBox::TabBox::self(), &TabBox::TabBox::tabBoxClosed);
    QVERIFY(tabboxClosedSpy.isValid());

    // press alt+tab
    quint32 timestamp = 0;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    kwinApp()->platform()->keyboardKeyPressed(KEY_TAB, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(TabBox::TabBox::self()->isGrabbed());

    // release alt
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(TabBox::TabBox::self()->isGrabbed(), false);
    QCOMPARE(workspace()->activeClient(), c2);

    surface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(c1));
}

void TabBoxTest::testMoveBackward()
{
    // this test verifies that Alt+Shift+tab works correctly moving backward

    // first create three windows
    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface1(Test::createXdgShellStableSurface(surface1.data()));
    auto c1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    QScopedPointer<Surface> surface3(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface3(Test::createXdgShellStableSurface(surface3.data()));
    auto c3 = Test::renderAndWaitForShown(surface3.data(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(TabBox::TabBox::self(), &TabBox::TabBox::tabBoxAdded);
    QVERIFY(tabboxAddedSpy.isValid());
    QSignalSpy tabboxClosedSpy(TabBox::TabBox::self(), &TabBox::TabBox::tabBoxClosed);
    QVERIFY(tabboxClosedSpy.isValid());

    // press alt+shift+tab
    quint32 timestamp = 0;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier | Qt::ShiftModifier);
    kwinApp()->platform()->keyboardKeyPressed(KEY_TAB, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(TabBox::TabBox::self()->isGrabbed());

    // release alt
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 0);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(TabBox::TabBox::self()->isGrabbed(), false);
    QCOMPARE(workspace()->activeClient(), c1);

    surface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(c1));
}

WAYLANDTEST_MAIN(TabBoxTest)
#include "tabbox_test.moc"
