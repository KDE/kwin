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

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/server_decoration.h>

#include <KWayland/Server/shell_interface.h>

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

#include <QThread>

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

private:
    KWayland::Client::Compositor *m_compositor = nullptr;
    Shell *m_shell = nullptr;
    ShmPool *m_shm = nullptr;
    EventQueue *m_queue = nullptr;
    ServerSideDecorationManager *m_ssd = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
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
    // setup connection
    m_connection = new ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());

    m_compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name,
                                             registry.interface(Registry::Interface::Compositor).version,
                                             this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(registry.interface(Registry::Interface::Shm).name,
                                   registry.interface(Registry::Interface::Shm).version,
                                   this);
    QVERIFY(m_shm->isValid());
    m_shell = registry.createShell(registry.interface(Registry::Interface::Shell).name,
                                   registry.interface(Registry::Interface::Shell).version,
                                   this);
    QVERIFY(m_shell->isValid());
    m_ssd = registry.createServerSideDecorationManager(registry.interface(Registry::Interface::ServerSideDecorationManager).name,
                                                       registry.interface(Registry::Interface::ServerSideDecorationManager).version,
                                                       this);
    QVERIFY(m_ssd);

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(1280, 512));
}

void TestMaximized::cleanup()
{
#define CLEANUP(name) \
    if (name) { \
        delete name; \
        name = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_shm)
    CLEANUP(m_shell)
    CLEANUP(m_ssd)
    CLEANUP(m_queue)

    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
#undef CLEANUP
}

void TestMaximized::testMaximizedPassedToDeco()
{
    // this test verifies that when a ShellClient gets maximized the Decoration receives the signal
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));
    QScopedPointer<ServerSideDecoration> ssd(m_ssd->create(surface.data()));

    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit(Surface::CommitFlag::None);

    QVERIFY(clientAddedSpy.isEmpty());
    QVERIFY(clientAddedSpy.wait());
    auto client = clientAddedSpy.first().first().value<ShellClient*>();
    QVERIFY(client);
    QVERIFY(client->isDecorated());
    auto decoration = client->decoration();
    QVERIFY(decoration);
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);

    // now maximize
    QSignalSpy maximizedChangedSpy(decoration->client().data(), &KDecoration2::DecoratedClient::maximizedChanged);
    QVERIFY(maximizedChangedSpy.isValid());
    workspace()->slotWindowMaximize();
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeFull);
    QCOMPARE(maximizedChangedSpy.count(), 1);
    QCOMPARE(maximizedChangedSpy.last().first().toBool(), true);

    // now unmaximize again
    workspace()->slotWindowMaximize();
    QCOMPARE(client->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(maximizedChangedSpy.count(), 2);
    QCOMPARE(maximizedChangedSpy.last().first().toBool(), false);
}

void TestMaximized::testInitiallyMaximized()
{
    // this test verifies that a window created as maximized, will be maximized
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));

    QSignalSpy sizeChangedSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    shellSurface->setMaximized();
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(shellSurface->size(), QSize(1280, 1024));

    // now let's render in an incorrect size
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit(Surface::CommitFlag::None);

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
