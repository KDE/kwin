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
#include "platform.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "workspace.h"
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/surface.h>

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_plasma_surface-0");

class PlasmaSurfaceTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testRoleOnAllDesktops_data();
    void testRoleOnAllDesktops();
    void testAcceptsFocus_data();
    void testAcceptsFocus();

private:
    ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    ShmPool *m_shm = nullptr;
    Shell *m_shell = nullptr;
    PlasmaShell *m_plasmaShell = nullptr;
    EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void PlasmaSurfaceTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    waylandServer()->init(s_socketName.toLocal8Bit());
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
}

void PlasmaSurfaceTest::init()
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
    m_plasmaShell = registry.createPlasmaShell(registry.interface(Registry::Interface::PlasmaShell).name,
                                         registry.interface(Registry::Interface::PlasmaShell).version,
                                         this);
    QVERIFY(m_plasmaShell->isValid());
}

void PlasmaSurfaceTest::cleanup()
{
#define CLEANUP(name) delete name; name = nullptr;
    CLEANUP(m_plasmaShell)
    CLEANUP(m_shell)
    CLEANUP(m_shm)
    CLEANUP(m_compositor)
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

void PlasmaSurfaceTest::testRoleOnAllDesktops_data()
{
    QTest::addColumn<PlasmaShellSurface::Role>("role");
    QTest::addColumn<bool>("expectedOnAllDesktops");

    QTest::newRow("Desktop") << PlasmaShellSurface::Role::Desktop << true;
    QTest::newRow("Panel") << PlasmaShellSurface::Role::Panel << true;
    QTest::newRow("OSD") << PlasmaShellSurface::Role::OnScreenDisplay << true;
    QTest::newRow("Normal") << PlasmaShellSurface::Role::Normal << false;
}

void PlasmaSurfaceTest::testRoleOnAllDesktops()
{
    // this test verifies that a ShellClient is set on all desktops when the role changes
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    // now render to map the window
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit();
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *c = workspace()->activeClient();
    QVERIFY(c);
    QCOMPARE(clientAddedSpy.first().first().value<ShellClient*>(), c);

    // currently the role is not yet set, so the window should not be on all desktops
    QCOMPARE(c->isOnAllDesktops(), false);

    // now let's try to change that
    QSignalSpy onAllDesktopsSpy(c, &AbstractClient::desktopChanged);
    QVERIFY(onAllDesktopsSpy.isValid());
    QFETCH(PlasmaShellSurface::Role, role);
    plasmaSurface->setRole(role);
    QFETCH(bool, expectedOnAllDesktops);
    QCOMPARE(onAllDesktopsSpy.wait(), expectedOnAllDesktops);
    QCOMPARE(c->isOnAllDesktops(), expectedOnAllDesktops);

    // let's create a second window where we init a little bit different
    // first creating the PlasmaSurface then the Shell Surface
    QScopedPointer<Surface> surface2(m_compositor->createSurface());
    QVERIFY(!surface2.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface2(m_plasmaShell->createSurface(surface2.data()));
    QVERIFY(!plasmaSurface2.isNull());
    plasmaSurface2->setRole(role);
    QScopedPointer<ShellSurface> shellSurface2(m_shell->createSurface(surface2.data()));
    QVERIFY(!shellSurface2.isNull());
    surface2->attachBuffer(m_shm->createBuffer(img));
    surface2->damage(QRect(0, 0, 100, 50));
    surface2->commit();
    QVERIFY(clientAddedSpy.wait());

    QVERIFY(workspace()->activeClient() != c);
    c = workspace()->activeClient();
    QEXPECT_FAIL("Desktop", "PS before WS not supported", Continue);
    QEXPECT_FAIL("Panel", "PS before WS not supported", Continue);
    QEXPECT_FAIL("OSD", "PS before WS not supported", Continue);
    QCOMPARE(c->isOnAllDesktops(), expectedOnAllDesktops);
}

void PlasmaSurfaceTest::testAcceptsFocus_data()
{
    QTest::addColumn<PlasmaShellSurface::Role>("role");
    QTest::addColumn<bool>("wantsInput");
    QTest::addColumn<bool>("active");

    QTest::newRow("Desktop") << PlasmaShellSurface::Role::Desktop << true << true;
    QTest::newRow("Panel") << PlasmaShellSurface::Role::Panel << true << false;
    QTest::newRow("OSD") << PlasmaShellSurface::Role::OnScreenDisplay << false << false;
    QTest::newRow("Normal") << PlasmaShellSurface::Role::Normal << true << true;
}

void PlasmaSurfaceTest::testAcceptsFocus()
{
    // this test verifies that some surface roles don't get focus
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(m_plasmaShell->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    QFETCH(PlasmaShellSurface::Role, role);
    plasmaSurface->setRole(role);

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    // now render to map the window
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit();
    QVERIFY(clientAddedSpy.wait());

    auto c = clientAddedSpy.first().first().value<ShellClient*>();
    QVERIFY(c);
    QTEST(c->wantsInput(), "wantsInput");
    QTEST(c->isActive(), "active");
}

WAYLANDTEST_MAIN(PlasmaSurfaceTest)
#include "plasma_surface_test.moc"
