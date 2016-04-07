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
#include "abstract_client.h"
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

static const QString s_socketName = QStringLiteral("wayland_test_kwin_transient_no_input-0");

class TransientNoInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testTransientNoFocus();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void TransientNoInputTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    waylandServer()->init(s_socketName.toLocal8Bit());
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
}

void TransientNoInputTest::init()
{
    using namespace KWayland::Client;
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
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QSignalSpy shmSpy(&registry, &Registry::shmAnnounced);
    QSignalSpy shellSpy(&registry, &Registry::shellAnnounced);
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    QVERIFY(shmSpy.isValid());
    QVERIFY(shellSpy.isValid());
    QVERIFY(compositorSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QVERIFY(!compositorSpy.isEmpty());
    QVERIFY(!shmSpy.isEmpty());
    QVERIFY(!shellSpy.isEmpty());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());
    m_shell = registry.createShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shell->isValid());
}

void TransientNoInputTest::cleanup()
{
    delete m_compositor;
    m_compositor = nullptr;
    delete m_shm;
    m_shm = nullptr;
    delete m_shell;
    m_shell = nullptr;
    delete m_queue;
    m_queue = nullptr;
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

}

void TransientNoInputTest::testTransientNoFocus()
{
    using namespace KWayland::Client;

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    // let's render
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit();

    m_connection->flush();
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *c = workspace()->activeClient();
    QVERIFY(c);
    QCOMPARE(clientAddedSpy.first().first().value<ShellClient*>(), c);

    // let's create a transient with no input
    QScopedPointer<Surface> transientSurface(m_compositor->createSurface());
    QVERIFY(!transientSurface.isNull());
    QScopedPointer<ShellSurface> transientShellSurface(m_shell->createSurface(transientSurface.data()));
    QVERIFY(!transientShellSurface.isNull());
    transientShellSurface->setTransient(surface.data(), QPoint(10, 20), ShellSurface::TransientFlag::NoFocus);
    m_connection->flush();
    // let's render
    QImage img2(QSize(200, 20), QImage::Format_ARGB32);
    img2.fill(Qt::red);
    transientSurface->attachBuffer(m_shm->createBuffer(img2));
    transientSurface->damage(QRect(0, 0, 200, 20));
    transientSurface->commit();
    m_connection->flush();
    QVERIFY(clientAddedSpy.wait());
    // get the latest ShellClient
    auto transientClient = clientAddedSpy.last().first().value<ShellClient*>();
    QVERIFY(transientClient != c);
    QCOMPARE(transientClient->geometry(), QRect(c->x() + 10, c->y() + 20, 200, 20));
    QVERIFY(transientClient->isTransient());
    QCOMPARE(transientClient->transientPlacementHint(), QPoint(10, 20));
    QVERIFY(!transientClient->wantsInput());

    // workspace's active window should not have changed
    QCOMPARE(workspace()->activeClient(), c);
}

}

WAYLANDTEST_MAIN(KWin::TransientNoInputTest)
#include "transient_no_input_test.moc"
