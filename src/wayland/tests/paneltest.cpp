/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "../src/client/compositor.h"
#include "../src/client/connection_thread.h"
#include "../src/client/datadevice.h"
#include "../src/client/datadevicemanager.h"
#include "../src/client/dataoffer.h"
#include "../src/client/event_queue.h"
#include "../src/client/keyboard.h"
#include "../src/client/plasmashell.h"
#include "../src/client/plasmawindowmanagement.h"
#include "../src/client/pointer.h"
#include "../src/client/registry.h"
#include "../src/client/seat.h"
#include "../src/client/shell.h"
#include "../src/client/shm_pool.h"
#include "../src/client/surface.h"
// Qt
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
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

void PanelTest::setupRegistry(Registry *registry)
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
                [this] (bool has) {
                    if (!has) {
                        return;
                    }
                    auto p = m_seat->createPointer(this);
                    connect(p, &Pointer::buttonStateChanged, this,
                        [this] (quint32 serial, quint32 time, quint32 button, KWayland::Client::Pointer::ButtonState state) {
                            Q_UNUSED(serial)
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
                        }
                    );
                }
            );
        }
    );
    connect(registry, &Registry::plasmaShellAnnounced, this,
        [this, registry] (quint32 name, quint32 version) {
            m_plasmaShell = registry->createPlasmaShell(name, version, this);
        }
    );
    connect(registry, &Registry::plasmaWindowManagementAnnounced, this,
        [this, registry] (quint32 name, quint32 version) {
            m_windowManagement = registry->createPlasmaWindowManagement(name, version, this);
            connect(m_windowManagement, &PlasmaWindowManagement::showingDesktopChanged, this,
                [] (bool set) {
                    qDebug() << "Showing desktop changed, new state: " << set;
                }
            );
            connect(m_windowManagement, &PlasmaWindowManagement::windowCreated, this,
                [this] (PlasmaWindow *w) {
                    connect(w, &PlasmaWindow::titleChanged, this,
                        [w] {
                            qDebug() << "Window title changed to: " << w->title();
                        }
                    );
                    connect(w, &PlasmaWindow::activeChanged, this,
                        [w] {
                            qDebug() << "Window active changed: " << w->isActive();
                        }
                    );
                    connect(w, &PlasmaWindow::maximizedChanged, this,
                        [w] {
                            qDebug() << "Window maximized changed: " << w->isMaximized();
                        }
                    );
                    connect(w, &PlasmaWindow::maximizedChanged, this,
                        [w] {
                            qDebug() << "Window minimized changed: " << w->isMinimized();
                        }
                    );
                    connect(w, &PlasmaWindow::keepAboveChanged, this,
                        [w] {
                            qDebug() << "Window keep above changed: " << w->isKeepAbove();
                        }
                    );
                    connect(w, &PlasmaWindow::keepBelowChanged, this,
                        [w] {
                            qDebug() << "Window keep below changed: " << w->isKeepBelow();
                        }
                    );
                    connect(w, &PlasmaWindow::onAllDesktopsChanged, this,
                        [w] {
                            qDebug() << "Window on all desktops changed: " << w->isOnAllDesktops();
                        }
                    );
                    connect(w, &PlasmaWindow::fullscreenChanged, this,
                        [w] {
                            qDebug() << "Window full screen changed: " << w->isFullscreen();
                        }
                    );
                    connect(w, &PlasmaWindow::demandsAttentionChanged, this,
                        [w] {
                            qDebug() << "Window demands attention changed: " << w->isDemandingAttention();
                        }
                    );
                    connect(w, &PlasmaWindow::closeableChanged, this,
                        [w] {
                            qDebug() << "Window is closeable changed: " << w->isCloseable();
                        }
                    );
                    connect(w, &PlasmaWindow::minimizeableChanged, this,
                        [w] {
                            qDebug() << "Window is minimizeable changed: " << w->isMinimizeable();
                        }
                    );
                    connect(w, &PlasmaWindow::maximizeableChanged, this,
                        [w] {
                            qDebug() << "Window is maximizeable changed: " << w->isMaximizeable();
                        }
                    );
                    connect(w, &PlasmaWindow::fullscreenableChanged, this,
                        [w] {
                            qDebug() << "Window is fullscreenable changed: " << w->isFullscreenable();
                        }
                    );
                }
            );
        }
    );
    connect(registry, &Registry::interfacesAnnounced, this,
        [this] {
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
        }
    );
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
    QCoreApplication app(argc, argv);
    PanelTest client;
    client.init();

    return app.exec();
}

#include "paneltest.moc"

