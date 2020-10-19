/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
#include <QImage>
// KWin
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shm_pool.h"
#include "../../src/server/buffer_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/surface_interface.h"

class TestShmPool : public QObject
{
    Q_OBJECT
public:
    explicit TestShmPool(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateBufferNullImage();
    void testCreateBufferNullSize();
    void testCreateBufferInvalidSize();
    void testCreateBufferFromImage();
    void testCreateBufferFromImageWithAlpha();
    void testCreateBufferFromData();
    void testReuseBuffer();
    void testDestroy();

private:
    KWaylandServer::Display *m_display;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::ShmPool *m_shmPool;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-surface-0");

TestShmPool::TestShmPool(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_shmPool(nullptr)
    , m_thread(nullptr)
{
}

void TestShmPool::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
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

    KWayland::Client::Registry registry;
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    // here we need a shm pool
    m_display->createShm();

    QVERIFY(shmSpy.wait());
    m_shmPool = registry.createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
}

void TestShmPool::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_shmPool) {
        delete m_shmPool;
        m_shmPool = nullptr;
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

void TestShmPool::testCreateBufferNullImage()
{
    QVERIFY(m_shmPool->isValid());
    QImage img;
    QVERIFY(img.isNull());
    QVERIFY(!m_shmPool->createBuffer(img));
}

void TestShmPool::testCreateBufferNullSize()
{
    QVERIFY(m_shmPool->isValid());
    QSize size(0, 0);
    QVERIFY(size.isNull());
    QVERIFY(!m_shmPool->createBuffer(size, 0, nullptr));
}

void TestShmPool::testCreateBufferInvalidSize()
{
    QVERIFY(m_shmPool->isValid());
    QSize size;
    QVERIFY(!size.isValid());
    QVERIFY(!m_shmPool->createBuffer(size, 0, nullptr));
}

void TestShmPool::testCreateBufferFromImage()
{
    QVERIFY(m_shmPool->isValid());
    QImage img(24, 24, QImage::Format_RGB32);
    img.fill(Qt::black);
    QVERIFY(!img.isNull());
    auto buffer = m_shmPool->createBuffer(img).toStrongRef();
    QVERIFY(buffer);
    QCOMPARE(buffer->size(), img.size());
    QImage img2(buffer->address(), img.width(), img.height(), QImage::Format_RGB32);
    QCOMPARE(img2, img);
}

void TestShmPool::testCreateBufferFromImageWithAlpha()
{
    QVERIFY(m_shmPool->isValid());
    QImage img(24, 24, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(255,0,0,100)); //red with alpha
    QVERIFY(!img.isNull());
    auto buffer = m_shmPool->createBuffer(img).toStrongRef();
    QVERIFY(buffer);
    QCOMPARE(buffer->size(), img.size());
    QImage img2(buffer->address(), img.width(), img.height(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(img2, img);
}

void TestShmPool::testCreateBufferFromData()
{
    QVERIFY(m_shmPool->isValid());
    QImage img(24, 24, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QVERIFY(!img.isNull());
    auto buffer = m_shmPool->createBuffer(img.size(), img.bytesPerLine(), img.constBits()).toStrongRef();
    QVERIFY(buffer);
    QCOMPARE(buffer->size(), img.size());
    QImage img2(buffer->address(), img.width(), img.height(), QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(img2, img);
}

void TestShmPool::testReuseBuffer()
{
    QVERIFY(m_shmPool->isValid());
    QImage img(24, 24, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QVERIFY(!img.isNull());
    auto buffer = m_shmPool->createBuffer(img).toStrongRef();
    QVERIFY(buffer);
    buffer->setReleased(true);
    buffer->setUsed(false);

    // same image should get the same buffer
    auto buffer2 = m_shmPool->createBuffer(img).toStrongRef();
    QCOMPARE(buffer, buffer2);
    buffer2->setReleased(true);
    buffer2->setUsed(false);

    // image with different size should get us a new buffer
    auto buffer3 = m_shmPool->getBuffer(QSize(10, 10), 8);
    QVERIFY(buffer3 != buffer2);

    // image with a different format should get us a new buffer
    QImage img2(24, 24, QImage::Format_RGB32);
    img2.fill(Qt::black);
    QVERIFY(!img2.isNull());
    auto buffer4 = m_shmPool->createBuffer(img2).toStrongRef();
    QVERIFY(buffer4);
    QVERIFY(buffer4 != buffer2);
    QVERIFY(buffer4 != buffer3);
}

void TestShmPool::testDestroy()
{
    using namespace KWayland::Client;
    connect(m_connection, &ConnectionThread::connectionDied, m_shmPool, &ShmPool::destroy);
    QVERIFY(m_shmPool->isValid());

    // let's create one Buffer
    m_shmPool->getBuffer(QSize(10, 10), 8);

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    // now the pool should be destroyed;
    QVERIFY(!m_shmPool->isValid());

    // calling destroy again should not fail
    m_shmPool->destroy();
}

QTEST_GUILESS_MAIN(TestShmPool)
#include "test_shm_pool.moc"
