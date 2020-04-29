/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/datasource.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/pointer.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shell.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
// Qt
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QThread>

using namespace KWayland::Client;

class CopyClient : public QObject
{
    Q_OBJECT
public:
    explicit CopyClient(QObject *parent = nullptr);
    virtual ~CopyClient();

    void init();

private:
    void setupRegistry(Registry *registry);
    void render();
    void copy(const QString &mimeType, qint32 fd);
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    EventQueue *m_eventQueue = nullptr;
    Compositor *m_compositor = nullptr;
    DataDeviceManager *m_dataDeviceManager = nullptr;
    DataDevice *m_dataDevice = nullptr;
    DataSource *m_copySource = nullptr;
    Seat *m_seat = nullptr;
    Shell *m_shell = nullptr;
    ShellSurface *m_shellSurface = nullptr;
    ShmPool *m_shm = nullptr;
    Surface *m_surface = nullptr;
};

CopyClient::CopyClient(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

CopyClient::~CopyClient()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void CopyClient::init()
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

void CopyClient::setupRegistry(Registry *registry)
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
            connect(m_seat, &Seat::hasPointerChanged, this,
                [this] {
                    auto p = m_seat->createPointer(this);
                    connect(p, &Pointer::entered, this,
                        [this] (quint32 serial) {
                            if (m_copySource) {
                                m_dataDevice->setSelection(serial, m_copySource);
                            }
                        }
                    );
                }
            );
        }
    );
    connect(registry, &Registry::dataDeviceManagerAnnounced, this,
        [this, registry](quint32 name, quint32 version) {
            m_dataDeviceManager = registry->createDataDeviceManager(name, version, this);
        }
    );
    connect(registry, &Registry::interfacesAnnounced, this,
        [this] {
            Q_ASSERT(m_compositor);
            Q_ASSERT(m_dataDeviceManager);
            Q_ASSERT(m_seat);
            Q_ASSERT(m_shell);
            Q_ASSERT(m_shm);
            m_surface = m_compositor->createSurface(this);
            Q_ASSERT(m_surface);
            m_shellSurface = m_shell->createSurface(m_surface, this);
            Q_ASSERT(m_shellSurface);
            m_shellSurface->setFullscreen();
            connect(m_shellSurface, &ShellSurface::sizeChanged, this, &CopyClient::render);

            m_dataDevice = m_dataDeviceManager->getDataDevice(m_seat, this);
            m_copySource = m_dataDeviceManager->createDataSource(this);
            m_copySource->offer(QStringLiteral("text/plain"));
            connect(m_copySource, &DataSource::sendDataRequested, this, &CopyClient::copy);
        }
    );
    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void CopyClient::render()
{
    const QSize &size = m_shellSurface->size();
    auto buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
    buffer->setUsed(true);
    QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::green);

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), size));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

void CopyClient::copy(const QString &mimeType, qint32 fd)
{
    qDebug() << "Requested to copy for mimeType" << mimeType;
    QFile c;
    if (c.open(fd, QFile::WriteOnly, QFile::AutoCloseHandle)) {
        c.write(QByteArrayLiteral("foo"));
        c.close();
        qDebug() << "Copied foo";
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    CopyClient client;
    client.init();

    return app.exec();
}

#include "copyclient.moc"
