/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
#include <QImage>
#include <QPainter>
// KWin
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/idleinhibit.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shm_pool.h"
#include "../../src/server/buffer_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/idleinhibit_v1_interface.h"
#include "../../src/server/surface_interface.h"
// Wayland
#include <wayland-client-protocol.h>

using KWayland::Client::Registry;

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
    void testMultipleSurfaces();
    void testOpaque();
    void testInput();
    void testScale();
    void testDestroy();
    void testUnmapOfNotMappedSurface();
    void testDamageTracking();
    void testSurfaceAt();
    void testDestroyAttachedBuffer();
    void testDestroyWithPendingCallback();
    void testOutput();
    void testDisconnect();
    void testInhibit();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::IdleInhibitManagerV1Interface *m_idleInhibitInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::ShmPool *m_shm;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::IdleInhibitManager *m_idleInhibitManager;
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
    using namespace KWaylandServer;
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(m_compositorInterface);

    m_idleInhibitInterface = new IdleInhibitManagerV1Interface(m_display, m_display);
    QVERIFY(m_idleInhibitInterface);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    /*connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, m_connection,
        [this]() {
            if (m_connection->display()) {
                wl_display_flush(m_connection->display());
            }
        }
    );*/

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    KWayland::Client::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    QSignalSpy allAnnounced(&registry, SIGNAL(interfacesAnnounced()));
    QVERIFY(allAnnounced.isValid());
    QVERIFY(shmSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QVERIFY(!compositorSpy.isEmpty());
    QVERIFY(!shmSpy.isEmpty());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());

    m_idleInhibitManager = registry.createIdleInhibitManager(registry.interface(Registry::Interface::IdleInhibitManagerUnstableV1).name, registry.interface(Registry::Interface::IdleInhibitManagerUnstableV1).version, this);
    QVERIFY(m_idleInhibitManager->isValid());
}

void TestWaylandSurface::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_idleInhibitManager) {
        delete m_idleInhibitManager;
        m_idleInhibitManager = nullptr;
    }
    if (m_shm) {
        delete m_shm;
        m_shm = nullptr;
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

    // these are the children of the display
    m_compositorInterface = nullptr;
    m_idleInhibitInterface = nullptr;
}

void TestWaylandSurface::testStaticAccessor()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    QVERIFY(!KWaylandServer::SurfaceInterface::get(nullptr));
    QVERIFY(!KWaylandServer::SurfaceInterface::get(1, nullptr));
    QVERIFY(KWayland::Client::Surface::all().isEmpty());
    KWayland::Client::Surface *s1 = m_compositor->createSurface();
    QVERIFY(s1->isValid());
    QCOMPARE(KWayland::Client::Surface::all().count(), 1);
    QCOMPARE(KWayland::Client::Surface::all().first(), s1);
    QCOMPARE(KWayland::Client::Surface::get(*s1), s1);
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface1 = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface1);
    QCOMPARE(KWaylandServer::SurfaceInterface::get(serverSurface1->resource()), serverSurface1);
    QCOMPARE(KWaylandServer::SurfaceInterface::get(serverSurface1->id(), serverSurface1->client()), serverSurface1);

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
    auto serverSurface2 = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface2);
    QCOMPARE(KWaylandServer::SurfaceInterface::get(serverSurface1->resource()), serverSurface1);
    QCOMPARE(KWaylandServer::SurfaceInterface::get(serverSurface1->id(), serverSurface1->client()), serverSurface1);
    QCOMPARE(KWaylandServer::SurfaceInterface::get(serverSurface2->resource()), serverSurface2);
    QCOMPARE(KWaylandServer::SurfaceInterface::get(serverSurface2->id(), serverSurface2->client()), serverSurface2);

    const quint32 surfaceId1 = serverSurface1->id();
    const quint32 surfaceId2 = serverSurface2->id();

    // delete s2 again
    delete s2;
    QCOMPARE(KWayland::Client::Surface::all().count(), 1);
    QCOMPARE(KWayland::Client::Surface::all().first(), s1);
    QCOMPARE(KWayland::Client::Surface::get(*s1), s1);

    // and finally delete the last one
    delete s1;
    QVERIFY(KWayland::Client::Surface::all().isEmpty());
    QVERIFY(!KWayland::Client::Surface::get(nullptr));
    QSignalSpy destroyedSpy(serverSurface1, &KWaylandServer::SurfaceInterface::destroyed);
    QVERIFY(destroyedSpy.isValid());
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!KWaylandServer::SurfaceInterface::get(nullptr));
    QVERIFY(!KWaylandServer::SurfaceInterface::get(surfaceId1, nullptr));
    QVERIFY(!KWaylandServer::SurfaceInterface::get(surfaceId2, nullptr));
}

