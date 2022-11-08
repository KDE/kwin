/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>

// server
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/plasmashell_interface.h"

// client
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/plasmashell.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

#include <wayland-client-protocol.h>

#include <cerrno> // For EPROTO

using namespace KWaylandServer;

class ErrorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testMultiplePlasmaShellSurfacesForSurface();

private:
    KWaylandServer::Display *m_display = nullptr;
    CompositorInterface *m_ci = nullptr;
    PlasmaShellInterface *m_psi = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::PlasmaShell *m_plasmaShell = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-error-0");

void ErrorTest::init()
{
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_ci = new CompositorInterface(m_display, m_display);
    m_psi = new PlasmaShellInterface(m_display, m_display);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    m_queue->setup(m_connection);

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor =
        registry.createCompositor(registry.interface(KWayland::Client::Registry::Interface::Compositor).name, registry.interface(KWayland::Client::Registry::Interface::Compositor).version, this);
    QVERIFY(m_compositor);
    m_plasmaShell = registry.createPlasmaShell(registry.interface(KWayland::Client::Registry::Interface::PlasmaShell).name,
                                               registry.interface(KWayland::Client::Registry::Interface::PlasmaShell).version,
                                               this);
    QVERIFY(m_plasmaShell);
}

void ErrorTest::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    CLEANUP(m_plasmaShell)
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
    CLEANUP(m_display)
#undef CLEANUP
    // these are the children of the display
    m_psi = nullptr;
    m_ci = nullptr;
}

void ErrorTest::testMultiplePlasmaShellSurfacesForSurface()
{
    // this test verifies that creating two ShellSurfaces for the same Surface triggers a protocol error
    QSignalSpy errorSpy(m_connection, &KWayland::Client::ConnectionThread::errorOccurred);
    // PlasmaShell is too smart and doesn't allow us to create a second PlasmaShellSurface
    // thus we need to cheat by creating a surface manually
    auto surface = wl_compositor_create_surface(*m_compositor);
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> shellSurface1(m_plasmaShell->createSurface(surface));
    std::unique_ptr<KWayland::Client::PlasmaShellSurface> shellSurface2(m_plasmaShell->createSurface(surface));
    QVERIFY(!m_connection->hasError());
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->hasError());
    QCOMPARE(m_connection->errorCode(), EPROTO);
    wl_surface_destroy(surface);
}

QTEST_GUILESS_MAIN(ErrorTest)
#include "test_error.moc"
