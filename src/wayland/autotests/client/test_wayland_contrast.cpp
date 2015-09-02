/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2015  Marco Martin <mart@kde.org>

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
#include "../../src/client/contrast.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/region_interface.h"
#include "../../src/server/contrast_interface.h"

#include <wayland-util.h>

class TestContrast : public QObject
{
    Q_OBJECT
public:
    explicit TestContrast(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::ContrastManagerInterface *m_contrastManagerInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::ContrastManager *m_contrastManager;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
    KWayland::Client::Registry m_registry;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-contrast-0");

TestContrast::TestContrast(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestContrast::init()
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

    QSignalSpy compositorSpy(&m_registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QVERIFY(compositorSpy.isValid());

    QSignalSpy contrastSpy(&m_registry, SIGNAL(contrastAnnounced(quint32,quint32)));
    QVERIFY(contrastSpy.isValid());

    QVERIFY(!m_registry.eventQueue());
    m_registry.setEventQueue(m_queue);
    QCOMPARE(m_registry.eventQueue(), m_queue);
    m_registry.create(m_connection->display());
    QVERIFY(m_registry.isValid());
    m_registry.setup();

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_contrastManagerInterface = m_display->createContrastManager(m_display);
    m_contrastManagerInterface->create();
    QVERIFY(m_contrastManagerInterface->isValid());

    QVERIFY(contrastSpy.wait());
    m_contrastManager = m_registry.createContrastManager(contrastSpy.first().first().value<quint32>(), contrastSpy.first().last().value<quint32>(), this);
}

void TestContrast::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestContrast::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    QScopedPointer<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QSignalSpy contrastChanged(serverSurface, SIGNAL(contrastChanged()));

    auto contrast = m_contrastManager->createContrast(surface.data(), surface.data());
    contrast->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));

    contrast->setContrast(0.2);
    contrast->setIntensity(2.0);
    contrast->setSaturation(1.7);

    contrast->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(contrastChanged.wait());
    QCOMPARE(serverSurface->contrast()->region(), QRegion(0, 0, 10, 20));
    QCOMPARE(wl_fixed_from_double(serverSurface->contrast()->contrast()), wl_fixed_from_double(0.2));
    QCOMPARE(wl_fixed_from_double(serverSurface->contrast()->intensity()), wl_fixed_from_double(2.0));
    QCOMPARE(wl_fixed_from_double(serverSurface->contrast()->saturation()), wl_fixed_from_double(1.7));
}

QTEST_MAIN(TestContrast)
#include "test_wayland_contrast.moc"
