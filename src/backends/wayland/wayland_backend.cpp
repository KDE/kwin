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
#include "../drm/gbm_dmabuf.h"
#include <gbm.h>
#endif
#include "logging.h"
#include "renderloop_p.h"
#include "scene_qpainter_wayland_backend.h"
#include "session.h"
#include "wayland_output.h"

#include "composite.h"
#include "cursor.h"
#include "input.h"
#include "main.h"
#include "scene.h"
#include "screens.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "dpmsinputeventfilter.h"

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

#include <QMetaMethod>
#include <QThread>

#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>

#include <cmath>

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
        doInstallImage(nullptr, QSize(), 1);
        return;
    }

    auto buffer = m_backend->shmPool()->createBuffer(image).toStrongRef();
    wl_buffer *imageBuffer = *buffer.data();
    doInstallImage(imageBuffer, image.size(), image.devicePixelRatio());
}

void WaylandCursor::doInstallImage(wl_buffer *image, const QSize &size, qreal scale)
{
    auto *pointer = m_backend->seat()->pointerDevice()->nativePointer();
    if (!pointer || !pointer->isValid()) {
        return;
    }
    pointer->setCursor(m_surface, image ? Cursors::self()->currentCursor()->hotspot() : QPoint());
    drawSurface(image, size, scale);
}

void WaylandCursor::drawSurface(wl_buffer *image, const QSize &size, qreal scale)
{
    m_surface->attachBuffer(image);
    m_surface->setScale(std::ceil(scale));
    m_surface->damageBuffer(QRect(QPoint(0, 0), size));
    m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    m_backend->flush();
}

WaylandSubSurfaceCursor::WaylandSubSurfaceCursor(WaylandBackend *backend)
    : WaylandCursor(backend)
{
}

