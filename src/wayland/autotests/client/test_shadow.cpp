/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// client
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shadow.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
// server
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/shadow_interface.h"
#include "wayland/shmclientbuffer.h"

using namespace KWaylandServer;

class ShadowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateShadow();
    void testShadowElements();
    void testSurfaceDestroy();

private:
    KWaylandServer::Display *m_display = nullptr;

    KWayland::Client::ConnectionThread *m_connection = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    ShadowManagerInterface *m_shadowInterface = nullptr;
    QThread *m_thread = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::ShadowManager *m_shadow = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-shadow-0");

void ShadowTest::init()
{
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_shadowInterface = new ShadowManagerInterface(m_display, m_display);

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

    m_shm = registry.createShmPool(registry.interface(KWayland::Client::Registry::Interface::Shm).name, registry.interface(KWayland::Client::Registry::Interface::Shm).version, this);
    QVERIFY(m_shm->isValid());
    m_compositor =
        registry.createCompositor(registry.interface(KWayland::Client::Registry::Interface::Compositor).name, registry.interface(KWayland::Client::Registry::Interface::Compositor).version, this);
    QVERIFY(m_compositor->isValid());
    m_shadow =
        registry.createShadowManager(registry.interface(KWayland::Client::Registry::Interface::Shadow).name, registry.interface(KWayland::Client::Registry::Interface::Shadow).version, this);
    QVERIFY(m_shadow->isValid());
}

void ShadowTest::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    CLEANUP(m_shm)
    CLEANUP(m_compositor)
    CLEANUP(m_shadow)
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
    m_compositorInterface = nullptr;
    m_shadowInterface = nullptr;
}

void ShadowTest::testCreateShadow()
{
    // this test verifies the basic shadow behavior, create for surface, commit it, etc.
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    // a surface without anything should not have a Shadow
    QVERIFY(!serverSurface->shadow());
    QSignalSpy shadowChangedSpy(serverSurface, &SurfaceInterface::shadowChanged);

    // let's create a shadow for the Surface
    std::unique_ptr<KWayland::Client::Shadow> shadow(m_shadow->createShadow(surface.get()));
    // that should not have triggered the shadowChangedSpy)
    QVERIFY(!shadowChangedSpy.wait(100));

    // now let's commit the surface, that should trigger the shadow changed
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());
    QCOMPARE(shadowChangedSpy.count(), 1);

    // we didn't set anything on the shadow, so it should be all default values
    auto serverShadow = serverSurface->shadow();
    QVERIFY(serverShadow);
    QCOMPARE(serverShadow->offset(), QMarginsF());
    QVERIFY(!serverShadow->topLeft());
    QVERIFY(!serverShadow->top());
    QVERIFY(!serverShadow->topRight());
    QVERIFY(!serverShadow->right());
    QVERIFY(!serverShadow->bottomRight());
    QVERIFY(!serverShadow->bottom());
    QVERIFY(!serverShadow->bottomLeft());
    QVERIFY(!serverShadow->left());

    // now let's remove the shadow
    m_shadow->removeShadow(surface.get());
    // just removing should not remove it yet, surface needs to be committed
    QVERIFY(!shadowChangedSpy.wait(100));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());
    QCOMPARE(shadowChangedSpy.count(), 2);
    QVERIFY(!serverSurface->shadow());
}

static QImage bufferToImage(ClientBuffer *clientBuffer)
{
    auto shmBuffer = qobject_cast<ShmClientBuffer *>(clientBuffer);
    if (shmBuffer) {
        return shmBuffer->data();
    }
    return QImage();
}

void ShadowTest::testShadowElements()
{
    // this test verifies that all shadow elements are correctly passed to the server
    // first create surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    QSignalSpy shadowChangedSpy(serverSurface, &SurfaceInterface::shadowChanged);

    // now create the shadow
    std::unique_ptr<KWayland::Client::Shadow> shadow(m_shadow->createShadow(surface.get()));
    QImage topLeftImage(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    topLeftImage.fill(Qt::white);
    shadow->attachTopLeft(m_shm->createBuffer(topLeftImage));
    QImage topImage(QSize(11, 11), QImage::Format_ARGB32_Premultiplied);
    topImage.fill(Qt::black);
    shadow->attachTop(m_shm->createBuffer(topImage));
    QImage topRightImage(QSize(12, 12), QImage::Format_ARGB32_Premultiplied);
    topRightImage.fill(Qt::red);
    shadow->attachTopRight(m_shm->createBuffer(topRightImage));
    QImage rightImage(QSize(13, 13), QImage::Format_ARGB32_Premultiplied);
    rightImage.fill(Qt::darkRed);
    shadow->attachRight(m_shm->createBuffer(rightImage));
    QImage bottomRightImage(QSize(14, 14), QImage::Format_ARGB32_Premultiplied);
    bottomRightImage.fill(Qt::green);
    shadow->attachBottomRight(m_shm->createBuffer(bottomRightImage));
    QImage bottomImage(QSize(15, 15), QImage::Format_ARGB32_Premultiplied);
    bottomImage.fill(Qt::darkGreen);
    shadow->attachBottom(m_shm->createBuffer(bottomImage));
    QImage bottomLeftImage(QSize(16, 16), QImage::Format_ARGB32_Premultiplied);
    bottomLeftImage.fill(Qt::blue);
    shadow->attachBottomLeft(m_shm->createBuffer(bottomLeftImage));
    QImage leftImage(QSize(17, 17), QImage::Format_ARGB32_Premultiplied);
    leftImage.fill(Qt::darkBlue);
    shadow->attachLeft(m_shm->createBuffer(leftImage));
    shadow->setOffsets(QMarginsF(1, 2, 3, 4));
    shadow->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(shadowChangedSpy.wait());
    auto serverShadow = serverSurface->shadow();
    QVERIFY(serverShadow);
    QCOMPARE(serverShadow->offset(), QMarginsF(1, 2, 3, 4));
    QCOMPARE(bufferToImage(serverShadow->topLeft()), topLeftImage);
    QCOMPARE(bufferToImage(serverShadow->top()), topImage);
    QCOMPARE(bufferToImage(serverShadow->topRight()), topRightImage);
    QCOMPARE(bufferToImage(serverShadow->right()), rightImage);
    QCOMPARE(bufferToImage(serverShadow->bottomRight()), bottomRightImage);
    QCOMPARE(bufferToImage(serverShadow->bottom()), bottomImage);
    QCOMPARE(bufferToImage(serverShadow->bottomLeft()), bottomLeftImage);
    QCOMPARE(bufferToImage(serverShadow->left()), leftImage);
}

void ShadowTest::testSurfaceDestroy()
{
    using namespace KWaylandServer;
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    auto serverSurface = serverSurfaceCreated.first().first().value<SurfaceInterface *>();
    QSignalSpy shadowChangedSpy(serverSurface, &SurfaceInterface::shadowChanged);

    std::unique_ptr<KWayland::Client::Shadow> shadow(m_shadow->createShadow(surface.get()));
    shadow->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());
    auto serverShadow = serverSurface->shadow();
    QVERIFY(serverShadow);

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QSignalSpy shadowDestroyedSpy(serverShadow.data(), &QObject::destroyed);
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(shadowDestroyedSpy.isEmpty());
    // destroy the shadow
    shadow.reset();
    QVERIFY(shadowDestroyedSpy.wait());
    QCOMPARE(shadowDestroyedSpy.count(), 1);
}

QTEST_GUILESS_MAIN(ShadowTest)
#include "test_shadow.moc"