void TestWaylandSurface::testDamage()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    KWayland::Client::Surface *s = m_compositor->createSurface();
    s->setScale(2);
    QVERIFY(serverSurfaceCreated.wait());
    KWaylandServer::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->damage(), QRegion());
    QVERIFY(!serverSurface->isMapped());

    QSignalSpy committedSpy(serverSurface, SIGNAL(committed()));
    QSignalSpy damageSpy(serverSurface, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy.isValid());

    // send damage without a buffer
    s->damage(QRect(0, 0, 100, 100));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QVERIFY(damageSpy.isEmpty());
    QVERIFY(!serverSurface->isMapped());
    QCOMPARE(committedSpy.count(), 1);

    QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto b = m_shm->createBuffer(img);
    s->attachBuffer(b, QPoint(55, 55));
    s->damage(QRect(0, 0, 10, 10));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(damageSpy.wait());
    QCOMPARE(serverSurface->offset(), QPoint(55, 55)); // offset is surface local so scale doesn't change this
    QCOMPARE(serverSurface->damage(), QRegion(0, 0, 5, 5)); // scale is 2
    QCOMPARE(damageSpy.first().first().value<QRegion>(), QRegion(0, 0, 5, 5));
    QVERIFY(serverSurface->isMapped());
    QCOMPARE(committedSpy.count(), 2);

    // damage multiple times
    QRegion testRegion(5, 8, 3, 6);
    testRegion = testRegion.united(QRect(10, 11, 6, 1));
    img = QImage(QSize(40, 35), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damage(testRegion);
    damageSpy.clear();
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(damageSpy.wait());
    QCOMPARE(serverSurface->damage(), testRegion);
    QCOMPARE(damageSpy.first().first().value<QRegion>(), testRegion);
    QVERIFY(serverSurface->isMapped());
    QCOMPARE(committedSpy.count(), 3);

    // damage buffer
    const QRegion testRegion2(30, 40, 22, 4);
    const QRegion cmpRegion2(15, 20, 11, 2); // divided by scale factor
    img = QImage(QSize(80, 70), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damageBuffer(testRegion2);
    damageSpy.clear();
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(damageSpy.wait());
    QCOMPARE(serverSurface->damage(), cmpRegion2);
    QCOMPARE(damageSpy.first().first().value<QRegion>(), cmpRegion2);
    QVERIFY(serverSurface->isMapped());

    // combined regular damage and damaged buffer
    const QRegion testRegion3 = testRegion.united(cmpRegion2);
    img = QImage(QSize(80, 70), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damage(testRegion);
    s->damageBuffer(testRegion2);
    damageSpy.clear();
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(damageSpy.wait());
    QVERIFY(serverSurface->damage() != testRegion);
    QVERIFY(serverSurface->damage() != testRegion2);
    QVERIFY(serverSurface->damage() != cmpRegion2);
    QCOMPARE(serverSurface->damage(), testRegion3);
    QCOMPARE(damageSpy.first().first().value<QRegion>(), testRegion3);
    QVERIFY(serverSurface->isMapped());
}

void TestWaylandSurface::testFrameCallback()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    KWayland::Client::Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    KWaylandServer::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy damageSpy(serverSurface, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy.isValid());

    QSignalSpy frameRenderedSpy(s, SIGNAL(frameRendered()));
    QVERIFY(frameRenderedSpy.isValid());
    QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto b = m_shm->createBuffer(img);
    s->attachBuffer(b);
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
    // create the surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    KWayland::Client::Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    KWaylandServer::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);

    // create three images
    QImage black(24, 24, QImage::Format_RGB32);
    black.fill(Qt::black);
    QImage red(24, 24, QImage::Format_ARGB32); //Note - deliberately not premultiplied
    red.fill(QColor(255, 0, 0, 128));
    QImage blue(24, 24, QImage::Format_ARGB32_Premultiplied);
    blue.fill(QColor(0, 0, 255, 128));

    QSharedPointer<KWayland::Client::Buffer> blackBufferPtr = m_shm->createBuffer(black).toStrongRef();
    QVERIFY(blackBufferPtr);
    wl_buffer *blackBuffer = *(blackBufferPtr.data());
    QSharedPointer<KWayland::Client::Buffer> redBuffer = m_shm->createBuffer(red).toStrongRef();
    QVERIFY(redBuffer);
    QSharedPointer<KWayland::Client::Buffer> blueBuffer = m_shm->createBuffer(blue).toStrongRef();
    QVERIFY(blueBuffer);

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
    QSignalSpy mappedSpy(serverSurface, SIGNAL(mapped()));
    QVERIFY(mappedSpy.isValid());
    QSignalSpy unmappedSpy(serverSurface, SIGNAL(unmapped()));
    QVERIFY(unmappedSpy.isValid());
    QVERIFY(damageSpy.wait());
    QCOMPARE(mappedSpy.count(), 1);
    QVERIFY(unmappedSpy.isEmpty());

    // now the ServerSurface should have the black image attached as a buffer
    KWaylandServer::BufferInterface *buffer = serverSurface->buffer();
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
    QCOMPARE(mappedSpy.count(), 1);
    QVERIFY(unmappedSpy.isEmpty());
    KWaylandServer::BufferInterface *buffer2 = serverSurface->buffer();
    buffer2->ref();
    QVERIFY(buffer2->shmBuffer());
    QCOMPARE(buffer2->data().format(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(buffer2->data().width(), 24);
    QCOMPARE(buffer2->data().height(), 24);
    for (int i = 0; i < 24; ++i) {
        for (int j = 0; j < 24; ++j) {
            // it's premultiplied in the format
            QCOMPARE(buffer2->data().pixel(i, j), qRgba(128, 0, 0, 128));
        }
    }
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
    QCOMPARE(mappedSpy.count(), 1);
    QVERIFY(unmappedSpy.isEmpty());
    QVERIFY(!buffer2->isReferenced());
    delete buffer2;
    // TODO: we should have a signal on when the Buffer gets released
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    if (!redBuffer.data()->isReleased()) {
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    }
    QVERIFY(redBuffer.data()->isReleased());

    KWaylandServer::BufferInterface *buffer3 = serverSurface->buffer();
    buffer3->ref();
    QVERIFY(buffer3->shmBuffer());
    QCOMPARE(buffer3->data().format(), QImage::Format_ARGB32_Premultiplied);
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

    // commit a different value shouldn't change our buffer
    QCOMPARE(serverSurface->buffer(), buffer3);
    damageSpy.clear();
    s->setInputRegion(m_compositor->createRegion(QRegion(0, 0, 24, 24)).get());
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(serverSurface->buffer(), buffer3);
    QVERIFY(damageSpy.isEmpty());
    QCOMPARE(mappedSpy.count(), 1);
    QVERIFY(unmappedSpy.isEmpty());
    QVERIFY(serverSurface->isMapped());

    // clear the surface
    s->attachBuffer(blackBuffer);
    s->damage(QRect(0, 0, 1, 1));
    // TODO: better method
    s->attachBuffer((wl_buffer*)nullptr);
    s->damage(QRect(0, 0, 10, 10));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(unmappedSpy.wait());
    QCOMPARE(mappedSpy.count(), 1);
    QCOMPARE(unmappedSpy.count(), 1);
    QVERIFY(damageSpy.isEmpty());
    QVERIFY(!serverSurface->isMapped());

    // TODO: add signal test on release
    buffer->unref();
}

void TestWaylandSurface::testMultipleSurfaces()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(shmSpy.wait());

    ShmPool pool1;
    ShmPool pool2;
    pool1.setup(registry.bindShm(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>()));
    pool2.setup(registry.bindShm(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>()));
    QVERIFY(pool1.isValid());
    QVERIFY(pool2.isValid());

    // create the surfaces
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> s1(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface1 = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface1);
    //second surface
    QScopedPointer<Surface> s2(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface2 = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface2);
    QVERIFY(serverSurface1->resource() != serverSurface2->resource());

    // create two images
    QImage black(24, 24, QImage::Format_RGB32);
    black.fill(Qt::black);
    QImage red(24, 24, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 128));

    auto blackBuffer = pool1.createBuffer(black);
    auto redBuffer = pool2.createBuffer(red);

    s1->attachBuffer(blackBuffer);
    s1->damage(QRect(0, 0, 24, 24));
    s1->commit(Surface::CommitFlag::None);
    QSignalSpy damageSpy1(serverSurface1, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy1.isValid());
    QVERIFY(damageSpy1.wait());

    // now the ServerSurface should have the black image attached as a buffer
    BufferInterface *buffer1 = serverSurface1->buffer();
    QVERIFY(buffer1);
    QImage buffer1Data = buffer1->data();
    QCOMPARE(buffer1Data, black);
    // accessing the same buffer is OK
    QImage buffer1Data2 = buffer1->data();
    QCOMPARE(buffer1Data2, buffer1Data);
    buffer1Data = QImage();
    QVERIFY(buffer1Data.isNull());
    buffer1Data2 = QImage();
    QVERIFY(buffer1Data2.isNull());

    // attach a buffer for the other surface
    s2->attachBuffer(redBuffer);
    s2->damage(QRect(0, 0, 24, 24));
    s2->commit(Surface::CommitFlag::None);
    QSignalSpy damageSpy2(serverSurface2, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy2.isValid());
    QVERIFY(damageSpy2.wait());

    BufferInterface *buffer2 = serverSurface2->buffer();
    QVERIFY(buffer2);
    QImage buffer2Data = buffer2->data();
    QCOMPARE(buffer2Data, red);

    // while buffer2 is accessed we cannot access buffer1
    buffer1Data = buffer1->data();
    QVERIFY(buffer1Data.isNull());

    // a deep copy can be kept around
    QImage deepCopy = buffer2Data.copy();
    QCOMPARE(deepCopy, red);
    buffer2Data = QImage();
    QVERIFY(buffer2Data.isNull());
    QCOMPARE(deepCopy, red);

    // now that buffer2Data is destroyed we can access buffer1 again
    buffer1Data = buffer1->data();
    QVERIFY(!buffer1Data.isNull());
    QCOMPARE(buffer1Data, black);
}

void TestWaylandSurface::testOpaque()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);
    QSignalSpy opaqueRegionChangedSpy(serverSurface, SIGNAL(opaqueChanged(QRegion)));
    QVERIFY(opaqueRegionChangedSpy.isValid());

    // by default there should be an empty opaque region
    QCOMPARE(serverSurface->opaque(), QRegion());

    // let's install an opaque region
    s->setOpaqueRegion(m_compositor->createRegion(QRegion(0, 10, 20, 30)).get());
    // the region should only be applied after the surface got committed
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSurface->opaque(), QRegion());
    QCOMPARE(opaqueRegionChangedSpy.count(), 0);

    // so let's commit to get the new region
    s->commit(Surface::CommitFlag::None);
    QVERIFY(opaqueRegionChangedSpy.wait());
    QCOMPARE(opaqueRegionChangedSpy.count(), 1);
    QCOMPARE(opaqueRegionChangedSpy.last().first().value<QRegion>(), QRegion(0, 10, 20, 30));
    QCOMPARE(serverSurface->opaque(), QRegion(0, 10, 20, 30));

    // committing without setting a new region shouldn't change
    s->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(opaqueRegionChangedSpy.count(), 1);
    QCOMPARE(serverSurface->opaque(), QRegion(0, 10, 20, 30));

    // let's change the opaque region
    s->setOpaqueRegion(m_compositor->createRegion(QRegion(10, 20, 30, 40)).get());
    s->commit(Surface::CommitFlag::None);
    QVERIFY(opaqueRegionChangedSpy.wait());
    QCOMPARE(opaqueRegionChangedSpy.count(), 2);
    QCOMPARE(opaqueRegionChangedSpy.last().first().value<QRegion>(), QRegion(10, 20, 30, 40));
    QCOMPARE(serverSurface->opaque(), QRegion(10, 20, 30, 40));

    // and let's go back to an empty region
    s->setOpaqueRegion();
    s->commit(Surface::CommitFlag::None);
    QVERIFY(opaqueRegionChangedSpy.wait());
    QCOMPARE(opaqueRegionChangedSpy.count(), 3);
    QCOMPARE(opaqueRegionChangedSpy.last().first().value<QRegion>(), QRegion());
    QCOMPARE(serverSurface->opaque(), QRegion());
}

