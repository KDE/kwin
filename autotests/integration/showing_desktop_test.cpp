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
#include "platform.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/shell.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_showing_desktop-0");

class ShowingDesktopTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testRestoreFocus();
    void testRestoreFocusWithDesktopWindow();
};

void ShowingDesktopTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    waylandServer()->initWorkspace();
}

void ShowingDesktopTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PlasmaShell));
}

void ShowingDesktopTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ShowingDesktopTest::testRestoreFocus()
{
    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface1(Test::createShellSurface(surface1.data()));
    auto client1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::blue);
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface2(Test::createShellSurface(surface2.data()));
    auto client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client1 != client2);

    QCOMPARE(workspace()->activeClient(), client2);
    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());

    QVERIFY(workspace()->activeClient());
    QCOMPARE(workspace()->activeClient(), client2);
}

void ShowingDesktopTest::testRestoreFocusWithDesktopWindow()
{
    // first create a desktop window

    QScopedPointer<Surface> desktopSurface(Test::createSurface());
    QVERIFY(!desktopSurface.isNull());
    QScopedPointer<ShellSurface> desktopShellSurface(Test::createShellSurface(desktopSurface.data()));
    QVERIFY(!desktopSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(desktopSurface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::Desktop);

    auto desktop = Test::renderAndWaitForShown(desktopSurface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(desktop);
    QVERIFY(desktop->isDesktop());

    // now create some windows
    QScopedPointer<Surface> surface1(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface1(Test::createShellSurface(surface1.data()));
    auto client1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::blue);
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface2(Test::createShellSurface(surface2.data()));
    auto client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client1 != client2);

    QCOMPARE(workspace()->activeClient(), client2);
    workspace()->slotToggleShowDesktop();
    QVERIFY(workspace()->showingDesktop());
    QCOMPARE(workspace()->activeClient(), desktop);
    workspace()->slotToggleShowDesktop();
    QVERIFY(!workspace()->showingDesktop());

    QVERIFY(workspace()->activeClient());
    QCOMPARE(workspace()->activeClient(), client2);
}

WAYLANDTEST_MAIN(ShowingDesktopTest)
#include "showing_desktop_test.moc"
