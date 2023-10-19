/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
// KWin
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferview.h"
#include "wayland/compositor.h"
#include "wayland/display.h"
#include "wayland/idleinhibit_v1.h"
#include "wayland/output.h"
#include "wayland/surface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/idleinhibit.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"

#include "../../tests/fakeoutput.h"

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
    void testOpaque();
    void testInput();
    void testScale();
    void testUnmapOfNotMappedSurface();
    void testSurfaceAt();
    void testDestroyAttachedBuffer();
    void testDestroyWithPendingCallback();
    void testOutput();
    void testDisconnect();
    void testInhibit();

private:
    KWin::Display *m_display;
    KWin::CompositorInterface *m_compositorInterface;
    KWin::IdleInhibitManagerV1Interface *m_idleInhibitInterface;
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
    using namespace KWin;
    delete m_display;
    m_display = new KWin::Display(this);
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
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
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
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy shmSpy(&registry, &KWayland::Client::Registry::shmAnnounced);
    QSignalSpy allAnnounced(&registry, &KWayland::Client::Registry::interfacesAnnounced);
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

    m_idleInhibitManager = registry.createIdleInhibitManager(registry.interface(KWayland::Client::Registry::Interface::IdleInhibitManagerUnstableV1).name,
                                                             registry.interface(KWayland::Client::Registry::Interface::IdleInhibitManagerUnstableV1).version,
                                                             this);
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
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWin::CompositorInterface::surfaceCreated);

    QVERIFY(!KWin::SurfaceInterface::get(nullptr));
    QVERIFY(!KWin::SurfaceInterface::get(1, nullptr));
    QVERIFY(KWayland::Client::Surface::all().isEmpty());
    std::unique_ptr<KWayland::Client::Surface> s1(m_compositor->createSurface());
    QVERIFY(s1->isValid());
    QCOMPARE(KWayland::Client::Surface::all().count(), 1);
    QCOMPARE(KWayland::Client::Surface::all().first(), s1.get());
    QCOMPARE(KWayland::Client::Surface::get(*s1), s1.get());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface1 = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QVERIFY(serverSurface1);
    QCOMPARE(KWin::SurfaceInterface::get(serverSurface1->resource()), serverSurface1);
    QCOMPARE(KWin::SurfaceInterface::get(serverSurface1->id(), serverSurface1->client()), serverSurface1);

    QVERIFY(!s1->size().isValid());
    QSignalSpy sizeChangedSpy(s1.get(), &KWayland::Client::Surface::sizeChanged);
    const QSize testSize(200, 300);
    s1->setSize(testSize);
    QCOMPARE(s1->size(), testSize);
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(sizeChangedSpy.first().first().toSize(), testSize);

    // add another surface
    std::unique_ptr<KWayland::Client::Surface> s2(m_compositor->createSurface());
    QVERIFY(s2->isValid());
    QCOMPARE(KWayland::Client::Surface::all().count(), 2);
    QCOMPARE(KWayland::Client::Surface::all().first(), s1.get());
    QCOMPARE(KWayland::Client::Surface::all().last(), s2.get());
    QCOMPARE(KWayland::Client::Surface::get(*s1), s1.get());
    QCOMPARE(KWayland::Client::Surface::get(*s2), s2.get());
    serverSurfaceCreated.clear();
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface2 = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QVERIFY(serverSurface2);
    QCOMPARE(KWin::SurfaceInterface::get(serverSurface1->resource()), serverSurface1);
    QCOMPARE(KWin::SurfaceInterface::get(serverSurface1->id(), serverSurface1->client()), serverSurface1);
    QCOMPARE(KWin::SurfaceInterface::get(serverSurface2->resource()), serverSurface2);
    QCOMPARE(KWin::SurfaceInterface::get(serverSurface2->id(), serverSurface2->client()), serverSurface2);

    const quint32 surfaceId1 = serverSurface1->id();
    const quint32 surfaceId2 = serverSurface2->id();

    // delete s2 again
    s2.reset();
    QCOMPARE(KWayland::Client::Surface::all().count(), 1);
    QCOMPARE(KWayland::Client::Surface::all().first(), s1.get());
    QCOMPARE(KWayland::Client::Surface::get(*s1), s1.get());

    // and finally delete the last one
    s1.reset();
    QVERIFY(KWayland::Client::Surface::all().isEmpty());
    QVERIFY(!KWayland::Client::Surface::get(nullptr));
    QSignalSpy destroyedSpy(serverSurface1, &KWin::SurfaceInterface::destroyed);
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!KWin::SurfaceInterface::get(nullptr));
    QVERIFY(!KWin::SurfaceInterface::get(surfaceId1, nullptr));
    QVERIFY(!KWin::SurfaceInterface::get(surfaceId2, nullptr));
}