void WaylandSubSurfaceCursor::init()
{
    if (auto *pointer = backend()->seat()->pointerDevice()->nativePointer()) {
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

void WaylandSubSurfaceCursor::doInstallImage(wl_buffer *image, const QSize &size, qreal scale)
{
    if (!image) {
        delete m_subSurface;
        m_subSurface = nullptr;
        return;
    }
    createSubSurface();
    // cursor position might have changed due to different cursor hot spot
    move(input()->pointer()->pos());
    drawSurface(image, size, scale);
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
    Compositor::self()->scene()->addRepaintFull();
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::Keyboard *keyboard, WaylandSeat *seat)
    : InputDevice(seat)
    , m_seat(seat)
    , m_keyboard(keyboard)
{
    connect(keyboard, &Keyboard::keyChanged, this, [this](quint32 key, Keyboard::KeyState nativeState, quint32 time) {
        InputRedirection::KeyboardKeyState state;
        switch (nativeState) {
        case Keyboard::KeyState::Pressed:
            if (key == KEY_RIGHTCTRL) {
                m_seat->backend()->togglePointerLock();
            }
            state = InputRedirection::KeyboardKeyPressed;
            break;
        case Keyboard::KeyState::Released:
            state = InputRedirection::KeyboardKeyReleased;
            break;
        default:
            Q_UNREACHABLE();
        }
        Q_EMIT keyChanged(key, state, time, this);
    });
    connect(keyboard, &Keyboard::modifiersChanged, this, [this](quint32 depressed, quint32 latched, quint32 locked, quint32 group) {
        m_seat->backend()->keyboardModifiers(depressed, latched, locked, group);
    });
    connect(keyboard, &Keyboard::keymapChanged, this, [this](int fd, quint32 size) {
        m_seat->backend()->keymapChange(fd, size);
    });
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::Pointer *pointer, WaylandSeat *seat)
    : InputDevice(seat)
    , m_seat(seat)
    , m_pointer(pointer)
{
    connect(pointer, &Pointer::entered, this, [this](quint32 serial, const QPointF &relativeToSurface) {
        Q_UNUSED(relativeToSurface)
        m_enteredSerial = serial;
    });
    connect(pointer, &Pointer::motion, this, [this](const QPointF &relativeToSurface, quint32 time) {
        WaylandOutput *output = m_seat->backend()->findOutput(m_pointer->enteredSurface());
        Q_ASSERT(output);
        const QPointF absolutePos = output->geometry().topLeft() + relativeToSurface;
        Q_EMIT pointerMotionAbsolute(absolutePos, time, this);
    });
    connect(pointer, &Pointer::buttonStateChanged, this, [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState nativeState) {
        Q_UNUSED(serial)
        InputRedirection::PointerButtonState state;
        switch (nativeState) {
        case Pointer::ButtonState::Pressed:
            state = InputRedirection::PointerButtonPressed;
            break;
        case Pointer::ButtonState::Released:
            state = InputRedirection::PointerButtonReleased;
            break;
        default:
            Q_UNREACHABLE();
        }
        Q_EMIT pointerButtonChanged(button, state, time, this);
    });
    // TODO: Send discreteDelta and source as well.
    connect(pointer, &Pointer::axisChanged, this, [this](quint32 time, Pointer::Axis nativeAxis, qreal delta) {
        InputRedirection::PointerAxis axis;
        switch (nativeAxis) {
        case Pointer::Axis::Horizontal:
            axis = InputRedirection::PointerAxisHorizontal;
            break;
        case Pointer::Axis::Vertical:
            axis = InputRedirection::PointerAxisVertical;
            break;
        default:
            Q_UNREACHABLE();
        }
        Q_EMIT pointerAxisChanged(axis, delta, 0, InputRedirection::PointerAxisSourceUnknown, time, this);
    });

    KWayland::Client::PointerGestures *pointerGestures = m_seat->backend()->pointerGestures();
    if (pointerGestures) {
        m_pinchGesture.reset(pointerGestures->createPinchGesture(m_pointer.data(), this));
        connect(m_pinchGesture.data(), &PointerPinchGesture::started, this, [this](quint32 serial, quint32 time) {
            Q_UNUSED(serial);
            Q_EMIT pinchGestureBegin(m_pinchGesture->fingerCount(), time, this);
        });
        connect(m_pinchGesture.data(), &PointerPinchGesture::updated, this, [this](const QSizeF &delta, qreal scale, qreal rotation, quint32 time) {
            Q_EMIT pinchGestureUpdate(scale, rotation, delta, time, this);
        });
        connect(m_pinchGesture.data(), &PointerPinchGesture::ended, this, [this](quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            Q_EMIT pinchGestureEnd(time, this);
        });
        connect(m_pinchGesture.data(), &PointerPinchGesture::cancelled, this, [this](quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            Q_EMIT pinchGestureCancelled(time, this);
        });

        m_swipeGesture.reset(pointerGestures->createSwipeGesture(m_pointer.data(), this));
        connect(m_swipeGesture.data(), &PointerSwipeGesture::started, this, [this](quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            Q_EMIT swipeGestureBegin(m_swipeGesture->fingerCount(), time, this);
        });
        connect(m_swipeGesture.data(), &PointerSwipeGesture::updated, this, [this](const QSizeF &delta, quint32 time) {
            Q_EMIT swipeGestureUpdate(delta, time, this);
        });
        connect(m_swipeGesture.data(), &PointerSwipeGesture::ended, this, [this](quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            Q_EMIT swipeGestureEnd(time, this);
        });
        connect(m_swipeGesture.data(), &PointerSwipeGesture::cancelled, this, [this](quint32 serial, quint32 time) {
            Q_UNUSED(serial)
            Q_EMIT swipeGestureCancelled(time, this);
        });
    }
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::RelativePointer *relativePointer, WaylandSeat *seat)
    : InputDevice(seat)
    , m_seat(seat)
    , m_relativePointer(relativePointer)
{
    connect(relativePointer, &RelativePointer::relativeMotion, this, [this](const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 timestamp) {
        Q_EMIT pointerMotion(delta, deltaNonAccelerated, timestamp, timestamp * 1000, this);
    });
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::Touch *touch, WaylandSeat *seat)
    : InputDevice(seat)
    , m_seat(seat)
    , m_touch(touch)
{
    connect(touch, &Touch::sequenceCanceled, this, [this]() {
        Q_EMIT touchCanceled(this);
    });
    connect(touch, &Touch::frameEnded, this, [this]() {
        Q_EMIT touchFrame(this);
    });
    connect(touch, &Touch::sequenceStarted, this, [this](TouchPoint *tp) {
        Q_EMIT touchDown(tp->id(), tp->position(), tp->time(), this);
    });
    connect(touch, &Touch::pointAdded, this, [this](TouchPoint *tp) {
        Q_EMIT touchDown(tp->id(), tp->position(), tp->time(), this);
    });
    connect(touch, &Touch::pointRemoved, this, [this](TouchPoint *tp) {
        Q_EMIT touchUp(tp->id(), tp->time(), this);
    });
    connect(touch, &Touch::pointMoved, this, [this](TouchPoint *tp) {
        Q_EMIT touchMotion(tp->id(), tp->position(), tp->time(), this);
    });
}

WaylandInputDevice::~WaylandInputDevice()
{
}

QString WaylandInputDevice::sysName() const
{
    return QString();
}

QString WaylandInputDevice::name() const
{
    return QString();
}

bool WaylandInputDevice::isEnabled() const
{
    return true;
}

void WaylandInputDevice::setEnabled(bool enabled)
{
    Q_UNUSED(enabled)
}

LEDs WaylandInputDevice::leds() const
{
    return LEDs();
}

void WaylandInputDevice::setLeds(LEDs leds)
{
    Q_UNUSED(leds)
}

bool WaylandInputDevice::isKeyboard() const
{
    return m_keyboard;
}

bool WaylandInputDevice::isAlphaNumericKeyboard() const
{
    return m_keyboard;
}

bool WaylandInputDevice::isPointer() const
{
    return m_pointer || m_relativePointer;
}

bool WaylandInputDevice::isTouchpad() const
{
    return false;
}

bool WaylandInputDevice::isTouch() const
{
    return m_touch;
}

bool WaylandInputDevice::isTabletTool() const
{
    return false;
}

bool WaylandInputDevice::isTabletPad() const
{
    return false;
}

bool WaylandInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool WaylandInputDevice::isLidSwitch() const
{
    return false;
}

KWayland::Client::Pointer *WaylandInputDevice::nativePointer() const
{
    return m_pointer.data();
}

WaylandInputBackend::WaylandInputBackend(WaylandBackend *backend, QObject *parent)
    : InputBackend(parent)
    , m_backend(backend)
{
}

void WaylandInputBackend::initialize()
{
    connect(m_backend, &WaylandBackend::seatCreated, this, &WaylandInputBackend::checkSeat);
    checkSeat();
}

void WaylandInputBackend::checkSeat()
{
    if (auto seat = m_backend->seat()) {
        if (seat->relativePointerDevice()) {
            Q_EMIT deviceAdded(seat->relativePointerDevice());
        }
        if (seat->pointerDevice()) {
            Q_EMIT deviceAdded(seat->pointerDevice());
        }
        if (seat->keyboardDevice()) {
            Q_EMIT deviceAdded(seat->keyboardDevice());
        }
        if (seat->touchDevice()) {
            Q_EMIT deviceAdded(seat->touchDevice());
        }

        connect(seat, &WaylandSeat::deviceAdded, this, &InputBackend::deviceAdded);
        connect(seat, &WaylandSeat::deviceRemoved, this, &InputBackend::deviceRemoved);
    }
}

WaylandSeat::WaylandSeat(KWayland::Client::Seat *nativeSeat, WaylandBackend *backend)
    : QObject(nullptr)
    , m_seat(nativeSeat)
    , m_backend(backend)
{
    connect(m_seat, &Seat::hasKeyboardChanged, this, [this](bool hasKeyboard) {
        if (hasKeyboard) {
            createKeyboardDevice();
        } else {
            destroyKeyboardDevice();
        }
    });
    connect(m_seat, &Seat::hasPointerChanged, this, [this](bool hasPointer) {
        if (hasPointer && !m_pointerDevice) {
            createPointerDevice();
        } else {
            destroyPointerDevice();
        }
    });
    connect(m_seat, &Seat::hasTouchChanged, this, [this](bool hasTouch) {
        if (hasTouch && !m_touchDevice) {
            createTouchDevice();
        } else {
            destroyTouchDevice();
        }
    });
}

WaylandSeat::~WaylandSeat()
{
    destroyPointerDevice();
    destroyKeyboardDevice();
    destroyTouchDevice();
}

void WaylandSeat::createPointerDevice()
{
    m_pointerDevice = new WaylandInputDevice(m_seat->createPointer(), this);
    Q_EMIT deviceAdded(m_pointerDevice);
}

void WaylandSeat::destroyPointerDevice()
{
    if (m_pointerDevice) {
        Q_EMIT deviceRemoved(m_pointerDevice);
        destroyRelativePointer();
        delete m_pointerDevice;
        m_pointerDevice = nullptr;
    }
}

void WaylandSeat::createRelativePointer()
{
    KWayland::Client::RelativePointerManager *manager = m_backend->relativePointerManager();
    if (manager) {
        m_relativePointerDevice = new WaylandInputDevice(manager->createRelativePointer(m_pointerDevice->nativePointer()), this);
        Q_EMIT deviceAdded(m_relativePointerDevice);
    }
}

void WaylandSeat::destroyRelativePointer()
{
    if (m_relativePointerDevice) {
        Q_EMIT deviceRemoved(m_relativePointerDevice);
        delete m_relativePointerDevice;
        m_relativePointerDevice = nullptr;
    }
}

void WaylandSeat::createKeyboardDevice()
{
    m_keyboardDevice = new WaylandInputDevice(m_seat->createKeyboard(), this);
    Q_EMIT deviceAdded(m_keyboardDevice);
}

void WaylandSeat::destroyKeyboardDevice()
{
    if (m_keyboardDevice) {
        Q_EMIT deviceRemoved(m_keyboardDevice);
        delete m_keyboardDevice;
        m_keyboardDevice = nullptr;
    }
}

void WaylandSeat::createTouchDevice()
{
    m_touchDevice = new WaylandInputDevice(m_seat->createTouch(), this);
    Q_EMIT deviceAdded(m_touchDevice);
}

void WaylandSeat::destroyTouchDevice()
{
    if (m_touchDevice) {
        Q_EMIT deviceRemoved(m_touchDevice);
        delete m_touchDevice;
        m_touchDevice = nullptr;
    }
}

WaylandBackend::WaylandBackend(QObject *parent)
    : Platform(parent)
    , m_session(Session::create(Session::Type::Noop, this))
    , m_display(nullptr)
    , m_eventQueue(new EventQueue(this))
    , m_registry(new Registry(this))
    , m_compositor(new KWayland::Client::Compositor(this))
    , m_subCompositor(new KWayland::Client::SubCompositor(this))
    , m_shm(new ShmPool(this))
    , m_connectionThreadObject(new ConnectionThread(nullptr))
    , m_connectionThread(nullptr)
{
    setPerScreenRenderingEnabled(true);
    supportsOutputChanges();
    connect(this, &WaylandBackend::connectionFailed, qApp, &QCoreApplication::quit);


#if HAVE_WAYLAND_EGL
    char const *drm_render_node = "/dev/dri/renderD128";
    m_drmFileDescriptor = open(drm_render_node, O_RDWR);
    if (m_drmFileDescriptor < 0) {
        qCWarning(KWIN_WAYLAND_BACKEND) << "Failed to open drm render node" << drm_render_node;
        m_gbmDevice = nullptr;
        return;
    }
    m_gbmDevice = gbm_create_device(m_drmFileDescriptor);
#endif
}

WaylandBackend::~WaylandBackend()
{
    if (sceneEglDisplay() != EGL_NO_DISPLAY) {
        eglTerminate(sceneEglDisplay());
    }

    if (m_pointerGestures) {
        m_pointerGestures->release();
    }
    if (m_pointerConstraints) {
        m_pointerConstraints->release();
    }
    delete m_waylandCursor;

    m_eventQueue->release();
    destroyOutputs();

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
#if HAVE_WAYLAND_EGL
    gbm_device_destroy(m_gbmDevice);
    close(m_drmFileDescriptor);
#endif
    qCDebug(KWIN_WAYLAND_BACKEND) << "Destroyed Wayland display";
}

bool WaylandBackend::initialize()
{
    connect(m_registry, &Registry::compositorAnnounced, this, [this](quint32 name, quint32 version) {
        if (version < 4) {
            qFatal("wl_compositor version 4 or later is required");
        }
        m_compositor->setup(m_registry->bindCompositor(name, version));
    });
    connect(m_registry, &Registry::subCompositorAnnounced, this,
        [this](quint32 name) {
            m_subCompositor->setup(m_registry->bindSubCompositor(name, 1));
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
                Q_EMIT pointerLockSupportedChanged();
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
                Q_EMIT pointerLockSupportedChanged();
            }
        }
    );
    connect(m_registry, &Registry::pointerGesturesUnstableV1Announced, this,
        [this](quint32 name, quint32 version) {
            if (m_pointerGestures) {
                return;
            }
            m_pointerGestures = m_registry->createPointerGestures(name, version, this);
        }
    );
    connect(m_registry, &Registry::interfacesAnnounced, this, &WaylandBackend::createOutputs);
    connect(m_registry, &Registry::interfacesAnnounced, this,
        [this] {
            const auto seatInterface = m_registry->interface(Registry::Interface::Seat);
            if (seatInterface.name == 0) {
                return;
            }

            m_seat = new WaylandSeat(m_registry->createSeat(seatInterface.name, std::min(2u, seatInterface.version), this), this);
            Q_EMIT seatCreated();

            m_waylandCursor = new WaylandCursor(this);
        }
    );
    if (!deviceIdentifier().isEmpty()) {
        m_connectionThreadObject->setSocketName(deviceIdentifier());
    }
    connect(Cursors::self(), &Cursors::currentCursorChanged, this,
        [this] {
            if (!m_seat || !m_waylandCursor) {
                return;
            }
            m_waylandCursor->installImage();
        }
    );
    connect(Cursors::self(), &Cursors::positionChanged, this,
        [this](Cursor *cursor, const QPoint &position) {
            Q_UNUSED(cursor)
            if (m_waylandCursor) {
                m_waylandCursor->move(position);
            }
        }
    );
    connect(this, &WaylandBackend::pointerLockChanged, this, [this] (bool locked) {
        delete m_waylandCursor;
        if (locked) {
            m_waylandCursor = new WaylandSubSurfaceCursor(this);
            m_waylandCursor->move(input()->pointer()->pos());
            m_seat->createRelativePointer();
        } else {
            m_seat->destroyRelativePointer();
            m_waylandCursor = new WaylandCursor(this);
        }
        m_waylandCursor->init();
    });
    initConnection();
    return true;
}

Session *WaylandBackend::session() const
{
    return m_session;
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
            Q_EMIT systemCompositorDied();
            delete m_seat;
            m_shm->destroy();

            destroyOutputs();

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

KWayland::Client::ServerSideDecorationManager *WaylandBackend::ssdManager()
{
    if (!m_ssdManager) {
        using namespace KWayland::Client;
        const auto ssdManagerIface = m_registry->interface(Registry::Interface::ServerSideDecorationManager);
        m_ssdManager = ssdManagerIface.name == 0 ? nullptr : m_registry->createServerSideDecorationManager(ssdManagerIface.name, ssdManagerIface.version, this);
    }
    return m_ssdManager;
}

void WaylandBackend::createOutputs()
{
    using namespace KWayland::Client;


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
        createOutput(QPoint(logicalWidthSum, 0), QSize(pixelWidth, pixelHeight));

        logicalWidthSum += logicalWidth;
    }
}

WaylandOutput *WaylandBackend::createOutput(const QPoint &position, const QSize &size)
{
    auto surface = m_compositor->createSurface(this);
    if (!surface || !surface->isValid()) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Creating Wayland Surface failed";
        return nullptr;
    }

    if (ssdManager()) {
        auto decoration = ssdManager()->create(surface, this);
        connect(decoration, &ServerSideDecoration::modeChanged, this, [decoration] {
            if (decoration->mode() != ServerSideDecoration::Mode::Server) {
                decoration->requestMode(ServerSideDecoration::Mode::Server);
            }
        });
    }

    WaylandOutput *waylandOutput = nullptr;

    if (m_xdgShell && m_xdgShell->isValid()) {
        waylandOutput = new XdgShellOutput(surface, m_xdgShell, this, m_nextId++);
    }

    if (!waylandOutput) {
        qCCritical(KWIN_WAYLAND_BACKEND) << "Binding to all shell interfaces failed for output";
        return nullptr;
    }

    waylandOutput->init(position, size);

    connect(waylandOutput, &WaylandOutput::sizeChanged, this, [this, waylandOutput](const QSize &size) {
        Q_UNUSED(size)
        updateScreenSize(waylandOutput);
        Compositor::self()->scene()->addRepaintFull();
    });
    connect(waylandOutput, &WaylandOutput::frameRendered, this, [waylandOutput]() {
        waylandOutput->resetRendered();

        // The current time of the monotonic clock is a pretty good estimate when the frame
        // has been presented, however it will be much better if we check whether the host
        // compositor supports the wp_presentation protocol.
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(waylandOutput->renderLoop());
        renderLoopPrivate->notifyFrameCompleted(std::chrono::steady_clock::now().time_since_epoch());
    });

    // The output will only actually be added when it receives its first
    // configure event, and buffers can start being attached
    m_pendingInitialOutputs++;
    return waylandOutput;
}

