/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/pointer.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shadow.h"
#include "KWayland/Client/shell.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/xdgshell.h"
// Qt
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QThread>

using namespace KWayland::Client;

class XdgTest : public QObject
{
    Q_OBJECT
public:
    explicit XdgTest(QObject *parent = nullptr);
    virtual ~XdgTest();

    void init();

private:
    void setupRegistry(Registry *registry);
    void createPopup();
    void render();
    void renderPopup();
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    EventQueue *m_eventQueue = nullptr;
    Compositor *m_compositor = nullptr;
    ShmPool *m_shm = nullptr;
    Surface *m_surface = nullptr;
    XdgShell *m_xdgShell = nullptr;
    XdgShellSurface *m_xdgShellSurface = nullptr;
    Surface *m_popupSurface = nullptr;
    XdgShellPopup *m_xdgShellPopup = nullptr;
};

XdgTest::XdgTest(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

XdgTest::~XdgTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void XdgTest::init()
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

void XdgTest::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::compositorAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_compositor = registry->createCompositor(name, version, this);
    });
    connect(registry, &Registry::shmAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_shm = registry->createShmPool(name, version, this);
    });
    connect(registry, &Registry::xdgShellUnstableV6Announced, this, [this, registry](quint32 name, quint32 version) {
        m_xdgShell = registry->createXdgShell(name, version, this);
        m_xdgShell->setEventQueue(m_eventQueue);
    });
    connect(registry, &Registry::interfacesAnnounced, this, [this] {
        Q_ASSERT(m_compositor);
        Q_ASSERT(m_xdgShell);
        Q_ASSERT(m_shm);
        m_surface = m_compositor->createSurface(this);
        Q_ASSERT(m_surface);
        m_xdgShellSurface = m_xdgShell->createSurface(m_surface, this);
        Q_ASSERT(m_xdgShellSurface);
        connect(m_xdgShellSurface,
                &XdgShellSurface::configureRequested,
                this,
                [this](const QSize &size, KWayland::Client::XdgShellSurface::States states, int serial) {
                    m_xdgShellSurface->ackConfigure(serial);
                    render();
                });

        m_xdgShellSurface->setTitle(QStringLiteral("Test Window"));

        m_surface->commit();
    });
    connect(registry, &Registry::seatAnnounced, this, [this, registry](quint32 name) {
        Seat *s = registry->createSeat(name, 2, this);
        connect(s, &Seat::hasPointerChanged, this, [this, s](bool has) {
            if (!has) {
                return;
            }
            Pointer *p = s->createPointer(this);
            connect(p, &Pointer::buttonStateChanged, this, [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState state) {
                if (state == Pointer::ButtonState::Released) {
                    if (m_popupSurface) {
                        m_popupSurface->deleteLater();
                        m_popupSurface = nullptr;
                    } else {
                        createPopup();
                    }
                }
            });
        });
    });

    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void XdgTest::createPopup()
{
    if (m_popupSurface) {
        m_popupSurface->deleteLater();
    }

    m_popupSurface = m_compositor->createSurface(this);

    XdgPositioner positioner(QSize(200, 200), QRect(50, 50, 400, 400));
    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    positioner.setGravity(Qt::BottomEdge);
    positioner.setConstraints(XdgPositioner::Constraint::FlipX | XdgPositioner::Constraint::SlideY);
    m_xdgShellPopup = m_xdgShell->createPopup(m_popupSurface, m_xdgShellSurface, positioner, m_popupSurface);
    renderPopup();
}

void XdgTest::render()
{
    const QSize &size = m_xdgShellSurface->size().isValid() ? m_xdgShellSurface->size() : QSize(500, 500);
    auto buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
    buffer->setUsed(true);
    QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 255, 255, 255));
    // draw a red rectangle indicating the anchor of the top level
    QPainter painter(&image);
    painter.setBrush(Qt::red);
    painter.setPen(Qt::black);
    painter.drawRect(50, 50, 400, 400);

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), size));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

void XdgTest::renderPopup()
{
    QSize size(200, 200);
    auto buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
    buffer->setUsed(true);
    QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(0, 0, 255, 255));

    m_popupSurface->attachBuffer(*buffer);
    m_popupSurface->damage(QRect(QPoint(0, 0), size));
    m_popupSurface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    XdgTest client;
    client.init();

    return app.exec();
}

#include "xdgtest.moc"
