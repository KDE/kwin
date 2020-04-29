/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/dataoffer.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shell.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/subcompositor.h"
#include "KWayland/Client/surface.h"
// Qt
#include <QGuiApplication>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QMimeType>
#include <QThread>
#include <QTimer>
// system
#include <unistd.h>

#include <linux/input.h>

using namespace KWayland::Client;

class SubSurfaceTest : public QObject
{
    Q_OBJECT
public:
    explicit SubSurfaceTest(QObject *parent = nullptr);
    virtual ~SubSurfaceTest();

    void init();

private:
    void setupRegistry(Registry *registry);
    void render();
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    EventQueue *m_eventQueue = nullptr;
    Compositor *m_compositor = nullptr;
    Seat *m_seat = nullptr;
    Shell *m_shell = nullptr;
    ShellSurface *m_shellSurface = nullptr;
    ShmPool *m_shm = nullptr;
    Surface *m_surface = nullptr;
    SubCompositor *m_subCompositor = nullptr;
};

SubSurfaceTest::SubSurfaceTest(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

SubSurfaceTest::~SubSurfaceTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void SubSurfaceTest::init()
{
    connect(m_connectionThreadObject, &ConnectionThread::connected, this,
        [this] {
            m_eventQueue = new EventQueue(this);
            m_eventQueue->setup(m_connectionThreadObject);

            Registry *registry = new Registry(this);
            setupRegistry(registry);
        },
        Qt::QueuedConnection
    );
    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();

    m_connectionThreadObject->initConnection();
}


void SubSurfaceTest::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::compositorAnnounced, this,
        [this, registry](quint32 name, quint32 version) {
            m_compositor = registry->createCompositor(name, version, this);
        }
    );
    connect(registry, &Registry::shellAnnounced, this,
        [this, registry](quint32 name, quint32 version) {
            m_shell = registry->createShell(name, version, this);
        }
    );
    connect(registry, &Registry::shmAnnounced, this,
        [this, registry](quint32 name, quint32 version) {
            m_shm = registry->createShmPool(name, version, this);
        }
    );
    connect(registry, &Registry::seatAnnounced, this,
        [this, registry](quint32 name, quint32 version) {
            m_seat = registry->createSeat(name, version, this);
        }
    );
    connect(registry, &Registry::interfacesAnnounced, this,
        [this, registry] {
            Q_ASSERT(m_compositor);
            Q_ASSERT(m_seat);
            Q_ASSERT(m_shell);
            Q_ASSERT(m_shm);
            m_surface = m_compositor->createSurface(this);
            Q_ASSERT(m_surface);
            m_shellSurface = m_shell->createSurface(m_surface, this);
            Q_ASSERT(m_shellSurface);
            m_shellSurface->setToplevel();
            connect(m_shellSurface, &ShellSurface::sizeChanged, this, &SubSurfaceTest::render);

            auto subInterface = registry->interface(Registry::Interface::SubCompositor);
            if (subInterface.name != 0) {
                m_subCompositor = registry->createSubCompositor(subInterface.name, subInterface.version, this);
                Q_ASSERT(m_subCompositor);
                // create the sub surface
                auto surface = m_compositor->createSurface(this);
                Q_ASSERT(surface);
                auto subsurface = m_subCompositor->createSubSurface(surface, m_surface, this);
                Q_ASSERT(subsurface);
                QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::red);
                surface->attachBuffer(m_shm->createBuffer(image));
                surface->damage(QRect(0, 0, 100, 100));
                surface->commit(Surface::CommitFlag::None);
                // and another sub-surface to the sub-surface
                auto surface2 = m_compositor->createSurface(this);
                Q_ASSERT(surface2);
                auto subsurface2 = m_subCompositor->createSubSurface(surface2, surface, this);
                Q_ASSERT(subsurface2);
                QImage green(QSize(50, 50), QImage::Format_ARGB32_Premultiplied);
                green.fill(Qt::green);
                surface2->attachBuffer(m_shm->createBuffer(green));
                surface2->damage(QRect(0, 0, 50, 50));
                surface2->commit(Surface::CommitFlag::None);
                QTimer *timer = new QTimer(this);
                connect(timer, &QTimer::timeout, surface2,
                    [surface2, this] {
                        QImage yellow(QSize(50, 50), QImage::Format_ARGB32_Premultiplied);
                        yellow.fill(Qt::yellow);
                        surface2->attachBuffer(m_shm->createBuffer(yellow));
                        surface2->damage(QRect(0, 0, 50, 50));
                        surface2->commit(Surface::CommitFlag::None);
                        m_surface->commit(Surface::CommitFlag::None);
                    }
                );
                timer->setSingleShot(true);
                timer->start(5000);
            }
            render();
        }
    );
    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void SubSurfaceTest::render()
{
    const QSize &size = m_shellSurface->size().isValid() ? m_shellSurface->size() : QSize(200, 200);
    auto buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
    buffer->setUsed(true);
    QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::blue);

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), size));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    SubSurfaceTest client;
    client.init();

    return app.exec();
}

#include "subsurfacetest.moc"