void WaylandBackend::destroyOutputs()
{
    while (!m_outputs.isEmpty()) {
        WaylandOutput *output = m_outputs.takeLast();
        Q_EMIT outputDisabled(output);
        Q_EMIT outputRemoved(output);
        delete output;
    }
}

InputBackend *WaylandBackend::createInputBackend()
{
    return new WaylandInputBackend(this);
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

WaylandOutput *WaylandBackend::findOutput(KWayland::Client::Surface *nativeSurface) const
{
    for (WaylandOutput *output : m_outputs) {
        if (output->surface() == nativeSurface) {
            return output;
        }
    }
    return nullptr;
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
    auto pointer = m_seat->pointerDevice()->nativePointer();
    if (!pointer) {
        return;
    }
    if (m_outputs.isEmpty()) {
        return;
    }

    for (auto output : qAsConst(m_outputs)) {
        output->lockPointer(m_seat->pointerDevice()->nativePointer(), !m_pointerLockRequested);
    }
    m_pointerLockRequested = !m_pointerLockRequested;
    flush();
}

bool WaylandBackend::pointerIsLocked()
{
    for (auto *output : qAsConst(m_outputs)) {
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

void WaylandBackend::addConfiguredOutput(WaylandOutput *output)
{
    m_outputs << output;
    Q_EMIT outputAdded(output);
    Q_EMIT outputEnabled(output);

    m_pendingInitialOutputs--;
    if (m_pendingInitialOutputs == 0) {
        // Mark as ready once all the initial set of screens has arrived
        // (i.e, received their first configure and it is now safe to commit
        // buffers to them)
        setReady(true);
        Q_EMIT screensQueried();
    }
}

DmaBufTexture *WaylandBackend::createDmaBufTexture(const QSize& size)
{
#if HAVE_WAYLAND_EGL
    return GbmDmaBuf::createBuffer(size, m_gbmDevice);
#else
    return nullptr;
#endif
}

void WaylandBackend::createDpmsFilter()
{
    if (m_dpmsFilter) {
        // already another output is off
        return;
    }
    m_dpmsFilter.reset(new DpmsInputEventFilter);
    input()->prependInputEventFilter(m_dpmsFilter.data());
}

void WaylandBackend::clearDpmsFilter()
{
    m_dpmsFilter.reset();
}

AbstractOutput *WaylandBackend::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    Q_UNUSED(name);
    return createOutput(m_outputs.constLast()->geometry().topRight(), size * scale);
}

void WaylandBackend::removeVirtualOutput(AbstractOutput *output)
{
    WaylandOutput *waylandOutput = dynamic_cast<WaylandOutput *>(output);
    if (waylandOutput && m_outputs.removeAll(waylandOutput)) {
        Q_EMIT outputDisabled(waylandOutput);
        Q_EMIT outputRemoved(waylandOutput);
        delete waylandOutput;
    }
}

}

} // KWin
