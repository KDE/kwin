/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/dataoffer.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/plasmashell.h"
#include "KWayland/Client/plasmawindowmanagement.h"
#include "KWayland/Client/pointer.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shell.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
// Qt
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMimeType>
#include <QThread>
// system
#include <unistd.h>

#include <linux/input.h>

using namespace KWayland::Client;

class PanelTest : public QObject
{
    Q_OBJECT
public:
    explicit PanelTest(QObject *parent = nullptr);
    virtual ~PanelTest();

    void init();

private:
    void setupRegistry(Registry *registry);
    void render();
    void showTooltip(const QPointF &pos);
    void hideTooltip();
    void moveTooltip(const QPointF &pos);
    QThread *m_connectionThread;
    ConnectionThread *m_connectionThreadObject;
    EventQueue *m_eventQueue = nullptr;
    Compositor *m_compositor = nullptr;
    Seat *m_seat = nullptr;
    Shell *m_shell = nullptr;
    ShellSurface *m_shellSurface = nullptr;
    ShmPool *m_shm = nullptr;
    Surface *m_surface = nullptr;
    PlasmaShell *m_plasmaShell = nullptr;
    PlasmaShellSurface *m_plasmaShellSurface = nullptr;
    PlasmaWindowManagement *m_windowManagement = nullptr;
    struct
    {
        Surface *surface = nullptr;
        ShellSurface *shellSurface = nullptr;
        PlasmaShellSurface *plasmaSurface = nullptr;
        bool visible = false;
    } m_tooltip;
};

PanelTest::PanelTest(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
}

PanelTest::~PanelTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void PanelTest::init()
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

void PanelTest::showTooltip(const QPointF &pos)
{
    if (!m_tooltip.surface) {
        m_tooltip.surface = m_compositor->createSurface(this);
        m_tooltip.shellSurface = m_shell->createSurface(m_tooltip.surface, this);
        if (m_plasmaShell) {
            m_tooltip.plasmaSurface = m_plasmaShell->createSurface(m_tooltip.surface, this);
        }
    }
    m_tooltip.shellSurface->setTransient(m_surface, pos.toPoint());

    if (!m_tooltip.visible) {
        const QSize size(100, 50);
        auto buffer = m_shm->getBuffer(size, size.width() * 4).toStrongRef();
        buffer->setUsed(true);
        QImage image(buffer->address(), size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::red);

        m_tooltip.surface->attachBuffer(*buffer);
        m_tooltip.surface->damage(QRect(QPoint(0, 0), size));
        m_tooltip.surface->commit(Surface::CommitFlag::None);
        m_tooltip.visible = true;
    }
}

void PanelTest::hideTooltip()
{
    if (!m_tooltip.visible) {
        return;
    }
    m_tooltip.surface->attachBuffer(Buffer::Ptr());
    m_tooltip.surface->commit(Surface::CommitFlag::None);
    m_tooltip.visible = false;
}

void PanelTest::moveTooltip(const QPointF &pos)
{
    if (m_tooltip.plasmaSurface) {
        m_tooltip.plasmaSurface->setPosition(QPoint(10, 0) + pos.toPoint());
    }
}