void TestWaylandSurface::testInput()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);
    QSignalSpy inputRegionChangedSpy(serverSurface, SIGNAL(inputChanged(QRegion)));
    QVERIFY(inputRegionChangedSpy.isValid());
    QSignalSpy committedSpy(serverSurface, &SurfaceInterface::committed);
    QVERIFY(committedSpy.isValid());

    // the input region should be empty if the surface has no buffer
    QVERIFY(!serverSurface->isMapped());
    QCOMPARE(serverSurface->input(), QRegion());

    // the default input region is infinite
    QImage black(100, 50, QImage::Format_RGB32);
    black.fill(Qt::black);
    QSharedPointer<KWayland::Client::Buffer> buffer1 = m_shm->createBuffer(black).toStrongRef();
    QVERIFY(buffer1);
    s->attachBuffer(buffer1);
    s->commit(Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QVERIFY(serverSurface->isMapped());
    QCOMPARE(inputRegionChangedSpy.count(), 1);
    QCOMPARE(serverSurface->input(), QRegion(0, 0, 100, 50));

    // let's install an input region
    s->setInputRegion(m_compositor->createRegion(QRegion(0, 10, 20, 30)).get());
    // the region should only be applied after the surface got committed
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(inputRegionChangedSpy.count(), 1);
    QCOMPARE(serverSurface->input(), QRegion(0, 0, 100, 50));

    // so let's commit to get the new region
    s->commit(Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(inputRegionChangedSpy.count(), 2);
    QCOMPARE(serverSurface->input(), QRegion(0, 10, 20, 30));

    // committing without setting a new region shouldn't change
    s->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(inputRegionChangedSpy.count(), 2);
    QCOMPARE(serverSurface->input(), QRegion(0, 10, 20, 30));

    // let's change the input region, note that the new input region is cropped
    s->setInputRegion(m_compositor->createRegion(QRegion(10, 20, 30, 40)).get());
    s->commit(Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(inputRegionChangedSpy.count(), 3);
    QCOMPARE(serverSurface->input(), QRegion(10, 20, 30, 30));

    // and let's go back to an empty region
    s->setInputRegion();
    s->commit(Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(inputRegionChangedSpy.count(), 4);
    QCOMPARE(serverSurface->input(), QRegion(0, 0, 100, 50));
}

void TestWaylandSurface::testScale()
{
    // this test verifies that updating the scale factor is correctly passed to the Wayland server
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QCOMPARE(s->scale(), 1);
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->bufferScale(), 1);

    // let's change the scale factor
    QSignalSpy bufferScaleChangedSpy(serverSurface, &SurfaceInterface::bufferScaleChanged);

    //changing the scale implicitly changes the size
    QSignalSpy sizeChangedSpy(serverSurface, &SurfaceInterface::sizeChanged);

    QVERIFY(bufferScaleChangedSpy.isValid());
    s->setScale(2);
    QCOMPARE(s->scale(), 2);
    // needs a commit
    QVERIFY(!bufferScaleChangedSpy.wait(100));
    s->commit(Surface::CommitFlag::None);
    QVERIFY(bufferScaleChangedSpy.wait());
    QCOMPARE(bufferScaleChangedSpy.count(), 1);
    QCOMPARE(bufferScaleChangedSpy.first().first().toInt(), 2);
    QCOMPARE(serverSurface->bufferScale(), 2);

    //even though we've changed the scale, if we don't have a buffer we
    //don't have a size. If we don't have a size it can't have changed
    QCOMPARE(sizeChangedSpy.count(), 0);
    QVERIFY(!serverSurface->size().isValid());

    // let's try changing to same factor, should not emit changed on server
    s->setScale(2);
    s->commit(Surface::CommitFlag::None);
    QVERIFY(!bufferScaleChangedSpy.wait(100));

    // but changing to a different value should still work
    s->setScale(4);
    s->commit(Surface::CommitFlag::None);
    QVERIFY(bufferScaleChangedSpy.wait());
    QCOMPARE(bufferScaleChangedSpy.count(), 2);
    QCOMPARE(bufferScaleChangedSpy.first().first().toInt(), 2);
    QCOMPARE(bufferScaleChangedSpy.last().first().toInt(), 4);
    QCOMPARE(serverSurface->bufferScale(), 4);
    bufferScaleChangedSpy.clear();

    //attach a buffer of 100x100, our scale is 4, so this should be a size of 25x25
    QImage red(100, 100, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 128));
    QSharedPointer<Buffer> redBuffer = m_shm->createBuffer(red).toStrongRef();
    QVERIFY(redBuffer);
    s->attachBuffer(redBuffer.data());
    s->damage(QRect(0,0, 25,25));
    s->commit(Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(serverSurface->size(), QSize(25,25));
    sizeChangedSpy.clear();
    bufferScaleChangedSpy.clear();

    //set the scale to 1, buffer is still 100x100 so size should change to 100x100
    s->setScale(1);
    s->commit(Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(bufferScaleChangedSpy.count(), 1);
    QCOMPARE(serverSurface->bufferScale(), 1);
    QCOMPARE(serverSurface->size(), QSize(100,100));
    sizeChangedSpy.clear();
    bufferScaleChangedSpy.clear();

    //set scale and size in one commit, buffer is 50x50 at scale 2 so size should be 25x25
    QImage blue(50, 50, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 128));
    QSharedPointer<Buffer> blueBuffer = m_shm->createBuffer(blue).toStrongRef();
    QVERIFY(blueBuffer);
    s->attachBuffer(blueBuffer.data());
    s->setScale(2);
    s->commit(Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(bufferScaleChangedSpy.count(), 1);
    QCOMPARE(serverSurface->bufferScale(), 2);
    QCOMPARE(serverSurface->size(), QSize(25,25));
}

void TestWaylandSurface::testDestroy()
{
    using namespace KWayland::Client;
    Surface *s = m_compositor->createSurface();

    connect(m_connection, &ConnectionThread::connectionDied, s, &Surface::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_compositor, &Compositor::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_shm, &ShmPool::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_queue, &EventQueue::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_idleInhibitManager, &IdleInhibitManager::destroy);
    QVERIFY(s->isValid());

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());

    delete m_compositorInterface;
    m_compositorInterface = nullptr;
    delete m_idleInhibitInterface;
    m_idleInhibitInterface = nullptr;
    delete m_display;
    m_display = nullptr;

    QVERIFY(connectionDiedSpy.wait());

    // now the Surface should be destroyed;
    QVERIFY(!s->isValid());

    // calling destroy again should not fail
    s->destroy();
}

void TestWaylandSurface::testUnmapOfNotMappedSurface()
{
    // this test verifies that a surface which doesn't have a buffer attached doesn't trigger the unmapped signal
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();

    QSignalSpy unmappedSpy(serverSurface, &SurfaceInterface::unmapped);
    QVERIFY(unmappedSpy.isValid());
    QSignalSpy bufferScaleChanged(serverSurface, &SurfaceInterface::bufferScaleChanged);

    // let's map a null buffer and change scale to trigger a signal we can wait for
    s->attachBuffer(Buffer::Ptr());
    s->setScale(2);
    s->commit(Surface::CommitFlag::None);

    QVERIFY(bufferScaleChanged.wait());
    QVERIFY(unmappedSpy.isEmpty());
}

void TestWaylandSurface::testDamageTracking()
{
    // this tests the damage tracking feature
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();

    // before first commit, the tracked damage should be empty
    QVERIFY(serverSurface->trackedDamage().isEmpty());

    // Now let's damage the surface
    QSignalSpy damagedSpy(serverSurface, &SurfaceInterface::damaged);
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 100, 100));
    s->commit(Surface::CommitFlag::None);
    QVERIFY(damagedSpy.wait());
    QCOMPARE(serverSurface->trackedDamage(), QRegion(0, 0, 100, 100));
    QCOMPARE(serverSurface->damage(), QRegion(0, 0, 100, 100));

    // resetting the tracked damage should empty it
    serverSurface->resetTrackedDamage();
    QVERIFY(serverSurface->trackedDamage().isEmpty());
    // but not affect the actual damage
    QCOMPARE(serverSurface->damage(), QRegion(0, 0, 100, 100));

    // let's damage some parts of the surface
    QPainter p;
    p.begin(&image);
    p.fillRect(QRect(0, 0, 10, 10), Qt::blue);
    p.end();
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 10, 10));
    s->commit(Surface::CommitFlag::None);
    QVERIFY(damagedSpy.wait());
    QCOMPARE(serverSurface->trackedDamage(), QRegion(0, 0, 10, 10));
    QCOMPARE(serverSurface->damage(), QRegion(0, 0, 10, 10));

    // and damage some part completely not bounding to the current damage region
    p.begin(&image);
    p.fillRect(QRect(50, 40, 20, 30), Qt::blue);
    p.end();
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(50, 40, 20, 30));
    s->commit(Surface::CommitFlag::None);
    QVERIFY(damagedSpy.wait());
    QCOMPARE(serverSurface->trackedDamage(), QRegion(0, 0, 10, 10).united(QRegion(50, 40, 20, 30)));
    QCOMPARE(serverSurface->trackedDamage().rectCount(), 2);
    QCOMPARE(serverSurface->damage(), QRegion(50, 40, 20, 30));

    // now let's reset the tracked damage again
    serverSurface->resetTrackedDamage();
    QVERIFY(serverSurface->trackedDamage().isEmpty());
    // but not affect the actual damage
    QCOMPARE(serverSurface->damage(), QRegion(50, 40, 20, 30));
}

