/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "cursor.h"
#include "input.h"
#include "platform.h"
#include "screens.h"
#include "shell_client.h"
#include "useractions.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/shell.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Server/seat_interface.h>

#include <KGlobalAccel>
#include <linux/input.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_globalshortcuts-0");

class GlobalShortcutsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testConsumedShift();
    void testRepeatedTrigger();
    void testUserActionsMenu();
};

void GlobalShortcutsTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    waylandServer()->initWorkspace();
}

void GlobalShortcutsTest::init()
{
    QVERIFY(Test::setupWaylandConnection(s_socketName));
    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(640, 512));
}

void GlobalShortcutsTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void GlobalShortcutsTest::testConsumedShift()
{
    // this test verifies that a shortcut with a consumed shift modifier triggers
    // create the action
    QScopedPointer<QAction> action(new QAction(nullptr));
    action->setProperty("componentName", QStringLiteral(KWIN_NAME));
    action->setObjectName(QStringLiteral("globalshortcuts-test-consumed-shift"));
    QSignalSpy triggeredSpy(action.data(), &QAction::triggered);
    QVERIFY(triggeredSpy.isValid());
    KGlobalAccel::self()->setShortcut(action.data(), QList<QKeySequence>{Qt::Key_Percent}, KGlobalAccel::NoAutoloading);
    input()->registerShortcut(Qt::Key_Percent, action.data());

    // press shift+5
    quint32 timestamp = 0;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier);
    kwinApp()->platform()->keyboardKeyPressed(KEY_5, timestamp++);
    QTRY_COMPARE(triggeredSpy.count(), 1);
    kwinApp()->platform()->keyboardKeyReleased(KEY_5, timestamp++);

    // release shift
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
}

void GlobalShortcutsTest::testRepeatedTrigger()
{
    // this test verifies that holding a key, triggers repeated global shortcut
    // in addition pressing another key should stop triggering the shortcut

    QScopedPointer<QAction> action(new QAction(nullptr));
    action->setProperty("componentName", QStringLiteral(KWIN_NAME));
    action->setObjectName(QStringLiteral("globalshortcuts-test-consumed-shift"));
    QSignalSpy triggeredSpy(action.data(), &QAction::triggered);
    QVERIFY(triggeredSpy.isValid());
    KGlobalAccel::self()->setShortcut(action.data(), QList<QKeySequence>{Qt::Key_Percent}, KGlobalAccel::NoAutoloading);
    input()->registerShortcut(Qt::Key_Percent, action.data());

    // we need to configure the key repeat first. It is only enabled on libinput
    waylandServer()->seat()->setKeyRepeatInfo(25, 300);

    // press shift+5
    quint32 timestamp = 0;
    kwinApp()->platform()->keyboardKeyPressed(KEY_WAKEUP, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::ShiftModifier);
    kwinApp()->platform()->keyboardKeyPressed(KEY_5, timestamp++);
    QTRY_COMPARE(triggeredSpy.count(), 1);
    // and should repeat
    QVERIFY(triggeredSpy.wait());
    QVERIFY(triggeredSpy.wait());
    // now release the key
    kwinApp()->platform()->keyboardKeyReleased(KEY_5, timestamp++);
    QVERIFY(!triggeredSpy.wait(500));

    kwinApp()->platform()->keyboardKeyReleased(KEY_WAKEUP, timestamp++);
    QVERIFY(!triggeredSpy.wait(500));

    // release shift
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
}

void GlobalShortcutsTest::testUserActionsMenu()
{
    // this test tries to trigger the user actions menu with Alt+F3
    // the problem here is that pressing F3 consumes modifiers as it's part of the
    // Ctrl+alt+F3 keysym for vt switching. xkbcommon considers all modifiers as consumed
    // which a transformation to any keysym would cause
    // for more information see:
    // https://bugs.freedesktop.org/show_bug.cgi?id=92818
    // https://github.com/xkbcommon/libxkbcommon/issues/17

    // first create a window
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QVERIFY(c->isActive());

    quint32 timestamp = 0;
    QVERIFY(!workspace()->userActionsMenu()->isShown());
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_F3, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_F3, timestamp++);
    QTRY_VERIFY(workspace()->userActionsMenu()->isShown());
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);
}

WAYLANDTEST_MAIN(GlobalShortcutsTest)
#include "globalshortcuts_test.moc"
