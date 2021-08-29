/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "KWayland/Client/shadow.h"
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shell.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
// Qt
#include <QGuiApplication>
#include <QImage>
#include <QThread>

using namespace KWayland::Client;

class ShadowTest : public QObject
{
    Q_OBJECT
public:
    explicit ShadowTest(QObject *parent = nullptr);
    virtual ~ShadowTest();

    void init();

private:
    void setupRegistry(Registry *registry);
    void setupShadow();
    void render();
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    EventQueue *m_eventQueue = nullptr;
    Compositor *m_compositor = nullptr;
    Shell *m_shell = nullptr;
    ShellSurface *m_shellSurface = nullptr;
    ShmPool *m_shm = nullptr;
    Surface *m_surface = nullptr;
    ShadowManager *m_shadowManager = nullptr;
};

ShadowTest::ShadowTest(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

ShadowTest::~ShadowTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void ShadowTest::init()
{
    connect(
        m_connectionThreadObject,
        &ConnectionThread::connected,
        this,
        [this] {
            m_eventQueue = new EventQueue(this);
            m_eventQueue->setup(m_connectionThreadObject);

            Registry *registry = new Registry(this);
            setupRegistry(registry);
        },
        Qt::QueuedConnection);
    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();

    m_connectionThreadObject->initConnection();
}

void ShadowTest::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::compositorAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_compositor = registry->createCompositor(name, version, this);
    });
    connect(registry, &Registry::shellAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_shell = registry->createShell(name, version, this);
    });
    connect(registry, &Registry::shmAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_shm = registry->createShmPool(name, version, this);
    });
    connect(registry, &Registry::shadowAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_shadowManager = registry->createShadowManager(name, version, this);
        m_shadowManager->setEventQueue(m_eventQueue);
    });
    connect(registry, &Registry::interfacesAnnounced, this, [this] {
        Q_ASSERT(m_compositor);
        Q_ASSERT(m_shell);
        Q_ASSERT(m_shm);
        m_surface = m_compositor->createSurface(this);
        Q_ASSERT(m_surface);
        setupShadow();
        m_shellSurface = m_shell->createSurface(m_surface, this);
        Q_ASSERT(m_shellSurface);
        m_shellSurface->setToplevel();
        connect(m_shellSurface, &ShellSurface::sizeChanged, this, &ShadowTest::render);
        render();
    });
    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void ShadowTest::setupShadow()
{
    Q_ASSERT(m_shadowManager);
    Shadow *shadow = m_shadowManager->createShadow(m_surface, this);
    Q_ASSERT(shadow);

    auto addElement = [this](const QColor color) {
        const QSize size = QSize(10, 10);
        auto buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
        buffer->setUsed(true);
        QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
        image.fill(color);
        return buffer;
    };
    shadow->attachTopLeft(addElement(Qt::red));
    shadow->attachTop(addElement(Qt::darkRed));
    shadow->attachTopRight(addElement(Qt::green));
    shadow->attachRight(addElement(Qt::darkGreen));
    shadow->attachBottomRight(addElement(Qt::darkBlue));
    shadow->attachBottom(addElement(Qt::cyan));
    shadow->attachBottomLeft(addElement(Qt::darkCyan));
    shadow->attachLeft(addElement(Qt::magenta));
    shadow->setOffsets(QMarginsF(5, 5, 5, 5));
    shadow->commit();
}

void ShadowTest::render()
{
    const QSize &size = m_shellSurface->size().isValid() ? m_shellSurface->size() : QSize(300, 200);
    auto buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
    buffer->setUsed(true);
    QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 255, 255, 128));

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), size));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ShadowTest client;
    client.init();

    return app.exec();
}

#include "shadowtest.moc"
