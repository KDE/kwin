/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// own
#include "wayland_backend.h"
// KWin
#include "cursor.h"
#include "logging.h"
#include "main.h"
#include "scene_qpainter_wayland_backend.h"
#include "screens_wayland.h"
#include "wayland_server.h"
#include "wayland_cursor_theme.h"
#include "egl_wayland_backend.h"
#include <KWayland/Client/buffer.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>
// Qt
#include <QMetaMethod>
#include <QThread>
// Wayland
#include <wayland-cursor.h>

namespace KWin
{
namespace Wayland
{

using namespace KWayland::Client;

WaylandSeat::WaylandSeat(wl_seat *seat, WaylandBackend *backend)
    : QObject(NULL)
    , m_seat(new Seat(this))
    , m_pointer(NULL)
    , m_keyboard(NULL)
    , m_touch(nullptr)
    , m_cursor(NULL)
    , m_theme(new WaylandCursorTheme(backend->shmPool(), this))
    , m_enteredSerial(0)
    , m_backend(backend)
    , m_installCursor(false)
{
    m_seat->setup(seat);
    connect(m_seat, &Seat::hasKeyboardChanged, this,
        [this](bool hasKeyboard) {
            if (hasKeyboard) {
                m_keyboard = m_seat->createKeyboard(this);
                connect(m_keyboard, &Keyboard::keyChanged, this,
                    [this](quint32 key, Keyboard::KeyState state, quint32 time) {
                        switch (state) {
                        case Keyboard::KeyState::Pressed:
                            m_backend->keyboardKeyPressed(key, time);
                            break;
                        case Keyboard::KeyState::Released:
                            m_backend->keyboardKeyReleased(key, time);
                            break;
                        default:
                            Q_UNREACHABLE();
                        }
                    }
                );
                connect(m_keyboard, &Keyboard::modifiersChanged, this,
                    [this](quint32 depressed, quint32 latched, quint32 locked, quint32 group) {
                        m_backend->keyboardModifiers(depressed, latched, locked, group);
                    }
                );
                connect(m_keyboard, &Keyboard::keymapChanged, this,
                    [this](int fd, quint32 size) {
                        m_backend->keymapChange(fd, size);
                    }
                );
            } else {
                destroyKeyboard();
            }
        }
    );
    connect(m_seat, &Seat::hasPointerChanged, this,
        [this](bool hasPointer) {
            if (hasPointer && !m_pointer) {
                m_pointer = m_seat->createPointer(this);
                connect(m_pointer, &Pointer::entered, this,
                    [this](quint32 serial) {
                        m_enteredSerial = serial;
                        if (!m_installCursor) {
                            // explicitly hide cursor
                            m_pointer->hideCursor();
                        }
                    }
                );
                connect(m_pointer, &Pointer::motion, this,
                    [this](const QPointF &relativeToSurface, quint32 time) {
                        m_backend->pointerMotion(relativeToSurface, time);
                    }
                );
                connect(m_pointer, &Pointer::buttonStateChanged, this,
                    [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState state) {
                        Q_UNUSED(serial)
                        switch (state) {
                        case Pointer::ButtonState::Pressed:
                            m_backend->pointerButtonPressed(button, time);
                            break;
                        case Pointer::ButtonState::Released:
                            m_backend->pointerButtonReleased(button, time);
                            break;
                        default:
                            Q_UNREACHABLE();
                        }
                    }
                );
                connect(m_pointer, &Pointer::axisChanged, this,
                    [this](quint32 time, Pointer::Axis axis, qreal delta) {
                        switch (axis) {
                        case Pointer::Axis::Horizontal:
                            m_backend->pointerAxisHorizontal(delta, time);
                            break;
                        case Pointer::Axis::Vertical:
                            m_backend->pointerAxisVertical(delta, time);
                            break;
                        default:
                            Q_UNREACHABLE();
                        }
                    }
                );
            } else {
                destroyPointer();
            }
        }
    );
    connect(m_seat, &Seat::hasTouchChanged,
        [this] (bool hasTouch) {
            if (hasTouch && !m_touch) {
                m_touch = m_seat->createTouch(this);
                connect(m_touch, &Touch::sequenceCanceled, m_backend, &AbstractBackend::touchCancel);
                connect(m_touch, &Touch::frameEnded, m_backend, &AbstractBackend::touchFrame);
                connect(m_touch, &Touch::sequenceStarted, this,
                    [this] (TouchPoint *tp) {
                        m_backend->touchDown(tp->id(), tp->position(), tp->time());
                    }
                );
                connect(m_touch, &Touch::pointAdded, this,
                    [this] (TouchPoint *tp) {
                        m_backend->touchDown(tp->id(), tp->position(), tp->time());
                    }
                );
                connect(m_touch, &Touch::pointRemoved, this,
                    [this] (TouchPoint *tp) {
                        m_backend->touchUp(tp->id(), tp->time());
                    }
                );
                connect(m_touch, &Touch::pointMoved, this,
                    [this] (TouchPoint *tp) {
                        m_backend->touchMotion(tp->id(), tp->position(), tp->time());
                    }
                );
            } else {
                destroyTouch();
            }
        }
    );
    WaylandServer *server = waylandServer();
    if (server) {
        using namespace KWayland::Server;
        SeatInterface *si = server->seat();
        connect(m_seat, &Seat::hasKeyboardChanged, si, &SeatInterface::setHasKeyboard);
        connect(m_seat, &Seat::hasPointerChanged, si, &SeatInterface::setHasPointer);
        connect(m_seat, &Seat::hasTouchChanged, si, &SeatInterface::setHasTouch);
        connect(m_seat, &Seat::nameChanged, si, &SeatInterface::setName);
    }
}

WaylandSeat::~WaylandSeat()
{
    destroyPointer();
    destroyKeyboard();
    destroyTouch();
}

void WaylandSeat::destroyPointer()
{
    delete m_pointer;
    m_pointer = nullptr;
}

void WaylandSeat::destroyKeyboard()
{
    delete m_keyboard;
    m_keyboard = nullptr;
}

void WaylandSeat::destroyTouch()
{
    delete m_touch;
    m_touch = nullptr;
}

void WaylandSeat::installCursorImage(wl_buffer *image, const QSize &size, const QPoint &hotSpot)
{
    if (!m_installCursor) {
        return;
    }
    if (!m_pointer || !m_pointer->isValid()) {
        return;
    }
    if (!m_cursor) {
        m_cursor = m_backend->compositor()->createSurface(this);
    }
    if (!m_cursor || !m_cursor->isValid()) {
        return;
    }
    m_pointer->setCursor(m_cursor, hotSpot);
    m_cursor->attachBuffer(image);
    m_cursor->damage(QRect(QPoint(0,0), size));
    m_cursor->commit(Surface::CommitFlag::None);
}

void WaylandSeat::installCursorImage(Qt::CursorShape shape)
{
    wl_cursor_image *image = m_theme->get(shape);
    if (!image) {
        return;
    }
    installCursorImage(wl_cursor_image_get_buffer(image),
                       QSize(image->width, image->height),
                       QPoint(image->hotspot_x, image->hotspot_y));
}

void WaylandSeat::installCursorImage(const QImage &image, const QPoint &hotSpot)
{
    installCursorImage(*(m_backend->shmPool()->createBuffer(image).data()), image.size(), hotSpot);
}

void WaylandSeat::setInstallCursor(bool install)
{
    // TODO: remove, add?
    m_installCursor = install;
}

WaylandBackend::WaylandBackend(QObject *parent)
    : AbstractBackend(parent)
    , m_display(nullptr)
    , m_eventQueue(new EventQueue(this))
    , m_registry(new Registry(this))
    , m_compositor(new Compositor(this))
    , m_shell(new Shell(this))
    , m_surface(nullptr)
    , m_shellSurface(NULL)
    , m_seat()
    , m_shm(new ShmPool(this))
    , m_connectionThreadObject(new ConnectionThread(nullptr))
    , m_connectionThread(nullptr)
{
    connect(this, &WaylandBackend::outputsChanged, this, &WaylandBackend::screensQueried);
    connect(this, &WaylandBackend::connectionFailed, this, &WaylandBackend::initFailed);
}

WaylandBackend::~WaylandBackend()
{
    destroyOutputs();
    if (m_shellSurface) {
        m_shellSurface->release();
    }
    if (m_surface) {
        m_surface->release();
    }
    m_shell->release();
    m_compositor->release();
    m_registry->release();
    m_seat.reset();
    m_shm->release();
    m_eventQueue->release();

    m_connectionThreadObject->deleteLater();
    m_connectionThread->quit();
    m_connectionThread->wait();

    qCDebug(KWIN_WAYLAND_BACKEND) << "Destroyed Wayland display";
}

void WaylandBackend::init()
{
    connect(m_registry, &Registry::compositorAnnounced, this,
        [this](quint32 name) {
            m_compositor->setup(m_registry->bindCompositor(name, 1));
        }
    );
    connect(m_registry, &Registry::shellAnnounced, this,
        [this](quint32 name) {
            m_shell->setup(m_registry->bindShell(name, 1));
        }
    );
    connect(m_registry, &Registry::outputAnnounced, this,
        [this](quint32 name) {
            Output *output = new Output(this);
            output->setup(m_registry->bindOutput(name, 2));
            m_outputs.append(output);
            connect(output, &Output::changed, this, &WaylandBackend::outputsChanged);
        }
    );
    connect(m_registry, &Registry::seatAnnounced, this,
        [this](quint32 name) {
            if (Application::usesLibinput()) {
                return;
            }
            m_seat.reset(new WaylandSeat(m_registry->bindSeat(name, 2), this));
        }
    );
    connect(m_registry, &Registry::shmAnnounced, this,
        [this](quint32 name) {
            m_shm->setup(m_registry->bindShm(name, 1));
        }
    );
    connect(m_registry, &Registry::interfacesAnnounced, this, &WaylandBackend::createSurface);
    m_connectionThreadObject->setSocketName(deviceIdentifier());
    initConnection();
}

void WaylandBackend::destroyOutputs()
{
    qDeleteAll(m_outputs);
    m_outputs.clear();
}

void WaylandBackend::initConnection()
{
    connect(m_connectionThreadObject, &ConnectionThread::connected, this,
        [this]() {
            // create the event queue for the main gui thread
            m_display = m_connectionThreadObject->display();
            m_eventQueue->setup(m_connectionThreadObject);
            m_registry->setEventQueue(m_eventQueue);
            // setup registry
            m_registry->create(m_display);
            m_registry->setup();
        },
        Qt::QueuedConnection);
    connect(m_connectionThreadObject, &ConnectionThread::connectionDied, this,
        [this]() {
            setReady(false);
            emit systemCompositorDied();
            m_seat.reset();
            m_shm->destroy();
            destroyOutputs();
            if (m_shellSurface) {
                m_shellSurface->destroy();
                delete m_shellSurface;
                m_shellSurface = nullptr;
            }
            if (m_surface) {
                m_surface->destroy();
                delete m_surface;
                m_surface = nullptr;
            }
            if (m_shell) {
                m_shell->destroy();
            }
            m_compositor->destroy();
            m_registry->destroy();
            m_eventQueue->destroy();
            if (m_display) {
                m_display = nullptr;
            }
        },
        Qt::QueuedConnection);
    connect(m_connectionThreadObject, &ConnectionThread::failed, this, &WaylandBackend::connectionFailed, Qt::QueuedConnection);

    m_connectionThread = new QThread(this);
    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();

    m_connectionThreadObject->initConnection();
}

void WaylandBackend::installCursorImage(Qt::CursorShape shape)
{
    if (!m_seat.isNull() && m_seat->isInstallCursor()) {
        m_seat->installCursorImage(shape);
    }
}

void WaylandBackend::installCursorFromServer()
{
    if (!waylandServer() || !waylandServer()->seat()->focusedPointer()) {
        return;
    }
    auto c = waylandServer()->seat()->focusedPointer()->cursor();
    if (c) {
        auto cursorSurface = c->surface();
        if (!cursorSurface.isNull()) {
            auto buffer = cursorSurface.data()->buffer();
            if (buffer) {
                // set cursor
                if (!m_seat.isNull() && m_seat->isInstallCursor()) {
                    m_seat->installCursorImage(buffer->data(), c->hotspot());
                }
                return;
            }
        }
    }
    // TODO: unset cursor
}

void WaylandBackend::createSurface()
{
    m_surface = m_compositor->createSurface(this);
    if (!m_surface || !m_surface->isValid()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Creating Wayland Surface failed";
        return;
    }
    if (m_seat) {
        m_seat->setInstallCursor(true);
    }
    if (m_shell->isValid()) {
        m_shellSurface = m_shell->createSurface(m_surface, this);
        connect(m_shellSurface, &ShellSurface::sizeChanged, this, &WaylandBackend::shellSurfaceSizeChanged);
        m_shellSurface->setSize(initialWindowSize());
        m_shellSurface->setToplevel();
        setReady(true);
    }
}

QSize WaylandBackend::shellSurfaceSize() const
{
    if (m_shellSurface) {
        return m_shellSurface->size();
    }
    return QSize();
}

Screens *WaylandBackend::createScreens(QObject *parent)
{
    return new WaylandScreens(this, parent);
}

OpenGLBackend *WaylandBackend::createOpenGLBackend()
{
    return new EglWaylandBackend(this);
}

QPainterBackend *WaylandBackend::createQPainterBackend()
{
    return new WaylandQPainterBackend(this);
}

}

} // KWin
