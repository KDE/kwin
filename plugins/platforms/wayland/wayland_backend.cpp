/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_backend.h"

#if HAVE_WAYLAND_EGL
#include "egl_wayland_backend.h"
#endif
#include "logging.h"
#include "scene_qpainter_wayland_backend.h"
#include "wayland_output.h"

#include "composite.h"
#include "cursor.h"
#include "input.h"
#include "main.h"
#include "outputscreens.h"
#include "pointer_input.h"
#include "screens.h"
#include "wayland_server.h"
#include "../drm/gbm_dmabuf.h"

#include <config-kwin.h>

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/pointergestures.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/relativepointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/subcompositor.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>
#include <KWayland/Client/xdgshell.h>

#include <KWaylandServer/seat_interface.h>

#include <QMetaMethod>
#include <QThread>

#include <linux/input.h>
#include <unistd.h>
#include <gbm.h>
#include <fcntl.h>

namespace KWin
{
namespace Wayland
{

using namespace KWayland::Client;

WaylandCursor::WaylandCursor(WaylandBackend *backend)
    : QObject(backend)
    , m_backend(backend)
{
    resetSurface();
}

void WaylandCursor::resetSurface()
{
    delete m_surface;
    m_surface = backend()->compositor()->createSurface(this);
}

void WaylandCursor::init()
{
    installImage();
}

WaylandCursor::~WaylandCursor()
{
    delete m_surface;
}

void WaylandCursor::installImage()
{
    const QImage image = Cursors::self()->currentCursor()->image();
    if (image.isNull() || image.size().isEmpty()) {
        doInstallImage(nullptr, QSize());
        return;
    }

    auto buffer = m_backend->shmPool()->createBuffer(image).toStrongRef();
    wl_buffer *imageBuffer = *buffer.data();
    doInstallImage(imageBuffer, image.size());
}

void WaylandCursor::doInstallImage(wl_buffer *image, const QSize &size)
{
    auto *pointer = m_backend->seat()->pointer();
    if (!pointer || !pointer->isValid()) {
        return;
    }
    pointer->setCursor(m_surface, image ? Cursors::self()->currentCursor()->hotspot() : QPoint());
    drawSurface(image, size);
}

void WaylandCursor::drawSurface(wl_buffer *image, const QSize &size)
{
    m_surface->attachBuffer(image);
    m_surface->damage(QRect(QPoint(0,0), size));
    m_surface->commit(Surface::CommitFlag::None);
    m_backend->flush();
}

WaylandSubSurfaceCursor::WaylandSubSurfaceCursor(WaylandBackend *backend)
    : WaylandCursor(backend)
{
}

void WaylandSubSurfaceCursor::init()
{
    if (auto *pointer = backend()->seat()->pointer()) {
        pointer->hideCursor();
    }
}

WaylandSubSurfaceCursor::~WaylandSubSurfaceCursor()
{
    delete m_subSurface;
}

void WaylandSubSurfaceCursor::changeOutput(WaylandOutput *output)
{
    delete m_subSurface;
    m_subSurface = nullptr;
    m_output = output;
    if (!output) {
        return;
    }
    createSubSurface();
    surface()->commit();
}

void WaylandSubSurfaceCursor::createSubSurface()
{
    if (m_subSurface) {
        return;
    }
    if (!m_output) {
        return;
    }
    resetSurface();
    m_subSurface = backend()->subCompositor()->createSubSurface(surface(), m_output->surface(), this);
    m_subSurface->setMode(SubSurface::Mode::Desynchronized);
}

void WaylandSubSurfaceCursor::doInstallImage(wl_buffer *image, const QSize &size)
{
    if (!image) {
        delete m_subSurface;
        m_subSurface = nullptr;
        return;
    }
    createSubSurface();
    // cursor position might have changed due to different cursor hot spot
    move(input()->pointer()->pos());
    drawSurface(image, size);
}

QPointF WaylandSubSurfaceCursor::absoluteToRelativePosition(const QPointF &position)
{
    return position - m_output->geometry().topLeft() - Cursors::self()->currentCursor()->hotspot();
}

void WaylandSubSurfaceCursor::move(const QPointF &globalPosition)
{
    auto *output = backend()->getOutputAt(globalPosition.toPoint());
    if (!m_output || (output && m_output != output)) {
        changeOutput(output);
        if (!m_output) {
            // cursor might be off the grid
            return;
        }
        installImage();
        return;
    }
    if (!m_subSurface) {
        return;
    }
    // place the sub-surface relative to the output it is on and factor in the hotspot
    const auto relativePosition = globalPosition.toPoint() - Cursors::self()->currentCursor()->hotspot() - m_output->geometry().topLeft();
    m_subSurface->setPosition(relativePosition);
    Compositor::self()->addRepaintFull();
}

WaylandSeat::WaylandSeat(wl_seat *seat, WaylandBackend *backend)
    : QObject(nullptr)
    , m_seat(new Seat(this))
    , m_pointer(nullptr)
    , m_keyboard(nullptr)
    , m_touch(nullptr)
    , m_enteredSerial(0)
    , m_backend(backend)
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
                            if (key == KEY_RIGHTCTRL) {
                                m_backend->togglePointerLock();
                            }
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
                setupPointerGestures();
                connect(m_pointer, &Pointer::entered, this,
                    [this](quint32 serial, const QPointF &relativeToSurface) {
                        Q_UNUSED(relativeToSurface)
                        m_enteredSerial = serial;
                    }
                );
                connect(m_pointer, &Pointer::motion, this,
                    [this](const QPointF &relativeToSurface, quint32 time) {
                        m_backend->pointerMotionRelativeToOutput(relativeToSurface, time);
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
                // TODO: Send discreteDelta and source as well.
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
                connect(m_touch, &Touch::sequenceCanceled, m_backend, &Platform::touchCancel);
                connect(m_touch, &Touch::frameEnded, m_backend, &Platform::touchFrame);
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
        using namespace KWaylandServer;
        SeatInterface *si = server->seat();
        connect(m_seat, &Seat::hasKeyboardChanged, si, &SeatInterface::setHasKeyboard);
        connect(m_seat, &Seat::hasPointerChanged, si, &SeatInterface::setHasPointer);
        connect(m_seat, &Seat::hasTouchChanged, si, &SeatInterface::setHasTouch);
        connect(m_seat, &Seat::nameChanged, si, &SeatInterface::setName);
    }
}

void WaylandBackend::pointerMotionRelativeToOutput(const QPointF &position, quint32 time)
{
    auto outputIt = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(), [this](WaylandOutput *wo) {
            return wo->surface() == m_seat->pointer()->enteredSurface();
    });
    Q_ASSERT(outputIt != m_outputs.constEnd());
    const QPointF outputPosition = (*outputIt)->geometry().topLeft() + position;
    Platform::pointerMotion(outputPosition, time);
}

WaylandSeat::~WaylandSeat()
{
    destroyPointer();
    destroyKeyboard();
    destroyTouch();
}

void WaylandSeat::destroyPointer()
{
    delete m_pinchGesture;
    m_pinchGesture = nullptr;
    delete m_swipeGesture;
    m_swipeGesture = nullptr;
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

void WaylandSeat::setupPointerGestures()
{
    if (!m_pointer || !m_gesturesInterface) {
        return;
    }
    if (m_pinchGesture || m_swipeGesture) {
        return;
    }
    m_pinchGesture = m_gesturesInterface->createPinchGesture(m_pointer, this);
    m_swipeGesture = m_gesturesInterface->createSwipeGesture(m_pointer, this);
    connect(m_pinchGesture, &PointerPinchGesture::started, m_backend,
        [this] (quint32 serial, quint32 time) {
            Q_UNUSED(serial);
            m_backend->processPinchGestureBegin(m_pinchGesture->fingerCount(), time);
        }
    );
    connect(m_pinchGesture, &PointerPinchGesture::updated, m_backend,
        [this] (const QSizeF &delta, qreal scale, qreal rotation, quint32 time) {
            m_backend->processPinchGestureUpdate(scale, rotation, delta, time);
        }
    );
    connect(m_pinchGesture, &PointerPinchGesture::ended, m_backend,
        [this] (quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            m_backend->processPinchGestureEnd(time);
        }
    );
    connect(m_pinchGesture, &PointerPinchGesture::cancelled, m_backend,
        [this] (quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            m_backend->processPinchGestureCancelled(time);
        }
    );

    connect(m_swipeGesture, &PointerSwipeGesture::started, m_backend,
        [this] (quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            m_backend->processSwipeGestureBegin(m_swipeGesture->fingerCount(), time);
        }
    );
    connect(m_swipeGesture, &PointerSwipeGesture::updated, m_backend, &Platform::processSwipeGestureUpdate);
    connect(m_swipeGesture, &PointerSwipeGesture::ended, m_backend,
        [this] (quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            m_backend->processSwipeGestureEnd(time);
        }
    );
    connect(m_swipeGesture, &PointerSwipeGesture::cancelled, m_backend,
        [this] (quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            m_backend->processSwipeGestureCancelled(time);
        }
    );
}

WaylandBackend::WaylandBackend(QObject *parent)
    : Platform(parent)
    , m_display(nullptr)
    , m_eventQueue(new EventQueue(this))
    , m_registry(new Registry(this))
    , m_compositor(new KWayland::Client::Compositor(this))
    , m_subCompositor(new KWayland::Client::SubCompositor(this))
    , m_shm(new ShmPool(this))
    , m_connectionThreadObject(new ConnectionThread(nullptr))
    , m_connectionThread(nullptr)
{
    supportsOutputChanges();
    connect(this, &WaylandBackend::connectionFailed, this, &WaylandBackend::initFailed);


    char const *drm_render_node = "/dev/dri/renderD128";
    m_drmFileDescriptor = open(drm_render_node, O_RDWR);
    if (m_drmFileDescriptor < 0) {
        qCWarning(KWIN_WAYLAND_BACKEND) << "Failed to open drm render node" << drm_render_node;
        m_gbmDevice = nullptr;
        return;
    }
    m_gbmDevice = gbm_create_device(m_drmFileDescriptor);
}

WaylandBackend::~WaylandBackend()
{
    if (m_pointerConstraints) {
        m_pointerConstraints->release();
    }
    delete m_waylandCursor;

    m_eventQueue->release();
    qDeleteAll(m_outputs);

    if (m_xdgShell) {
        m_xdgShell->release();
    }
    m_subCompositor->release();
    m_compositor->release();
    m_registry->release();
    delete m_seat;
    m_shm->release();

    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
    gbm_device_destroy(m_gbmDevice);
    close(m_drmFileDescriptor);

    qCDebug(KWIN_WAYLAND_BACKEND) << "Destroyed Wayland display";
}

void WaylandBackend::init()
{
    connect(m_registry, &Registry::compositorAnnounced, this,
        [this](quint32 name) {
            m_compositor->setup(m_registry->bindCompositor(name, 1));
        }
    );
    connect(m_registry, &Registry::subCompositorAnnounced, this,
        [this](quint32 name) {
            m_subCompositor->setup(m_registry->bindSubCompositor(name, 1));
        }
    );
    connect(m_registry, &Registry::seatAnnounced, this,
        [this](quint32 name) {
            if (Application::usesLibinput()) {
                return;
            }
            m_seat = new WaylandSeat(m_registry->bindSeat(name, 2), this);
        }
    );
    connect(m_registry, &Registry::shmAnnounced, this,
        [this](quint32 name) {
            m_shm->setup(m_registry->bindShm(name, 1));
        }
    );
    connect(m_registry, &Registry::relativePointerManagerUnstableV1Announced, this,
        [this](quint32 name, quint32 version) {
            if (m_relativePointerManager) {
                return;
            }
            m_relativePointerManager = m_registry->createRelativePointerManager(name, version, this);
            if (m_pointerConstraints) {
                emit pointerLockSupportedChanged();
            }
        }
    );
    connect(m_registry, &Registry::pointerConstraintsUnstableV1Announced, this,
        [this](quint32 name, quint32 version) {
            if (m_pointerConstraints) {
                return;
            }
            m_pointerConstraints = m_registry->createPointerConstraints(name, version, this);
            if (m_relativePointerManager) {
                emit pointerLockSupportedChanged();
            }
        }
    );
    connect(m_registry, &Registry::interfacesAnnounced, this, &WaylandBackend::createOutputs);
    connect(m_registry, &Registry::interfacesAnnounced, this,
        [this] {
            if (!m_seat) {
                return;
            }
            const auto gi = m_registry->interface(Registry::Interface::PointerGesturesUnstableV1);
            if (gi.name == 0) {
                return;
            }
            auto gesturesInterface = m_registry->createPointerGestures(gi.name, gi.version, m_seat);
            m_seat->installGesturesInterface(gesturesInterface);

            m_waylandCursor = new WaylandCursor(this);
        }
    );
    if (!deviceIdentifier().isEmpty()) {
        m_connectionThreadObject->setSocketName(deviceIdentifier());
    }
    connect(Cursors::self(), &Cursors::currentCursorChanged, this,
        [this] {
            if (!m_seat) {
                return;
            }
            m_waylandCursor->installImage();
            auto c = Cursors::self()->currentCursor();
            c->rendered(c->geometry());
        }
    );
    connect(this, &WaylandBackend::pointerLockChanged, this, [this] (bool locked) {
        delete m_waylandCursor;
        if (locked) {
            Q_ASSERT(!m_relativePointer);
            m_waylandCursor = new WaylandSubSurfaceCursor(this);
            m_waylandCursor->move(input()->pointer()->pos());
            m_relativePointer = m_relativePointerManager->createRelativePointer(m_seat->pointer(), this);
            if (!m_relativePointer->isValid()) {
                return;
            }
            connect(m_relativePointer, &RelativePointer::relativeMotion,
                    this, &WaylandBackend::relativeMotionHandler);
        } else {
            delete m_relativePointer;
            m_relativePointer = nullptr;
            m_waylandCursor = new WaylandCursor(this);
        }
        m_waylandCursor->init();
    });
    initConnection();
}

void WaylandBackend::relativeMotionHandler(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 timestamp)
{
    Q_UNUSED(deltaNonAccelerated)
    Q_ASSERT(m_waylandCursor);

    const auto oldGlobalPos = input()->pointer()->pos();
    const QPointF newPos = oldGlobalPos + QPointF(delta.width(), delta.height());
    m_waylandCursor->move(newPos);
    Platform::pointerMotion(newPos, timestamp);
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
            delete m_seat;
            m_shm->destroy();

            qDeleteAll(m_outputs);
            m_outputs.clear();

            if (m_xdgShell) {
                m_xdgShell->destroy();
            }
            m_subCompositor->destroy();
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

void WaylandBackend::updateScreenSize(WaylandOutput *output)
{
   auto it = std::find(m_outputs.constBegin(), m_outputs.constEnd(), output);

   int nextLogicalPosition = output->geometry().topRight().x();
   while (++it != m_outputs.constEnd()) {
       const QRect geo = (*it)->geometry();
       (*it)->setGeometry(QPoint(nextLogicalPosition, 0), geo.size());
       nextLogicalPosition = geo.topRight().x();
   }
}

void WaylandBackend::createOutputs()
{
    using namespace KWayland::Client;

    const auto ssdManagerIface = m_registry->interface(Registry::Interface::ServerSideDecorationManager);
    ServerSideDecorationManager *ssdManager = ssdManagerIface.name == 0 ? nullptr :
            m_registry->createServerSideDecorationManager(ssdManagerIface.name, ssdManagerIface.version, this);


    const auto xdgIface = m_registry->interface(Registry::Interface::XdgShellStable);
    if (xdgIface.name != 0) {
        m_xdgShell = m_registry->createXdgShell(xdgIface.name, xdgIface.version, this);
    }

    // we need to multiply the initial window size with the scale in order to
    // create an output window of this size in the end
    const int pixelWidth = initialWindowSize().width() * initialOutputScale() + 0.5;
    const int pixelHeight = initialWindowSize().height() * initialOutputScale() + 0.5;
    const int logicalWidth = initialWindowSize().width();

    int logicalWidthSum = 0;
    for (int i = 0; i < initialOutputCount(); i++) {
        auto surface = m_compositor->createSurface(this);
        if (!surface || !surface->isValid()) {
            qCCritical(KWIN_WAYLAND_BACKEND) << "Creating Wayland Surface failed";
            return;
        }

        if (ssdManager) {
            auto decoration = ssdManager->create(surface, this);
            connect(decoration, &ServerSideDecoration::modeChanged, this,
                [decoration] {
                    if (decoration->mode() != ServerSideDecoration::Mode::Server) {
                        decoration->requestMode(ServerSideDecoration::Mode::Server);
                    }
                }
            );
        }

        WaylandOutput *waylandOutput = nullptr;

        if (m_xdgShell && m_xdgShell->isValid()) {
            waylandOutput = new XdgShellOutput(surface, m_xdgShell, this, i+1);
        }

        if (!waylandOutput) {
            qCCritical(KWIN_WAYLAND_BACKEND) << "Binding to all shell interfaces failed for output" << i;
            return;
        }

        waylandOutput->init(QPoint(logicalWidthSum, 0), QSize(pixelWidth, pixelHeight));

        connect(waylandOutput, &WaylandOutput::sizeChanged, this, [this, waylandOutput](const QSize &size) {
            Q_UNUSED(size)
            updateScreenSize(waylandOutput);
            Compositor::self()->addRepaintFull();
        });
        connect(waylandOutput, &WaylandOutput::frameRendered, this, &WaylandBackend::checkBufferSwap);

        logicalWidthSum += logicalWidth;
        m_outputs << waylandOutput;
    }
    setReady(true);
    emit screensQueried();
}

Screens *WaylandBackend::createScreens(QObject *parent)
{
    return new OutputScreens(this, parent);
}

OpenGLBackend *WaylandBackend::createOpenGLBackend()
{
#if HAVE_WAYLAND_EGL
    return new EglWaylandBackend(this);
#else
    return nullptr;
#endif
}

QPainterBackend *WaylandBackend::createQPainterBackend()
{
    return new WaylandQPainterBackend(this);
}

void WaylandBackend::checkBufferSwap()
{
    const bool allRendered = std::all_of(m_outputs.constBegin(), m_outputs.constEnd(), [](WaylandOutput *o) {
            return o->rendered();
        });
    if (!allRendered) {
        // need to wait more
        // TODO: what if one does not need to be rendered (no damage)?
        return;
    }
    Compositor::self()->bufferSwapComplete();

    for (auto *output : qAsConst(m_outputs)) {
        output->resetRendered();
    }
}

void WaylandBackend::flush()
{
    if (m_connectionThreadObject) {
        m_connectionThreadObject->flush();
    }
}

WaylandOutput* WaylandBackend::getOutputAt(const QPointF &globalPosition)
{
    const auto pos = globalPosition.toPoint();
    auto checkPosition = [pos](WaylandOutput *output) {
        return output->geometry().contains(pos);
    };
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(), checkPosition);
    return it == m_outputs.constEnd() ? nullptr : *it;
}

bool WaylandBackend::supportsPointerLock()
{
    return m_pointerConstraints && m_relativePointerManager;
}

void WaylandBackend::togglePointerLock()
{
    if (!m_pointerConstraints) {
        return;
    }
    if (!m_relativePointerManager) {
        return;
    }
    if (!m_seat) {
        return;
    }
    auto pointer = m_seat->pointer();
    if (!pointer) {
        return;
    }
    if (m_outputs.isEmpty()) {
        return;
    }

    for (auto output : m_outputs) {
        output->lockPointer(m_seat->pointer(), !m_pointerLockRequested);
    }
    m_pointerLockRequested = !m_pointerLockRequested;
    flush();
}

bool WaylandBackend::pointerIsLocked()
{
    for (auto *output : m_outputs) {
        if (output->pointerIsLocked()) {
            return true;
        }
    }
    return false;
}

QVector<CompositingType> WaylandBackend::supportedCompositors() const
{
    if (selectedCompositor() != NoCompositing) {
        return {selectedCompositor()};
    }
#if HAVE_WAYLAND_EGL
    return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
#else
    return QVector<CompositingType>{QPainterCompositing};
#endif
}

Outputs WaylandBackend::outputs() const
{
    return m_outputs;
}

Outputs WaylandBackend::enabledOutputs() const
{
    // all outputs are enabled
    return m_outputs;
}

DmaBufTexture *WaylandBackend::createDmaBufTexture(const QSize& size)
{
    return GbmDmaBuf::createBuffer(size, m_gbmDevice);
}

}

} // KWin