void TestWaylandSurface::testDamage()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWin::CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    s->setScale(2);
    QVERIFY(serverSurfaceCreated.wait());
    KWin::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->bufferDamage(), QRegion());
    QVERIFY(!serverSurface->isMapped());

    QSignalSpy committedSpy(serverSurface, &KWin::SurfaceInterface::committed);
    QSignalSpy damageSpy(serverSurface, &KWin::SurfaceInterface::damaged);

    // send damage without a buffer
    {
        s->damage(QRect(0, 0, 100, 100));
        s->commit(KWayland::Client::Surface::CommitFlag::None);
        wl_display_flush(m_connection->display());
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(damageSpy.isEmpty());
        QVERIFY(!serverSurface->isMapped());
        QCOMPARE(committedSpy.count(), 1);
    }

    // surface damage
    {
        QImage img(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        auto b = m_shm->createBuffer(img);
        s->attachBuffer(b, QPoint(55, 55));
        s->damage(QRect(0, 0, 10, 10));
        s->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(damageSpy.wait());
        QCOMPARE(serverSurface->offset(), QPoint(55, 55)); // offset is surface local so scale doesn't change this
        QCOMPARE(serverSurface->bufferDamage(), QRegion(0, 0, 10, 10));
        QCOMPARE(damageSpy.first().first().value<QRegion>(), QRegion(0, 0, 10, 10));
        QVERIFY(serverSurface->isMapped());
        QCOMPARE(committedSpy.count(), 2);
    }

    // damage multiple times
    {
        const QRegion surfaceDamage = QRegion(5, 8, 3, 6).united(QRect(10, 11, 6, 1));
        const QRegion expectedDamage = QRegion(10, 16, 6, 12).united(QRect(20, 22, 12, 2));
        QImage img(QSize(40, 35), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        auto b = m_shm->createBuffer(img);
        s->attachBuffer(b);
        s->damage(surfaceDamage);
        damageSpy.clear();
        s->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(damageSpy.wait());
        QCOMPARE(serverSurface->bufferDamage(), expectedDamage);
        QCOMPARE(damageSpy.first().first().value<QRegion>(), expectedDamage);
        QVERIFY(serverSurface->isMapped());
        QCOMPARE(committedSpy.count(), 3);
    }

    // damage buffer
    {
        const QRegion damage(30, 40, 22, 4);
        QImage img(QSize(80, 70), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        auto b = m_shm->createBuffer(img);
        s->attachBuffer(b);
        s->damageBuffer(damage);
        damageSpy.clear();
        s->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(damageSpy.wait());
        QCOMPARE(serverSurface->bufferDamage(), damage);
        QCOMPARE(damageSpy.first().first().value<QRegion>(), damage);
        QVERIFY(serverSurface->isMapped());
    }

    // combined regular damage and damaged buffer
    {
        const QRegion surfaceDamage(10, 20, 5, 5);
        const QRegion bufferDamage(30, 50, 50, 20);
        const QRegion expectedDamage = QRegion(20, 40, 10, 10).united(QRect(30, 50, 50, 20));
        QImage img(QSize(80, 70), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        auto b = m_shm->createBuffer(img);
        s->attachBuffer(b);
        s->damage(surfaceDamage);
        s->damageBuffer(bufferDamage);
        damageSpy.clear();
        s->commit(KWayland::Client::Surface::CommitFlag::None);
        QVERIFY(damageSpy.wait());
        QCOMPARE(serverSurface->bufferDamage(), expectedDamage);
        QCOMPARE(damageSpy.first().first().value<QRegion>(), expectedDamage);
        QVERIFY(serverSurface->isMapped());
    }
}

void TestWaylandSurface::testFrameCallback()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWin::CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    KWin::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QVERIFY(serverSurface);

    QSignalSpy damageSpy(serverSurface, &KWin::SurfaceInterface::damaged);

    QSignalSpy frameRenderedSpy(s.get(), &KWayland::Client::Surface::frameRendered);
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
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWin::CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    KWin::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QVERIFY(serverSurface);

    // create three images
    QImage black(24, 24, QImage::Format_RGB32);
    black.fill(Qt::black);
    QImage red(24, 24, QImage::Format_ARGB32); // Note - deliberately not premultiplied
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
    QSignalSpy damageSpy(serverSurface, &KWin::SurfaceInterface::damaged);
    QSignalSpy mappedSpy(serverSurface, &KWin::SurfaceInterface::mapped);
    QSignalSpy unmappedSpy(serverSurface, &KWin::SurfaceInterface::unmapped);
    QVERIFY(damageSpy.wait());
    QCOMPARE(mappedSpy.count(), 1);
    QVERIFY(unmappedSpy.isEmpty());

    // now the ServerSurface should have the black image attached as a buffer
    KWin::GraphicsBuffer *buffer = serverSurface->buffer();
    buffer->ref();
    {
        KWin::GraphicsBufferView view(buffer);
        QVERIFY(view.image());
        QCOMPARE(*view.image(), black);
        QCOMPARE(view.image()->format(), QImage::Format_RGB32);
        QCOMPARE(view.image()->size(), QSize(24, 24));
    }

    // render another frame
    s->attachBuffer(redBuffer);
    s->damage(QRect(0, 0, 24, 24));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    damageSpy.clear();
    QVERIFY(damageSpy.wait());
    QCOMPARE(mappedSpy.count(), 1);
    QVERIFY(unmappedSpy.isEmpty());
    KWin::GraphicsBuffer *buffer2 = serverSurface->buffer();
    buffer2->ref();
    {
        KWin::GraphicsBufferView view(buffer2);
        QVERIFY(view.image());
        QCOMPARE(view.image()->format(), QImage::Format_ARGB32_Premultiplied);
        QCOMPARE(view.image()->size(), QSize(24, 24));
        for (int i = 0; i < 24; ++i) {
            for (int j = 0; j < 24; ++j) {
                // it's premultiplied in the format
                QCOMPARE(view.image()->pixel(i, j), qRgba(128, 0, 0, 128));
            }
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
    QSignalSpy frameRenderedSpy(s.get(), &KWayland::Client::Surface::frameRendered);
    s->commit();
    damageSpy.clear();
    QVERIFY(damageSpy.wait());
    QCOMPARE(mappedSpy.count(), 1);
    QVERIFY(unmappedSpy.isEmpty());
    QVERIFY(!buffer2->isReferenced());
    // TODO: we should have a signal on when the Buffer gets released
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    if (!redBuffer.data()->isReleased()) {
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    }
    QVERIFY(redBuffer.data()->isReleased());

    KWin::GraphicsBuffer *buffer3 = serverSurface->buffer();
    buffer3->ref();
    {
        KWin::GraphicsBufferView view(buffer3);
        QVERIFY(view.image());
        QCOMPARE(view.image()->format(), QImage::Format_ARGB32_Premultiplied);
        QCOMPARE(view.image()->size(), QSize(24, 24));
        for (int i = 0; i < 24; ++i) {
            for (int j = 0; j < 24; ++j) {
                // it's premultiplied in the format
                QCOMPARE(view.image()->pixel(i, j), qRgba(0, 0, 128, 128));
            }
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
    s->attachBuffer((wl_buffer *)nullptr);
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

void TestWaylandSurface::testOpaque()
{
    using namespace KWin;
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWin::CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QVERIFY(serverSurface);
    QSignalSpy opaqueRegionChangedSpy(serverSurface, &KWin::SurfaceInterface::opaqueChanged);

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
    QImage black(20, 40, QImage::Format_ARGB32_Premultiplied);
    black.fill(Qt::black);
    QSharedPointer<KWayland::Client::Buffer> buffer1 = m_shm->createBuffer(black).toStrongRef();
    s->attachBuffer(buffer1);
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(opaqueRegionChangedSpy.wait());
    QCOMPARE(opaqueRegionChangedSpy.count(), 1);
    QCOMPARE(opaqueRegionChangedSpy.last().first().value<QRegion>(), QRegion(0, 10, 20, 30));
    QCOMPARE(serverSurface->opaque(), QRegion(0, 10, 20, 30));

    // committing without setting a new region shouldn't change
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(opaqueRegionChangedSpy.count(), 1);
    QCOMPARE(serverSurface->opaque(), QRegion(0, 10, 20, 30));

    // let's change the opaque region, it will be clipped with rect(0, 0, 20, 40)
    s->setOpaqueRegion(m_compositor->createRegion(QRegion(10, 20, 30, 40)).get());
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(opaqueRegionChangedSpy.wait());
    QCOMPARE(opaqueRegionChangedSpy.count(), 2);
    QCOMPARE(opaqueRegionChangedSpy.last().first().value<QRegion>(), QRegion(10, 20, 10, 20));
    QCOMPARE(serverSurface->opaque(), QRegion(10, 20, 10, 20));

    // and let's go back to an empty region
    s->setOpaqueRegion();
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(opaqueRegionChangedSpy.wait());
    QCOMPARE(opaqueRegionChangedSpy.count(), 3);
    QCOMPARE(opaqueRegionChangedSpy.last().first().value<QRegion>(), QRegion());
    QCOMPARE(serverSurface->opaque(), QRegion());
}

void TestWaylandSurface::testInput()
{
    using namespace KWin;
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWin::CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QVERIFY(serverSurface);
    QSignalSpy inputRegionChangedSpy(serverSurface, &KWin::SurfaceInterface::inputChanged);
    QSignalSpy committedSpy(serverSurface, &SurfaceInterface::committed);

    // the input region should be empty if the surface has no buffer
    QVERIFY(!serverSurface->isMapped());
    QCOMPARE(serverSurface->input(), QRegion());

    // the default input region is infinite
    QImage black(100, 50, QImage::Format_RGB32);
    black.fill(Qt::black);
    QSharedPointer<KWayland::Client::Buffer> buffer1 = m_shm->createBuffer(black).toStrongRef();
    QVERIFY(buffer1);
    s->attachBuffer(buffer1);
    s->commit(KWayland::Client::Surface::CommitFlag::None);
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
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(inputRegionChangedSpy.count(), 2);
    QCOMPARE(serverSurface->input(), QRegion(0, 10, 20, 30));

    // committing without setting a new region shouldn't change
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(inputRegionChangedSpy.count(), 2);
    QCOMPARE(serverSurface->input(), QRegion(0, 10, 20, 30));

    // let's change the input region, note that the new input region is cropped
    s->setInputRegion(m_compositor->createRegion(QRegion(10, 20, 30, 40)).get());
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(inputRegionChangedSpy.count(), 3);
    QCOMPARE(serverSurface->input(), QRegion(10, 20, 30, 30));

    // and let's go back to an empty region
    s->setInputRegion();
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(committedSpy.wait());
    QCOMPARE(inputRegionChangedSpy.count(), 4);
    QCOMPARE(serverSurface->input(), QRegion(0, 0, 100, 50));
}

void TestWaylandSurface::testScale()
{
    // this test verifies that updating the scale factor is correctly passed to the Wayland server
    using namespace KWin;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QCOMPARE(s->scale(), 1);
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();
    QVERIFY(serverSurface);

    // changing the scale implicitly changes the size
    QSignalSpy sizeChangedSpy(serverSurface, &SurfaceInterface::sizeChanged);

    // attach a buffer of 100x100
    QImage red(100, 100, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 128));
    QSharedPointer<KWayland::Client::Buffer> redBuffer = m_shm->createBuffer(red).toStrongRef();
    QVERIFY(redBuffer);
    s->attachBuffer(redBuffer.data());
    s->damageBuffer(QRect(0, 0, 100, 100));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(serverSurface->size(), QSize(100, 100));

    // set the scale to 2, buffer is still 100x100 so size should change to 50x50
    s->setScale(2);
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 2);
    QCOMPARE(serverSurface->size(), QSize(50, 50));

    // set scale and size in one commit, buffer is 60x60 at scale 3 so size should be 20x20
    QImage blue(60, 60, QImage::Format_ARGB32_Premultiplied);
    red.fill(QColor(255, 0, 0, 128));
    QSharedPointer<KWayland::Client::Buffer> blueBuffer = m_shm->createBuffer(blue).toStrongRef();
    QVERIFY(blueBuffer);
    s->attachBuffer(blueBuffer.data());
    s->setScale(3);
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());
    QCOMPARE(sizeChangedSpy.count(), 3);
    QCOMPARE(serverSurface->size(), QSize(20, 20));
}

void TestWaylandSurface::testUnmapOfNotMappedSurface()
{
    // this test verifies that a surface which doesn't have a buffer attached doesn't trigger the unmapped signal
    using namespace KWin;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();

    QSignalSpy unmappedSpy(serverSurface, &SurfaceInterface::unmapped);
    QSignalSpy committedSpy(serverSurface, &SurfaceInterface::committed);

    // let's map a null buffer and change scale to trigger a signal we can wait for
    s->attachBuffer(KWayland::Client::Buffer::Ptr());
    s->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(committedSpy.wait());
    QVERIFY(unmappedSpy.isEmpty());
}

void TestWaylandSurface::testSurfaceAt()
{
    // this test verifies that surfaceAt(const QPointF&) works as expected for the case of no children
    using namespace KWin;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();

    // a newly created surface should not be mapped and not provide a surface at a position
    QVERIFY(!serverSurface->isMapped());
    QVERIFY(!serverSurface->surfaceAt(QPointF(0, 0)));

    // let's damage this surface
    QSignalSpy sizeChangedSpy(serverSurface, &SurfaceInterface::sizeChanged);
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 100, 100));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(sizeChangedSpy.wait());

    // now the surface is mapped and surfaceAt should give the surface
    QVERIFY(serverSurface->isMapped());
    QCOMPARE(serverSurface->surfaceAt(QPointF(0, 0)), serverSurface);
    QCOMPARE(serverSurface->surfaceAt(QPointF(99, 99)), serverSurface);
    // outside the geometry it should not give a surface
    QVERIFY(!serverSurface->surfaceAt(QPointF(100, 100)));
    QVERIFY(!serverSurface->surfaceAt(QPointF(-1, -1)));
}

void TestWaylandSurface::testDestroyAttachedBuffer()
{
    // this test verifies that destroying of a buffer attached to a surface works
    using namespace KWin;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::SurfaceInterface *>();

    // let's damage this surface
    QSignalSpy damagedSpy(serverSurface, &SurfaceInterface::damaged);
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(QRect(0, 0, 100, 100));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(damagedSpy.wait());
    QVERIFY(serverSurface->buffer());

    // attach another buffer
    image.fill(Qt::blue);
    s->attachBuffer(m_shm->createBuffer(image));
    m_connection->flush();

    // Let's try to destroy it
    delete m_shm;
    m_shm = nullptr;
    QTRY_VERIFY(serverSurface->buffer()->isDropped());
}

void TestWaylandSurface::testDestroyWithPendingCallback()
{
    // this test tries to verify that destroying a surface with a pending callback works correctly
    // first create surface
    using namespace KWin;
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
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
    QVERIFY(damagedSpy.wait());

    // now try to destroy the Surface again
    QSignalSpy destroyedSpy(serverSurface, &QObject::destroyed);
    s.reset();
    QVERIFY(destroyedSpy.wait());
}

void TestWaylandSurface::testDisconnect()
{
    // this test verifies that the server side correctly tears down the resources when the client disconnects
    using namespace KWin;
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    // destroy client
    QSignalSpy clientDisconnectedSpy(serverSurface->client(), &ClientConnection::disconnected);
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
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
    using namespace KWin;
    qRegisterMetaType<KWayland::Client::Output *>();
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    QVERIFY(s != nullptr);
    QVERIFY(s->isValid());
    QVERIFY(s->outputs().isEmpty());
    QSignalSpy enteredSpy(s.get(), &KWayland::Client::Surface::outputEntered);
    QSignalSpy leftSpy(s.get(), &KWayland::Client::Surface::outputLeft);
    // wait for the surface on the Server side
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->outputs(), QList<OutputInterface *>());

    // create another registry to get notified about added outputs
    KWayland::Client::Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy allAnnounced(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QSignalSpy outputAnnouncedSpy(&registry, &KWayland::Client::Registry::outputAnnounced);

    auto outputHandle = std::make_unique<FakeOutput>();
    auto serverOutput = std::make_unique<OutputInterface>(m_display, outputHandle.get());
    QVERIFY(outputAnnouncedSpy.wait());
    std::unique_ptr<KWayland::Client::Output> clientOutput(
        registry.createOutput(outputAnnouncedSpy.first().first().value<quint32>(), outputAnnouncedSpy.first().last().value<quint32>()));
    QVERIFY(clientOutput->isValid());
    m_connection->flush();
    m_display->dispatchEvents();

    // now enter it
    serverSurface->setOutputs(QList<OutputInterface *>{serverOutput.get()});
    QCOMPARE(serverSurface->outputs(), QList<OutputInterface *>{serverOutput.get()});
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(enteredSpy.first().first().value<KWayland::Client::Output *>(), clientOutput.get());
    QCOMPARE(s->outputs(), QList<KWayland::Client::Output *>{clientOutput.get()});

    // adding to same should not trigger
    serverSurface->setOutputs(QList<OutputInterface *>{serverOutput.get()});

    // leave again
    serverSurface->setOutputs(QList<OutputInterface *>());
    QCOMPARE(serverSurface->outputs(), QList<OutputInterface *>());
    QVERIFY(leftSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(leftSpy.first().first().value<KWayland::Client::Output *>(), clientOutput.get());
    QCOMPARE(s->outputs(), QList<KWayland::Client::Output *>());

    // leave again should not trigger
    serverSurface->setOutputs(QList<OutputInterface *>());

    // and enter again, just to verify
    serverSurface->setOutputs(QList<OutputInterface *>{serverOutput.get()});
    QCOMPARE(serverSurface->outputs(), QList<OutputInterface *>{serverOutput.get()});
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);

    // delete output client is on.
    // client should get an exit and be left on no outputs (which is allowed)
    serverOutput.reset();
    outputHandle.reset();
    QVERIFY(leftSpy.wait());
    QCOMPARE(serverSurface->outputs(), QList<OutputInterface *>());
}

void TestWaylandSurface::testInhibit()
{
    using namespace KWin;
    std::unique_ptr<KWayland::Client::Surface> s(m_compositor->createSurface());
    // wait for the surface on the Server side
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->inhibitsIdle(), false);

    QSignalSpy inhibitsChangedSpy(serverSurface, &SurfaceInterface::inhibitsIdleChanged);

    // now create an idle inhibition
    std::unique_ptr<KWayland::Client::IdleInhibitor> inhibitor1(m_idleInhibitManager->createInhibitor(s.get()));
    QVERIFY(inhibitsChangedSpy.wait());
    QCOMPARE(serverSurface->inhibitsIdle(), true);

    // creating a second idle inhibition should not trigger the signal
    std::unique_ptr<KWayland::Client::IdleInhibitor> inhibitor2(m_idleInhibitManager->createInhibitor(s.get()));
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
    inhibitor1.reset(m_idleInhibitManager->createInhibitor(s.get()));
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
