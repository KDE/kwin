/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/dataoffer.h"
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
#include <QMimeType>
#include <QThread>
#include <QtConcurrentRun>
// system
#include <unistd.h>

using namespace KWayland::Client;

class PasteClient : public QObject
{
    Q_OBJECT
public:
    explicit PasteClient(QObject *parent = nullptr);
    virtual ~PasteClient();

    void init();

private:
    void setupRegistry(Registry *registry);
    void render();
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    EventQueue *m_eventQueue = nullptr;
    Compositor *m_compositor = nullptr;
    DataDeviceManager *m_dataDeviceManager = nullptr;
    DataDevice *m_dataDevice = nullptr;
    Seat *m_seat = nullptr;
    Shell *m_shell = nullptr;
    ShellSurface *m_shellSurface = nullptr;
    ShmPool *m_shm = nullptr;
    Surface *m_surface = nullptr;
};

PasteClient::PasteClient(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

PasteClient::~PasteClient()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void PasteClient::init()
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

void PasteClient::setupRegistry(Registry *registry)
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
            connect(m_shellSurface, &ShellSurface::sizeChanged, this, &PasteClient::render);

            m_dataDevice = m_dataDeviceManager->getDataDevice(m_seat, this);
            connect(m_dataDevice, &DataDevice::selectionOffered, this,
                [this] {
                    auto dataOffer = m_dataDevice->offeredSelection();
                    if (!dataOffer) {
                        return;
                    }
                    const auto &mimeTypes = dataOffer->offeredMimeTypes();
                    auto it = std::find_if(mimeTypes.constBegin(), mimeTypes.constEnd(),
                        [](const QMimeType &type) {
                            return type.inherits(QStringLiteral("text/plain"));
                        });
                    if (it == mimeTypes.constEnd()) {
                        return;
                    }
                    int pipeFds[2];
                    if (pipe(pipeFds) != 0){
                        return;
                    }
                    dataOffer->receive((*it).name(), pipeFds[1]);
                    close(pipeFds[1]);
                    QtConcurrent::run(
                        [pipeFds] {
                            QFile readPipe;
                            if (readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
                                qDebug() << "Pasted: " << readPipe.readLine();
                            }
                            close(pipeFds[0]);
                            QCoreApplication::quit();
                        }
                    );
                }
            );
        }
    );
    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void PasteClient::render()
{
    const QSize &size = m_shellSurface->size();
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
    PasteClient client;
    client.init();

    return app.exec();
}

#include "pasteclient.moc"