void PanelTest::setupRegistry(Registry *registry)
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
    connect(registry, &Registry::seatAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_seat = registry->createSeat(name, version, this);
        connect(m_seat, &Seat::hasPointerChanged, this, [this](bool has) {
            if (!has) {
                return;
            }
            auto p = m_seat->createPointer(this);
            connect(p, &Pointer::buttonStateChanged, this, [this](quint32 serial, quint32 time, quint32 button, KWayland::Client::Pointer::ButtonState state) {
                if (!m_windowManagement) {
                    return;
                }
                if (state == Pointer::ButtonState::Released) {
                    return;
                }
                if (button == BTN_LEFT) {
                    m_windowManagement->showDesktop();
                } else if (button == BTN_RIGHT) {
                    m_windowManagement->hideDesktop();
                }
            });
            connect(p, &Pointer::entered, this, [this, p](quint32 serial, const QPointF &relativeToSurface) {
                if (p->enteredSurface() == m_surface) {
                    showTooltip(relativeToSurface);
                }
            });
            connect(p, &Pointer::motion, this, [this, p](const QPointF &relativeToSurface) {
                if (p->enteredSurface() == m_surface) {
                    moveTooltip(relativeToSurface);
                }
            });
            connect(p, &Pointer::left, this, [this] {
                hideTooltip();
            });
        });
    });
    connect(registry, &Registry::plasmaShellAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_plasmaShell = registry->createPlasmaShell(name, version, this);
    });
    connect(registry, &Registry::plasmaWindowManagementAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_windowManagement = registry->createPlasmaWindowManagement(name, version, this);
        connect(m_windowManagement, &PlasmaWindowManagement::showingDesktopChanged, this, [](bool set) {
            qDebug() << "Showing desktop changed, new state: " << set;
        });
        connect(m_windowManagement, &PlasmaWindowManagement::windowCreated, this, [this](PlasmaWindow *w) {
            connect(w, &PlasmaWindow::titleChanged, this, [w] {
                qDebug() << "Window title changed to: " << w->title();
            });
            connect(w, &PlasmaWindow::activeChanged, this, [w] {
                qDebug() << "Window active changed: " << w->isActive();
            });
            connect(w, &PlasmaWindow::maximizedChanged, this, [w] {
                qDebug() << "Window maximized changed: " << w->isMaximized();
            });
            connect(w, &PlasmaWindow::maximizedChanged, this, [w] {
                qDebug() << "Window minimized changed: " << w->isMinimized();
            });
            connect(w, &PlasmaWindow::keepAboveChanged, this, [w] {
                qDebug() << "Window keep above changed: " << w->isKeepAbove();
            });
            connect(w, &PlasmaWindow::keepBelowChanged, this, [w] {
                qDebug() << "Window keep below changed: " << w->isKeepBelow();
            });
            connect(w, &PlasmaWindow::onAllDesktopsChanged, this, [w] {
                qDebug() << "Window on all desktops changed: " << w->isOnAllDesktops();
            });
            connect(w, &PlasmaWindow::fullscreenChanged, this, [w] {
                qDebug() << "Window full screen changed: " << w->isFullscreen();
            });
            connect(w, &PlasmaWindow::demandsAttentionChanged, this, [w] {
                qDebug() << "Window demands attention changed: " << w->isDemandingAttention();
            });
            connect(w, &PlasmaWindow::closeableChanged, this, [w] {
                qDebug() << "Window is closeable changed: " << w->isCloseable();
            });
            connect(w, &PlasmaWindow::minimizeableChanged, this, [w] {
                qDebug() << "Window is minimizeable changed: " << w->isMinimizeable();
            });
            connect(w, &PlasmaWindow::maximizeableChanged, this, [w] {
                qDebug() << "Window is maximizeable changed: " << w->isMaximizeable();
            });
            connect(w, &PlasmaWindow::fullscreenableChanged, this, [w] {
                qDebug() << "Window is fullscreenable changed: " << w->isFullscreenable();
            });
            connect(w, &PlasmaWindow::iconChanged, this, [w] {
                qDebug() << "Window icon changed: " << w->icon().name();
            });
        });
    });
    connect(registry, &Registry::interfacesAnnounced, this, [this] {
        Q_ASSERT(m_compositor);
        Q_ASSERT(m_seat);
        Q_ASSERT(m_shell);
        Q_ASSERT(m_shm);
        m_surface = m_compositor->createSurface(this);
        Q_ASSERT(m_surface);
        m_shellSurface = m_shell->createSurface(m_surface, this);
        Q_ASSERT(m_shellSurface);
        m_shellSurface->setToplevel();
        connect(m_shellSurface, &ShellSurface::sizeChanged, this, &PanelTest::render);
        if (m_plasmaShell) {
            m_plasmaShellSurface = m_plasmaShell->createSurface(m_surface, this);
            m_plasmaShellSurface->setPosition(QPoint(10, 0));
            m_plasmaShellSurface->setRole(PlasmaShellSurface::Role::Panel);
        }
        render();
    });
    registry->setEventQueue(m_eventQueue);
    registry->create(m_connectionThreadObject);
    registry->setup();
}

void PanelTest::render()
{
    const QSize &size = m_shellSurface->size().isValid() ? m_shellSurface->size() : QSize(300, 20);
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
    QGuiApplication app(argc, argv);
    PanelTest client;
    client.init();

    return app.exec();
}

#include "paneltest.moc"