void TestWaylandSurface::testSurfaceAt()
{
    // this test verifies that surfaceAt(const QPointF&) works as expected for the case of no children
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();

    // a newly created surface should not be mapped and not provide a surface at a position
    QVERIFY(!serverSurface->isMapped());
    QVERIFY(!serverSurface->surfaceAt(QPointF(0, 0)));

    // let's damage this surface
    QSignalSpy sizeChangedSpy(serverSurface, &SurfaceInterface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 100, 100));
    s->commit(Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    // now the surface is mapped and surfaceAt should give the surface
    QVERIFY(serverSurface->isMapped());
    QCOMPARE(serverSurface->surfaceAt(QPointF(0, 0)), serverSurface);
    QCOMPARE(serverSurface->surfaceAt(QPointF(100, 100)), serverSurface);
    // outside the geometry it should not give a surface
    QVERIFY(!serverSurface->surfaceAt(QPointF(101, 101)));
    QVERIFY(!serverSurface->surfaceAt(QPointF(-1, -1)));
}

void TestWaylandSurface::testDestroyAttachedBuffer()
{
    // this test verifies that destroying of a buffer attached to a surface works
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();

    // let's damage this surface
    QSignalSpy damagedSpy(serverSurface, &SurfaceInterface::damaged);
    QVERIFY(damagedSpy.isValid());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 100, 100));
    s->commit(Surface::CommitFlag::None);
    QVERIFY(damagedSpy.wait());
    QVERIFY(serverSurface->buffer());

    // attach another buffer
    image.fill(Qt::blue);
    s->attachBuffer(m_shm->createBuffer(image));
    m_connection->flush();

    // Let's try to destroy it
    QSignalSpy destroySpy(serverSurface->buffer(), &BufferInterface::aboutToBeDestroyed);
    QVERIFY(destroySpy.isValid());
    delete m_shm;
    m_shm = nullptr;
    QVERIFY(destroySpy.wait());

    // TODO: should this emit unmapped?
    QVERIFY(!serverSurface->buffer());
}


