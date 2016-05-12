/********************************************************************
Copyright 2015 Marco Martin <mart@kde.org>

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
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"
#include "../../src/client/plasmawindowmanagement.h"
#include "../../src/client/surface.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/region_interface.h"
#include "../../src/server/plasmawindowmanagement_interface.h"
#include "../../src/server/surface_interface.h"

class TestWindowManagement : public QObject
{
    Q_OBJECT
public:
    explicit TestWindowManagement(QObject *parent = nullptr);
private Q_SLOTS:
    void init();

    void testWindowTitle();
    void testMinimizedGeometry();
    void testUseAfterUnmap();
    void testServerDelete();

    void cleanup();

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::PlasmaWindowManagementInterface *m_windowManagementInterface;
    KWayland::Server::PlasmaWindowInterface *m_windowInterface;
    KWayland::Server::SurfaceInterface *m_surfaceInterface = nullptr;

    KWayland::Client::Surface *m_surface = nullptr;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::PlasmaWindowManagement *m_windowManagement;
    KWayland::Client::PlasmaWindow *m_window;
    QThread *m_thread;
    KWayland::Client::Registry *m_registry;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-windowmanagement-0");

TestWindowManagement::TestWindowManagement(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestWindowManagement::init()
{
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new KWayland::Client::Registry(this);
    QSignalSpy compositorSpy(m_registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QVERIFY(compositorSpy.isValid());

    QSignalSpy windowManagementSpy(m_registry, SIGNAL(plasmaWindowManagementAnnounced(quint32,quint32)));
    QVERIFY(windowManagementSpy.isValid());


    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);


    m_windowManagementInterface = m_display->createPlasmaWindowManagement(m_display);
    m_windowManagementInterface->create();
    QVERIFY(m_windowManagementInterface->isValid());

    QVERIFY(windowManagementSpy.wait());
    m_windowManagement = m_registry->createPlasmaWindowManagement(windowManagementSpy.first().first().value<quint32>(), windowManagementSpy.first().last().value<quint32>(), this);

    QSignalSpy windowSpy(m_windowManagement, SIGNAL(windowCreated(KWayland::Client::PlasmaWindow *)));
    QVERIFY(windowSpy.isValid());
    m_windowInterface = m_windowManagementInterface->createWindow(this);

    QVERIFY(windowSpy.wait());
    m_window = windowSpy.first().first().value<KWayland::Client::PlasmaWindow *>();

    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    m_surface = m_compositor->createSurface(this);
    QVERIFY(m_surface);

    QVERIFY(serverSurfaceCreated.wait());
    m_surfaceInterface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(m_surfaceInterface);
}

void TestWindowManagement::testWindowTitle()
{
    m_windowInterface->setTitle(QStringLiteral("Test Title"));

    QSignalSpy titleSpy(m_window, SIGNAL(titleChanged()));
    QVERIFY(titleSpy.isValid());

    QVERIFY(titleSpy.wait());

    QCOMPARE(m_window->title(), QString::fromUtf8("Test Title"));
}

void TestWindowManagement::testMinimizedGeometry()
{
    m_window->setMinimizedGeometry(m_surface, QRect(5, 10, 100, 200));

    QSignalSpy geometrySpy(m_windowInterface, SIGNAL(minimizedGeometriesChanged()));
    QVERIFY(geometrySpy.isValid());

    QVERIFY(geometrySpy.wait());
    QCOMPARE(m_windowInterface->minimizedGeometries().values().first(), QRect(5, 10, 100, 200));

    m_window->unsetMinimizedGeometry(m_surface);
    QVERIFY(geometrySpy.wait());
    QVERIFY(m_windowInterface->minimizedGeometries().isEmpty());
}

void TestWindowManagement::cleanup()
{

    if (m_surface) {
        delete m_surface;
        m_surface = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_windowManagement) {
        delete m_windowManagement;
        m_windowManagement = nullptr;
    }
    if (m_registry) {
        delete m_registry;
        m_registry = nullptr;
    }
    if (m_thread) {
        if (m_connection) {
            m_connection->deleteLater();
        }
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    m_connection = nullptr;

    delete m_windowManagementInterface;
    m_windowManagementInterface = nullptr;

    delete m_windowInterface;
    m_windowInterface = nullptr;

    delete m_surfaceInterface;
    m_surfaceInterface = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestWindowManagement::testUseAfterUnmap()
{
    // this test verifies that when the client uses a window after it's unmapped, things don't break
    QSignalSpy unmappedSpy(m_window, &KWayland::Client::PlasmaWindow::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy destroyedSpy(m_window, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    m_windowInterface->unmap();
    m_window->requestClose();
    QVERIFY(unmappedSpy.wait());
    QVERIFY(destroyedSpy.wait());
    m_window = nullptr;
    QSignalSpy serverDestroyedSpy(m_windowInterface, &QObject::destroyed);
    QVERIFY(serverDestroyedSpy.isValid());
    QVERIFY(serverDestroyedSpy.wait());
    m_windowInterface = nullptr;
}

void TestWindowManagement::testServerDelete()
{
    QSignalSpy unmappedSpy(m_window, &KWayland::Client::PlasmaWindow::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy destroyedSpy(m_window, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete m_windowInterface;
    m_windowInterface = nullptr;
    QVERIFY(unmappedSpy.wait());
    QVERIFY(destroyedSpy.wait());
    m_window = nullptr;
}

QTEST_GUILESS_MAIN(TestWindowManagement)
#include "test_wayland_windowmanagement.moc"
