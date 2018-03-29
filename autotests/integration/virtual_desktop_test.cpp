/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#include "main.h"
#include "platform.h"
#include "screens.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "virtualdesktops.h"

#include <KWayland/Client/surface.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_virtualdesktop-0");

class VirtualDesktopTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testNetCurrentDesktop();
    void testLastDesktopRemoved_data();
    void testLastDesktopRemoved();
};

void VirtualDesktopTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    waylandServer()->initWorkspace();

    if (kwinApp()->x11Connection()) {
        // verify the current desktop x11 property on startup, see BUG: 391034
        Xcb::Atom currentDesktopAtom("_NET_CURRENT_DESKTOP");
        QVERIFY(currentDesktopAtom.isValid());
        Xcb::Property currentDesktop(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
        bool ok = true;
        QCOMPARE(currentDesktop.value(0, &ok), 0);
        QVERIFY(ok);
    }
}

void VirtualDesktopTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
    screens()->setCurrent(0);
    VirtualDesktopManager::self()->setCount(1);
}

void VirtualDesktopTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void VirtualDesktopTest::testNetCurrentDesktop()
{
    if (!kwinApp()->x11Connection()) {
        QSKIP("Skipped on Wayland only");
    }
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
    VirtualDesktopManager::self()->setCount(4);
    QCOMPARE(VirtualDesktopManager::self()->count(), 4u);

    Xcb::Atom currentDesktopAtom("_NET_CURRENT_DESKTOP");
    QVERIFY(currentDesktopAtom.isValid());
    Xcb::Property currentDesktop(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    bool ok = true;
    QCOMPARE(currentDesktop.value(0, &ok), 0);
    QVERIFY(ok);

    // go to desktop 2
    VirtualDesktopManager::self()->setCurrent(2);
    currentDesktop = Xcb::Property(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    QCOMPARE(currentDesktop.value(0, &ok), 1);
    QVERIFY(ok);

    // go to desktop 3
    VirtualDesktopManager::self()->setCurrent(3);
    currentDesktop = Xcb::Property(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    QCOMPARE(currentDesktop.value(0, &ok), 2);
    QVERIFY(ok);

    // go to desktop 4
    VirtualDesktopManager::self()->setCurrent(4);
    currentDesktop = Xcb::Property(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    QCOMPARE(currentDesktop.value(0, &ok), 3);
    QVERIFY(ok);

    // and back to first
    VirtualDesktopManager::self()->setCurrent(1);
    currentDesktop = Xcb::Property(0, kwinApp()->x11RootWindow(), currentDesktopAtom, XCB_ATOM_CARDINAL, 0, 1);
    QCOMPARE(currentDesktop.value(0, &ok), 0);
    QVERIFY(ok);
}

void VirtualDesktopTest::testLastDesktopRemoved_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("xdgShellV6") << Test::ShellSurfaceType::XdgShellV6;
}

void VirtualDesktopTest::testLastDesktopRemoved()
{
    // first create a new desktop
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
    VirtualDesktopManager::self()->setCount(2);
    QCOMPARE(VirtualDesktopManager::self()->count(), 2u);

    // switch to last desktop
    VirtualDesktopManager::self()->setCurrent(VirtualDesktopManager::self()->desktops().last());
    QCOMPARE(VirtualDesktopManager::self()->current(), 2u);

    // now create a window on this desktop
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(client);
    QCOMPARE(client->desktop(), 2);
    QSignalSpy desktopPresenceChangedSpy(client, &ShellClient::desktopPresenceChanged);
    QVERIFY(desktopPresenceChangedSpy.isValid());

    // and remove last desktop
    VirtualDesktopManager::self()->setCount(1);
    QCOMPARE(VirtualDesktopManager::self()->count(), 1u);
    // now the client should be moved as well
    QTRY_COMPARE(desktopPresenceChangedSpy.count(), 1);
    QCOMPARE(client->desktop(), 1);
}

WAYLANDTEST_MAIN(VirtualDesktopTest)
#include "virtual_desktop_test.moc"