void TestWaylandSurface::testDestroyWithPendingCallback()
{
    // this test tries to verify that destroying a surface with a pending callback works correctly
    // first create surface
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);

    // now render to it
    QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    auto b = m_shm->createBuffer(img);
    s->attachBuffer(b);
    s->damage(QRect(0, 0, 10, 10));
    // add some frame callbacks
    for (int i = 0; i < 1000; i++) {
        wl_surface_frame(*s);
    }
    s->commit(KWayland::Client::Surface::CommitFlag::FrameCallback);
    QSignalSpy damagedSpy(serverSurface, &SurfaceInterface::damaged);
    QVERIFY(damagedSpy.isValid());
    QVERIFY(damagedSpy.wait());

    // now try to destroy the Surface again
    QSignalSpy destroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    s.reset();
    QVERIFY(destroyedSpy.wait());
}

void TestWaylandSurface::testDisconnect()
{
    // this test verifies that the server side correctly tears down the resources when the client disconnects
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);

    // destroy client
    QSignalSpy clientDisconnectedSpy(serverSurface->client(), &ClientConnection::disconnected);
    QVERIFY(clientDisconnectedSpy.isValid());
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    QVERIFY(clientDisconnectedSpy.wait());
    QCOMPARE(clientDisconnectedSpy.count(), 1);
    if (surfaceDestroyedSpy.isEmpty()) {
        QVERIFY(surfaceDestroyedSpy.wait());
    }
    QTRY_COMPARE(surfaceDestroyedSpy.count(), 1);

    s->destroy();
    m_shm->destroy();
    m_compositor->destroy();
    m_queue->destroy();
    m_idleInhibitManager->destroy();
}

