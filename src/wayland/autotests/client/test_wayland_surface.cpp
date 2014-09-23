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
#include <QImage>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/surface.h"
#include "../../src/client/registry.h"
#include "../../src/client/shm_pool.h"
#include "../../src/server/buffer_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/surface_interface.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandSurface : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandSurface(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testStaticAccessor();
    void testDamage();
    void testFrameCallback();
    void testAttachBuffer();
    void testDestroy();

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-surface-0");

TestWaylandSurface::TestWaylandSurface(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandSurface::init()
{
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_compositorInterface = m_display->createCompositor(m_display);
    QVERIFY(m_compositorInterface);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, m_connection,
        [this]() {
            if (m_connection->display()) {
                wl_display_flush(m_connection->display());
            }
        }
    );

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    KWayland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(compositorSpy.wait());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
}

void TestWaylandSurface::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    delete m_compositorInterface;
    m_compositorInterface = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestWaylandSurface::testStaticAccessor()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    QVERIFY(KWayland::Client::Surface::all().isEmpty());
    KWayland::Client::Surface *s1 = m_compositor->createSurface();
    QVERIFY(s1->isValid());
    QCOMPARE(KWayland::Client::Surface::all().count(), 1);
    QCOMPARE(KWayland::Client::Surface::all().first(), s1);
    QCOMPARE(KWayland::Client::Surface::get(*s1), s1);
    QVERIFY(serverSurfaceCreated.wait());

    QVERIFY(!s1->size().isValid());
    QSignalSpy sizeChangedSpy(s1, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeChangedSpy.isValid());
    const QSize testSize(200, 300);
    s1->setSize(testSize);
    QCOMPARE(s1->size(), testSize);
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(sizeChangedSpy.first().first().toSize(), testSize);

    // add another surface
    KWayland::Client::Surface *s2 = m_compositor->createSurface();
    QVERIFY(s2->isValid());
    QCOMPARE(KWayland::Client::Surface::all().count(), 2);
    QCOMPARE(KWayland::Client::Surface::all().first(), s1);
    QCOMPARE(KWayland::Client::Surface::all().last(), s2);
    QCOMPARE(KWayland::Client::Surface::get(*s1), s1);
    QCOMPARE(KWayland::Client::Surface::get(*s2), s2);
    serverSurfaceCreated.clear();
    QVERIFY(serverSurfaceCreated.wait());

    // delete s2 again
    delete s2;
    QCOMPARE(KWayland::Client::Surface::all().count(), 1);
    QCOMPARE(KWayland::Client::Surface::all().first(), s1);
    QCOMPARE(KWayland::Client::Surface::get(*s1), s1);

    // and finally delete the last one
    delete s1;
    QVERIFY(KWayland::Client::Surface::all().isEmpty());
    QVERIFY(!KWayland::Client::Surface::get(nullptr));
}

void TestWaylandSurface::testDamage()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    KWayland::Client::Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    KWayland::Server::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->damage(), QRegion());

    QSignalSpy damageSpy(serverSurface, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy.isValid());

    // TODO: actually we would need to attach a buffer first
    s->damage(QRect(0, 0, 10, 10));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(damageSpy.wait());
    QCOMPARE(serverSurface->damage(), QRegion(0, 0, 10, 10));
    QCOMPARE(damageSpy.first().first().value<QRegion>(), QRegion(0, 0, 10, 10));

    // damage multiple times
    QRegion testRegion(5, 8, 3, 6);
    testRegion = testRegion.united(QRect(10, 20, 30, 15));
    s->damage(testRegion);
    damageSpy.clear();
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(damageSpy.wait());
    QCOMPARE(serverSurface->damage(), testRegion);
    QCOMPARE(damageSpy.first().first().value<QRegion>(), testRegion);
}

void TestWaylandSurface::testFrameCallback()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    KWayland::Client::Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    KWayland::Server::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy damageSpy(serverSurface, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy.isValid());

    QSignalSpy frameRenderedSpy(s, SIGNAL(frameRendered()));
    QVERIFY(frameRenderedSpy.isValid());
    s->damage(QRect(0, 0, 10, 10));
    s->commit();
    QVERIFY(damageSpy.wait());
    serverSurface->frameRendered(10);
    QVERIFY(frameRenderedSpy.isEmpty());
    QVERIFY(frameRenderedSpy.wait());
    QVERIFY(!frameRenderedSpy.isEmpty());
}

