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
#include "platform.h"
#include "shell_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/server_decoration.h>

#include <KWayland/Server/shell_interface.h>

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_maximized-0");

class TestMaximized : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMaximizedPassedToDeco();
    void testInitiallyMaximized();
};

void TestMaximized::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    waylandServer()->init(s_socketName.toLocal8Bit());

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestMaximized::init()
{
    QVERIFY(Test::setupWaylandConnection(s_socketName, Test::AdditionalWaylandInterface::Decoration));

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(1280, 512));
}

void TestMaximized::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestMaximized::testMaximizedPassedToDeco()
{
    // this test verifies that when a ShellClient gets maximized the Decoration receives the signal
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QScopedPointer<ServerSideDecoration> ssd(Test::waylandServerSideDecoration()->create(surface.data()));

    Test::render(surface.data(), QSize(100, 50), Qt::blue);

    QSignalSpy sizeChangedSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    QVERIFY(clientAddedSpy.isEmpty());
    QVERIFY(clientAddedSpy.wait());
    auto client = clientAddedSpy.first().first().value<ShellClient*>();
    QVERIFY(client);
    QVERIFY(client->isDecorated());
    auto decoration = client->decoration();
    QVERIFY(decoration);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);

    // now maximize
    QVERIFY(sizeChangedSpy.isEmpty());
    QSignalSpy bordersChangedSpy(decoration, &KDecoration2::Decoration::bordersChanged);
    QVERIFY(bordersChangedSpy.isValid());
    QSignalSpy maximizedChangedSpy(decoration->client().data(), &KDecoration2::DecoratedClient::maximizedChanged);
    QVERIFY(maximizedChangedSpy.isValid());
    workspace()->slotWindowMaximize();
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(maximizedChangedSpy.count(), 1);
    QCOMPARE(maximizedChangedSpy.last().first().toBool(), true);
    QCOMPARE(bordersChangedSpy.count(), 1);
    QCOMPARE(decoration->borderLeft(), 0);
    QCOMPARE(decoration->borderBottom(), 0);
    QCOMPARE(decoration->borderRight(), 0);
    QVERIFY(decoration->borderTop() != 0);

    QVERIFY(sizeChangedSpy.isEmpty());
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(sizeChangedSpy.first().first().toSize(), QSize(1280, 1024 - decoration->borderTop()));

    // now unmaximize again
    workspace()->slotWindowMaximize();
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(maximizedChangedSpy.count(), 2);
    QCOMPARE(maximizedChangedSpy.last().first().toBool(), false);
    QCOMPARE(bordersChangedSpy.count(), 2);
    QVERIFY(decoration->borderTop() != 0);
    QVERIFY(decoration->borderLeft() != 0);
    QVERIFY(decoration->borderRight() != 0);
    QVERIFY(decoration->borderBottom() != 0);

    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 2);
    QCOMPARE(sizeChangedSpy.last().first().toSize(), QSize(100, 50));
}

void TestMaximized::testInitiallyMaximized()
{
    // this test verifies that a window created as maximized, will be maximized
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));

    QSignalSpy sizeChangedSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    shellSurface->setMaximized();
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(shellSurface->size(), QSize(1280, 1024));

    // now let's render in an incorrect size
    Test::render(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(clientAddedSpy.isEmpty());
    QVERIFY(clientAddedSpy.wait());
    auto client = clientAddedSpy.first().first().value<ShellClient*>();
    QVERIFY(client);
    QCOMPARE(client->geometry(), QRect(0, 0, 100, 50));
    QEXPECT_FAIL("", "Should go out of maximzied", Continue);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QVERIFY(client->shellSurface()->isMaximized());
}

WAYLANDTEST_MAIN(TestMaximized)
#include "maximize_test.moc"