void TestWaylandSurface::testOutput()
{
    // This test verifies that the enter/leave are sent correctly to the Client
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    qRegisterMetaType<KWayland::Client::Output*>();
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(!s.isNull());
    QVERIFY(s->isValid());
    QVERIFY(s->outputs().isEmpty());
    QSignalSpy enteredSpy(s.data(), &Surface::outputEntered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(s.data(), &Surface::outputLeft);
    QVERIFY(leftSpy.isValid());
    // wait for the surface on the Server side
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->outputs(), QVector<OutputInterface*>());

    // create another registry to get notified about added outputs
    Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QSignalSpy outputAnnouncedSpy(&registry, &Registry::outputAnnounced);
    QVERIFY(outputAnnouncedSpy.isValid());

    auto serverOutput = new OutputInterface(m_display, m_display);
    serverOutput->create();
    QVERIFY(outputAnnouncedSpy.wait());
    QScopedPointer<Output> clientOutput(registry.createOutput(outputAnnouncedSpy.first().first().value<quint32>(), outputAnnouncedSpy.first().last().value<quint32>()));
    QVERIFY(clientOutput->isValid());
    m_connection->flush();
    m_display->dispatchEvents();

    // now enter it
    serverSurface->setOutputs(QVector<OutputInterface*>{serverOutput});
    QCOMPARE(serverSurface->outputs(), QVector<OutputInterface*>{serverOutput});
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(enteredSpy.first().first().value<Output*>(), clientOutput.data());
    QCOMPARE(s->outputs(), QVector<Output*>{clientOutput.data()});

    // adding to same should not trigger
    serverSurface->setOutputs(QVector<OutputInterface*>{serverOutput});

    // leave again
    serverSurface->setOutputs(QVector<OutputInterface*>());
    QCOMPARE(serverSurface->outputs(), QVector<OutputInterface*>());
    QVERIFY(leftSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(leftSpy.first().first().value<Output*>(), clientOutput.data());
    QCOMPARE(s->outputs(), QVector<Output*>());

    // leave again should not trigger
    serverSurface->setOutputs(QVector<OutputInterface*>());

    // and enter again, just to verify
    serverSurface->setOutputs(QVector<OutputInterface*>{serverOutput});
    QCOMPARE(serverSurface->outputs(), QVector<OutputInterface*>{serverOutput});
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);

    //delete output client is on.
    //client should get an exit and be left on no outputs (which is allowed)
    serverOutput->deleteLater();
    QVERIFY(leftSpy.wait());
    QCOMPARE(serverSurface->outputs(), QVector<OutputInterface*>());
}

