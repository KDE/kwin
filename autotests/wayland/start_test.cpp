/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "abstract_backend.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_start_test-0");

class StartTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testScreens();
    void testNoWindowsAtStart();
    void testCreateWindow();
};

void StartTest::initTestCase()
{
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    waylandServer()->backend()->setInitialWindowSize(QSize(1280, 1024));
    waylandServer()->init(s_socketName.toLocal8Bit());
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
}

void StartTest::testScreens()
{
    QCOMPARE(screens()->count(), 1);
    QCOMPARE(screens()->size(), QSize(1280, 1024));
    QCOMPARE(screens()->geometry(), QRect(0, 0, 1280, 1024));
}

void StartTest::testNoWindowsAtStart()
{
    QVERIFY(workspace()->clientList().isEmpty());
    QVERIFY(workspace()->desktopList().isEmpty());
    QVERIFY(workspace()->allClientList().isEmpty());
    QVERIFY(workspace()->deletedList().isEmpty());
    QVERIFY(workspace()->unmanagedList().isEmpty());
    QVERIFY(waylandServer()->clients().isEmpty());
}

void StartTest::testCreateWindow()
{
    // first we need to connect to the server
    using namespace KWayland::Client;
    auto connection = new ConnectionThread;
    QSignalSpy connectedSpy(connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    connection->setSocketName(s_socketName);
    QThread *thread = new QThread(this);
    connection->moveToThread(thread);
    thread->start();
    connection->initConnection();
    QVERIFY(connectedSpy.wait());

    QSignalSpy shellClientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(shellClientAddedSpy.isValid());
    QSignalSpy shellClientRemovedSpy(waylandServer(), &WaylandServer::shellClientRemoved);
    QVERIFY(shellClientRemovedSpy.isValid());

    {
    EventQueue queue;
    queue.setup(connection);
    Registry registry;
    registry.setEventQueue(&queue);
    registry.create(connection);
    QSignalSpy registryAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(registryAnnouncedSpy.isValid());
    registry.setup();
    QVERIFY(registryAnnouncedSpy.wait());

    const auto compositorData = registry.interface(Registry::Interface::Compositor);
    const auto shellData = registry.interface(Registry::Interface::Shell);
    const auto shmData = registry.interface(Registry::Interface::Shm);
    QScopedPointer<KWayland::Client::Compositor> compositor(registry.createCompositor(compositorData.name, compositorData.version));
    QVERIFY(!compositor.isNull());
    QScopedPointer<Shell> shell(registry.createShell(shellData.name, shellData.version));
    QVERIFY(!shell.isNull());

    QScopedPointer<Surface> surface(compositor->createSurface());
    QVERIFY(!surface.isNull());
    QSignalSpy surfaceRenderedSpy(surface.data(), &Surface::frameRendered);
    QVERIFY(surfaceRenderedSpy.isValid());

    QScopedPointer<ShellSurface> shellSurface(shell->createSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    connection->flush();
    QVERIFY(waylandServer()->clients().isEmpty());
    // now dispatch should give us the client
    waylandServer()->dispatch();
    QCOMPARE(waylandServer()->clients().count(), 1);
    // but still not yet in workspace
    QVERIFY(workspace()->allClientList().isEmpty());

    // let's render
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QScopedPointer<ShmPool> shm(registry.createShmPool(shmData.name, shmData.version));
    QVERIFY(!shm.isNull());
    surface->attachBuffer(shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit();

    connection->flush();
    QVERIFY(shellClientAddedSpy.wait());
    QCOMPARE(workspace()->allClientList().count(), 1);
    QCOMPARE(workspace()->allClientList().first(), waylandServer()->clients().first());
    QVERIFY(workspace()->activeClient());
    QCOMPARE(workspace()->activeClient()->pos(), QPoint(0, 0));
    QCOMPARE(workspace()->activeClient()->size(), QSize(100, 50));
    QCOMPARE(workspace()->activeClient()->geometry(), QRect(0, 0, 100, 50));

    // and kwin will render it
    QVERIFY(surfaceRenderedSpy.wait());
    }
    // this should tear down everything again
    QVERIFY(shellClientRemovedSpy.wait());
    QVERIFY(waylandServer()->clients().isEmpty());

    // cleanup
    connection->deleteLater();
    thread->quit();
    thread->wait();
}

}

WAYLANDTEST_MAIN(KWin::StartTest)
#include "start_test.moc"