void TestWaylandSurface::testAttachBuffer()
{
    // here we need a shm pool
    m_display->createShm();

    KWayland::Client::Registry registry;
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(shmSpy.wait());

    KWayland::Client::ShmPool pool;
    pool.setup(registry.bindShm(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>()));
    QVERIFY(pool.isValid());

    // create the surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    KWayland::Client::Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    KWayland::Server::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    // create two images
    QImage black(24, 24, QImage::Format_RGB32);
    black.fill(Qt::black);
    QImage red(24, 24, QImage::Format_ARGB32);
    red.fill(QColor(255, 0, 0, 128));
    QImage blue(24, 24, QImage::Format_ARGB32_Premultiplied);
    blue.fill(QColor(0, 0, 255, 128));

    wl_buffer *blackBuffer = *(pool.createBuffer(black).data());
    auto redBuffer = pool.createBuffer(red);
    auto blueBuffer = pool.createBuffer(blue).toStrongRef();

    QCOMPARE(blueBuffer->format(), KWayland::Client::Buffer::Format::ARGB32);
    QCOMPARE(blueBuffer->size(), blue.size());
    QVERIFY(!blueBuffer->isReleased());
    QVERIFY(!blueBuffer->isUsed());
    QCOMPARE(blueBuffer->stride(), blue.bytesPerLine());

    s->attachBuffer(redBuffer.data());
    s->attachBuffer(blackBuffer);
    s->damage(QRect(0, 0, 24, 24));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy damageSpy(serverSurface, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy.isValid());
    QVERIFY(damageSpy.wait());

    // now the ServerSurface should have the black image attached as a buffer
    KWayland::Server::BufferInterface *buffer = serverSurface->buffer();
    buffer->ref();
    QVERIFY(buffer->shmBuffer());
    QCOMPARE(buffer->data(), black);
    QCOMPARE(buffer->data().format(), QImage::Format_RGB32);

    // render another frame
    s->attachBuffer(redBuffer);
    s->damage(QRect(0, 0, 24, 24));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    damageSpy.clear();
    QVERIFY(damageSpy.wait());
    KWayland::Server::BufferInterface *buffer2 = serverSurface->buffer();
    buffer2->ref();
    QVERIFY(buffer2->shmBuffer());
    QCOMPARE(buffer2->data(), red);
    QCOMPARE(buffer2->data().format(), QImage::Format_ARGB32);
    buffer2->unref();
    QVERIFY(buffer2->isReferenced());
    QVERIFY(!redBuffer.data()->isReleased());

    // render another frame
    blueBuffer->setUsed(true);
    QVERIFY(blueBuffer->isUsed());
    s->attachBuffer(blueBuffer.data());
    s->damage(QRect(0, 0, 24, 24));
    QSignalSpy frameRenderedSpy(s, SIGNAL(frameRendered()));
    QVERIFY(frameRenderedSpy.isValid());
    s->commit();
    damageSpy.clear();
    QVERIFY(damageSpy.wait());
    QVERIFY(!buffer2->isReferenced());
    delete buffer2;
    // TODO: we should have a signal on when the Buffer gets released
    QTest::qWait(100);
    QVERIFY(redBuffer.data()->isReleased());

    KWayland::Server::BufferInterface *buffer3 = serverSurface->buffer();
    buffer3->ref();
    QVERIFY(buffer3->shmBuffer());
    QCOMPARE(buffer3->data().format(), QImage::Format_ARGB32);
    QCOMPARE(buffer3->data().width(), 24);
    QCOMPARE(buffer3->data().height(), 24);
    for (int i = 0; i < 24; ++i) {
        for (int j = 0; j < 24; ++j) {
            // it's premultiplied in the format
            QCOMPARE(buffer3->data().pixel(i, j), qRgba(0, 0, 128, 128));
        }
    }
    buffer3->unref();
    QVERIFY(buffer3->isReferenced());

    serverSurface->frameRendered(1);
    QVERIFY(frameRenderedSpy.wait());

    // TODO: add signal test on release
    buffer->unref();
}

void TestWaylandSurface::testDestroy()
{
    using namespace KWayland::Client;
    Surface *s = m_compositor->createSurface();

    connect(m_connection, &ConnectionThread::connectionDied, s, &Surface::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_compositor, &Compositor::destroy);
    QVERIFY(s->isValid());

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    m_compositorInterface = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    // now the Surface should be destroyed;
    QVERIFY(!s->isValid());

    // calling destroy again should not fail
    s->destroy();
}

QTEST_GUILESS_MAIN(TestWaylandSurface)
#include "test_wayland_surface.moc"
