/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "../../src/client/slide.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/region_interface.h"
#include "../../src/server/slide_interface.h"

class TestSlide : public QObject
{
    Q_OBJECT
public:
    explicit TestSlide(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::SlideManagerInterface *m_slideManagerInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::SlideManager *m_slideManager;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
    KWayland::Client::Registry m_registry;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-slide-0");

TestSlide::TestSlide(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestSlide::init()
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

    QSignalSpy slideSpy(&m_registry, SIGNAL(slideAnnounced(quint32,quint32)));
    QVERIFY(slideSpy.isValid());

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

    m_slideManagerInterface = m_display->createSlideManager(m_display);
    m_slideManagerInterface->create();
    QVERIFY(m_slideManagerInterface->isValid());

    QVERIFY(slideSpy.wait());
    m_slideManager = m_registry.createSlideManager(slideSpy.first().first().value<quint32>(), slideSpy.first().last().value<quint32>(), this);
}

void TestSlide::cleanup()
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

void TestSlide::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    QScopedPointer<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QSignalSpy slideChanged(serverSurface, SIGNAL(slideChanged()));

    auto slide = m_slideManager->createSlide(surface.data(), surface.data());
    slide->setLocation(KWayland::Client::Slide::Location::Top);
    slide->setOffset(15);
    slide->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(slideChanged.wait());
    QCOMPARE(serverSurface->slideOnShowHide()->location(), KWayland::Server::SlideInterface::Location::Top);
    QCOMPARE(serverSurface->slideOnShowHide()->offset(), 15);
}

QTEST_MAIN(TestSlide)
#include "test_wayland_slide.moc"
