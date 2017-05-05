/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// client
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/plasmashell.h"
#include "../../src/client/registry.h"
#include "../../src/client/shell.h"
#include "../../src/client/surface.h"
// server
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/shell_interface.h"
#include "../../src/server/plasmashell_interface.h"

#include <wayland-client-protocol.h>

#include <errno.h>  // For EPROTO

using namespace KWayland::Client;
using namespace KWayland::Server;

class ErrorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testMultipleShellSurfacesForSurface();
    void testMultiplePlasmaShellSurfacesForSurface();
    void testTransientForSameSurface_data();
    void testTransientForSameSurface();

private:
    Display *m_display = nullptr;
    CompositorInterface *m_ci = nullptr;
    ShellInterface *m_si = nullptr;
    PlasmaShellInterface *m_psi = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    EventQueue *m_queue = nullptr;
    Compositor *m_compositor = nullptr;
    Shell *m_shell = nullptr;
    PlasmaShell *m_plasmaShell = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-error-0");

void ErrorTest::init()
{
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_ci = m_display->createCompositor(m_display);
    m_ci->create();
    m_si = m_display->createShell(m_display);
    m_si->create();
    m_psi = m_display->createPlasmaShell(m_display);
    m_psi->create();

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name,
                                             registry.interface(Registry::Interface::Compositor).version,
                                             this);
    QVERIFY(m_compositor);
    m_shell = registry.createShell(registry.interface(Registry::Interface::Shell).name,
                                   registry.interface(Registry::Interface::Shell).version,
                                   this);
    QVERIFY(m_shell);
    m_plasmaShell = registry.createPlasmaShell(registry.interface(Registry::Interface::PlasmaShell).name,
                                               registry.interface(Registry::Interface::PlasmaShell).version,
                                               this);
    QVERIFY(m_plasmaShell);
}

void ErrorTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_plasmaShell)
    CLEANUP(m_shell)
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
    CLEANUP(m_psi)
    CLEANUP(m_si)
    CLEANUP(m_ci)
    CLEANUP(m_display)
#undef CLEANUP
}

void ErrorTest::testMultipleShellSurfacesForSurface()
{
    // this test verifies that creating two ShellSurfaces for the same Surface triggers a protocol error
    QSignalSpy errorSpy(m_connection, &ConnectionThread::errorOccurred);
    QVERIFY(errorSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<ShellSurface> shellSurface1(m_shell->createSurface(surface.data()));
    QScopedPointer<ShellSurface> shellSurface2(m_shell->createSurface(surface.data()));
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->hasError());
    QCOMPARE(m_connection->errorCode(), EPROTO);
}

void ErrorTest::testMultiplePlasmaShellSurfacesForSurface()
{
    // this test verifies that creating two ShellSurfaces for the same Surface triggers a protocol error
    QSignalSpy errorSpy(m_connection, &ConnectionThread::errorOccurred);
    QVERIFY(errorSpy.isValid());
    // PlasmaShell is too smart and doesn't allow us to create a second PlasmaShellSurface
    // thus we need to cheat by creating a surface manually
    auto surface = wl_compositor_create_surface(*m_compositor);
    QScopedPointer<PlasmaShellSurface> shellSurface1(m_plasmaShell->createSurface(surface));
    QScopedPointer<PlasmaShellSurface> shellSurface2(m_plasmaShell->createSurface(surface));
    QVERIFY(!m_connection->hasError());
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->hasError());
    QCOMPARE(m_connection->errorCode(), EPROTO);
    wl_surface_destroy(surface);
}

void ErrorTest::testTransientForSameSurface_data()
{
    QTest::addColumn<ShellSurface::TransientFlag>("flag");

    QTest::newRow("transient") << ShellSurface::TransientFlag::Default;
    QTest::newRow("transient no focus") << ShellSurface::TransientFlag::NoFocus;
}

void ErrorTest::testTransientForSameSurface()
{
    // this test verifies that creating a transient shell surface for itself triggers a protocol error
    QSignalSpy errorSpy(m_connection, &ConnectionThread::errorOccurred);
    QVERIFY(errorSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<ShellSurface> shellSurface(m_shell->createSurface(surface.data()));
    QFETCH(ShellSurface::TransientFlag, flag);
    shellSurface->setTransient(surface.data(), QPoint(), flag);
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->hasError());
    QCOMPARE(m_connection->errorCode(), EPROTO);
}

QTEST_GUILESS_MAIN(ErrorTest)
#include "test_error.moc"