void TestWaylandSurface::testInhibit()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QScopedPointer<Surface> s(m_compositor->createSurface());
    // wait for the surface on the Server side
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->inhibitsIdle(), false);

    QSignalSpy inhibitsChangedSpy(serverSurface, &SurfaceInterface::inhibitsIdleChanged);
    QVERIFY(inhibitsChangedSpy.isValid());

    // now create an idle inhibition
    QScopedPointer<IdleInhibitor> inhibitor1(m_idleInhibitManager->createInhibitor(s.data()));
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), true);

    // creating a second idle inhibition should not trigger the signal
    QScopedPointer<IdleInhibitor> inhibitor2(m_idleInhibitManager->createInhibitor(s.data()));
    QVERIFY(!inhibitsChangedSpy.wait(500));
    QCOMPARE(serverSurface->inhibitsIdle(), true);

    // and also deleting the first inhibitor should not yet change the inhibition
    inhibitor1.reset();
    QVERIFY(!inhibitsChangedSpy.wait(500));
    QCOMPARE(serverSurface->inhibitsIdle(), true);

    // but deleting also the second inhibitor should trigger
    inhibitor2.reset();
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), false);
    QCOMPARE(inhibitsChangedSpy.count(), 2);

    // recreate inhibitor1 should inhibit again
    inhibitor1.reset(m_idleInhibitManager->createInhibitor(s.data()));
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), true);
    // and destroying should uninhibit
    inhibitor1.reset();
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), false);
    QCOMPARE(inhibitsChangedSpy.count(), 4);
}

QTEST_GUILESS_MAIN(TestWaylandSurface)
#include "test_wayland_surface.moc"
