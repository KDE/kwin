/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_backend.h"
#include "compositor.h"
#include "core/drmdevice.h"
#include "input.h"
#include "wayland_display.h"
#include "wayland_egl_backend.h"
#include "wayland_logging.h"
#include "wayland_output.h"
#include "wayland_qpainter_backend.h"

#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointergestures.h>
#include <KWayland/Client/relativepointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

#include <QAbstractEventDispatcher>

#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <linux/input.h>
#include <unistd.h>
#include <wayland-client-core.h>

#include "wayland-linux-dmabuf-unstable-v1-client-protocol.h"

namespace KWin
{
namespace Wayland
{

using namespace KWayland::Client;

inline static QPointF sizeToPoint(const QSizeF &size)
{
    return QPointF(size.width(), size.height());
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::Keyboard *keyboard, WaylandSeat *seat)
    : m_seat(seat)
    , m_keyboard(keyboard)
{
    connect(keyboard, &Keyboard::left, this, [this](quint32 time) {
        for (quint32 key : std::as_const(m_pressedKeys)) {
            Q_EMIT keyChanged(key, KeyboardKeyState::Released, std::chrono::milliseconds(time), this);
        }
        m_pressedKeys.clear();
    });
    connect(keyboard, &Keyboard::keyChanged, this, [this](quint32 key, Keyboard::KeyState nativeState, quint32 time) {
        KeyboardKeyState state;
        switch (nativeState) {
        case Keyboard::KeyState::Pressed:
            if (key == KEY_RIGHTCTRL) {
                m_seat->backend()->togglePointerLock();
            }
            state = KeyboardKeyState::Pressed;
            m_pressedKeys.insert(key);
            break;
        case Keyboard::KeyState::Released:
            m_pressedKeys.remove(key);
            state = KeyboardKeyState::Released;
            break;
        default:
            Q_UNREACHABLE();
        }
        Q_EMIT keyChanged(key, state, std::chrono::milliseconds(time), this);
    });
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::Pointer *pointer, WaylandSeat *seat)
    : m_seat(seat)
    , m_pointer(pointer)
{
    connect(pointer, &Pointer::entered, this, [this](quint32 serial, const QPointF &relativeToSurface) {
        WaylandOutput *output = m_seat->backend()->findOutput(m_pointer->enteredSurface());
        Q_ASSERT(output);
        output->cursor()->setPointer(m_pointer.get());
    });
    connect(pointer, &Pointer::left, this, [this]() {
        // wl_pointer.leave carries the wl_surface, but KWayland::Client::Pointer::left does not.
        const auto outputs = m_seat->backend()->outputs();
        for (Output *output : outputs) {
            WaylandOutput *waylandOutput = static_cast<WaylandOutput *>(output);
            if (waylandOutput->cursor()->pointer()) {
                waylandOutput->cursor()->setPointer(nullptr);
            }
        }
    });
    connect(pointer, &Pointer::motion, this, [this](const QPointF &relativeToSurface, quint32 time) {
        WaylandOutput *output = m_seat->backend()->findOutput(m_pointer->enteredSurface());
        Q_ASSERT(output);
        const auto subsurface = m_seat->backend()->findSubSurface(m_pointer->enteredSurface());
        const QPointF absolutePos = output->geometry().topLeft() + relativeToSurface
            + (subsurface ? subsurface->position() : QPoint());
        Q_EMIT pointerMotionAbsolute(absolutePos, std::chrono::milliseconds(time), this);
    });
    connect(pointer, &Pointer::buttonStateChanged, this, [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState nativeState) {
        PointerButtonState state;
        switch (nativeState) {
        case Pointer::ButtonState::Pressed:
            state = PointerButtonState::Pressed;
            break;
        case Pointer::ButtonState::Released:
            state = PointerButtonState::Released;
            break;
        default:
            Q_UNREACHABLE();
        }
        Q_EMIT pointerButtonChanged(button, state, std::chrono::milliseconds(time), this);
    });
    // TODO: Send discreteDelta and source as well.
    connect(pointer, &Pointer::axisChanged, this, [this](quint32 time, Pointer::Axis nativeAxis, qreal delta) {
        PointerAxis axis;
        switch (nativeAxis) {
        case Pointer::Axis::Horizontal:
            axis = PointerAxis::Horizontal;
            break;
        case Pointer::Axis::Vertical:
            axis = PointerAxis::Vertical;
            break;
        default:
            Q_UNREACHABLE();
        }
        Q_EMIT pointerAxisChanged(axis, delta, 0, PointerAxisSource::Unknown, false, std::chrono::milliseconds(time), this);
    });

    connect(pointer, &Pointer::frame, this, [this]() {
        Q_EMIT pointerFrame(this);
    });

    KWayland::Client::PointerGestures *pointerGestures = m_seat->backend()->display()->pointerGestures();
    if (pointerGestures) {
        m_pinchGesture.reset(pointerGestures->createPinchGesture(m_pointer.get(), this));
        connect(m_pinchGesture.get(), &PointerPinchGesture::started, this, [this](quint32 serial, quint32 time) {
            Q_EMIT pinchGestureBegin(m_pinchGesture->fingerCount(), std::chrono::milliseconds(time), this);
        });
        connect(m_pinchGesture.get(), &PointerPinchGesture::updated, this, [this](const QSizeF &delta, qreal scale, qreal rotation, quint32 time) {
            Q_EMIT pinchGestureUpdate(scale, rotation, sizeToPoint(delta), std::chrono::milliseconds(time), this);
        });
        connect(m_pinchGesture.get(), &PointerPinchGesture::ended, this, [this](quint32 serial, quint32 time) {
            Q_EMIT pinchGestureEnd(std::chrono::milliseconds(time), this);
        });
        connect(m_pinchGesture.get(), &PointerPinchGesture::cancelled, this, [this](quint32 serial, quint32 time) {
            Q_EMIT pinchGestureCancelled(std::chrono::milliseconds(time), this);
        });

        m_swipeGesture.reset(pointerGestures->createSwipeGesture(m_pointer.get(), this));
        connect(m_swipeGesture.get(), &PointerSwipeGesture::started, this, [this](quint32 serial, quint32 time) {
            Q_EMIT swipeGestureBegin(m_swipeGesture->fingerCount(), std::chrono::milliseconds(time), this);
        });
        connect(m_swipeGesture.get(), &PointerSwipeGesture::updated, this, [this](const QSizeF &delta, quint32 time) {
            Q_EMIT swipeGestureUpdate(sizeToPoint(delta), std::chrono::milliseconds(time), this);
        });
        connect(m_swipeGesture.get(), &PointerSwipeGesture::ended, this, [this](quint32 serial, quint32 time) {
            Q_EMIT swipeGestureEnd(std::chrono::milliseconds(time), this);
        });
        connect(m_swipeGesture.get(), &PointerSwipeGesture::cancelled, this, [this](quint32 serial, quint32 time) {
            Q_EMIT swipeGestureCancelled(std::chrono::milliseconds(time), this);
        });
    }
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::RelativePointer *relativePointer, WaylandSeat *seat)
    : m_seat(seat)
    , m_relativePointer(relativePointer)
{
    connect(relativePointer, &RelativePointer::relativeMotion, this, [this](const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 timestamp) {
        Q_EMIT pointerMotion(sizeToPoint(delta), sizeToPoint(deltaNonAccelerated), std::chrono::microseconds(timestamp), this);
    });
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::Touch *touch, WaylandSeat *seat)
    : m_seat(seat)
    , m_touch(touch)
{
    connect(touch, &Touch::sequenceCanceled, this, [this]() {
        Q_EMIT touchCanceled(this);
    });
    connect(touch, &Touch::frameEnded, this, [this]() {
        Q_EMIT touchFrame(this);
    });
    connect(touch, &Touch::sequenceStarted, this, [this](TouchPoint *tp) {
        auto o = m_seat->backend()->findOutput(tp->surface());
        Q_ASSERT(o);
        const QPointF position = o->geometry().topLeft() + tp->position();
        Q_EMIT touchDown(tp->id(), position, std::chrono::milliseconds(tp->time()), this);
    });
    connect(touch, &Touch::pointAdded, this, [this](TouchPoint *tp) {
        auto o = m_seat->backend()->findOutput(tp->surface());
        Q_ASSERT(o);
        const QPointF position = o->geometry().topLeft() + tp->position();
        Q_EMIT touchDown(tp->id(), position, std::chrono::milliseconds(tp->time()), this);
    });
    connect(touch, &Touch::pointRemoved, this, [this](TouchPoint *tp) {
        Q_EMIT touchUp(tp->id(), std::chrono::milliseconds(tp->time()), this);
    });
    connect(touch, &Touch::pointMoved, this, [this](TouchPoint *tp) {
        auto o = m_seat->backend()->findOutput(tp->surface());
        Q_ASSERT(o);
        const QPointF position = o->geometry().topLeft() + tp->position();
        Q_EMIT touchMotion(tp->id(), position, std::chrono::milliseconds(tp->time()), this);
    });
}

WaylandInputDevice::~WaylandInputDevice()
{
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
}

bool WaylandInputDevice::isKeyboard() const
{
    return m_keyboard != nullptr;
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
    return m_touch != nullptr;
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
    return m_pointer.get();
}

WaylandInputBackend::WaylandInputBackend(WaylandBackend *backend, QObject *parent)
    : InputBackend(parent)
    , m_backend(backend)
{
}

void WaylandInputBackend::initialize()
{
    WaylandSeat *seat = m_backend->seat();
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

WaylandSeat::WaylandSeat(KWayland::Client::Seat *nativeSeat, WaylandBackend *backend)
    : QObject(nullptr)
    , m_seat(nativeSeat)
    , m_backend(backend)
{
    auto updateKeyboardDevice = [this](){
        if (m_seat->hasKeyboard()) {
            createKeyboardDevice();
        } else {
            destroyKeyboardDevice();
        }
    };

    updateKeyboardDevice();
    connect(m_seat, &Seat::hasKeyboardChanged, this, updateKeyboardDevice);

    auto updatePointerDevice = [this]() {
        if (m_seat->hasPointer()) {
            createPointerDevice();
        } else {
            destroyPointerDevice();
        }
    };

    updatePointerDevice();
    connect(m_seat, &Seat::hasPointerChanged, this, updatePointerDevice);

    auto updateTouchDevice = [this]() {
        if (m_seat->hasTouch()) {
            createTouchDevice();
        } else {
            destroyTouchDevice();
        }
    };

    updateTouchDevice();
    connect(m_seat, &Seat::hasTouchChanged, this, updateTouchDevice);
}

WaylandSeat::~WaylandSeat()
{
    destroyPointerDevice();
    destroyKeyboardDevice();
    destroyTouchDevice();
}

void WaylandSeat::createPointerDevice()
{
    m_pointerDevice = std::make_unique<WaylandInputDevice>(m_seat->createPointer(), this);
    Q_EMIT deviceAdded(m_pointerDevice.get());
}

void WaylandSeat::destroyPointerDevice()
{
    if (m_pointerDevice) {
        Q_EMIT deviceRemoved(m_pointerDevice.get());
        destroyRelativePointer();
        m_pointerDevice.reset();
    }
}

void WaylandSeat::createRelativePointer()
{
    KWayland::Client::RelativePointerManager *manager = m_backend->display()->relativePointerManager();
    if (manager) {
        m_relativePointerDevice = std::make_unique<WaylandInputDevice>(manager->createRelativePointer(m_pointerDevice->nativePointer()), this);
        Q_EMIT deviceAdded(m_relativePointerDevice.get());
    }
}

void WaylandSeat::destroyRelativePointer()
{
    if (m_relativePointerDevice) {
        Q_EMIT deviceRemoved(m_relativePointerDevice.get());
        m_relativePointerDevice.reset();
    }
}

void WaylandSeat::createKeyboardDevice()
{
    m_keyboardDevice = std::make_unique<WaylandInputDevice>(m_seat->createKeyboard(), this);
    Q_EMIT deviceAdded(m_keyboardDevice.get());
}

void WaylandSeat::destroyKeyboardDevice()
{
    if (m_keyboardDevice) {
        Q_EMIT deviceRemoved(m_keyboardDevice.get());
        m_keyboardDevice.reset();
    }
}

void WaylandSeat::createTouchDevice()
{
    m_touchDevice = std::make_unique<WaylandInputDevice>(m_seat->createTouch(), this);
    Q_EMIT deviceAdded(m_touchDevice.get());
}

void WaylandSeat::destroyTouchDevice()
{
    if (m_touchDevice) {
        Q_EMIT deviceRemoved(m_touchDevice.get());
        m_touchDevice.reset();
    }
}

WaylandBackend::WaylandBackend(const WaylandBackendOptions &options, QObject *parent)
    : OutputBackend(parent)
    , m_options(options)
{
}

WaylandBackend::~WaylandBackend()
{
    m_eglDisplay.reset();
    destroyOutputs();

    m_buffers.clear();

    m_seat.reset();
    m_display.reset();
    qCDebug(KWIN_WAYLAND_BACKEND) << "Destroyed Wayland display";
}

bool WaylandBackend::initialize()
{
    m_display = std::make_unique<WaylandDisplay>();
    if (!m_display->initialize(m_options.socketName)) {
        return false;
    }

    if (WaylandLinuxDmabufV1 *dmabuf = m_display->linuxDmabuf()) {
        m_drmDevice = DrmDevice::open(dmabuf->mainDevice());
        if (!m_drmDevice) {
            qCWarning(KWIN_WAYLAND_BACKEND) << "Failed to open drm render node" << dmabuf->mainDevice();
        }
    }

    createOutputs();

    m_seat = std::make_unique<WaylandSeat>(m_display->seat(), this);

    QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance();
    QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, m_display.get(), &WaylandDisplay::flush);
    QObject::connect(dispatcher, &QAbstractEventDispatcher::awake, m_display.get(), &WaylandDisplay::flush);

    connect(this, &WaylandBackend::pointerLockChanged, this, [this](bool locked) {
        if (locked) {
            m_seat->createRelativePointer();
        } else {
            m_seat->destroyRelativePointer();
        }
    });

    return true;
}

void WaylandBackend::createOutputs()
{
    // we need to multiply the initial window size with the scale in order to
    // create an output window of this size in the end
    const QSize pixelSize = m_options.outputSize * m_options.outputScale;
    for (int i = 0; i < m_options.outputCount; i++) {
        WaylandOutput *output = createOutput(QStringLiteral("WL-%1").arg(i), pixelSize, m_options.outputScale, m_options.fullscreen);
        m_outputs << output;
        Q_EMIT outputAdded(output);
    }

    Q_EMIT outputsQueried();
}

WaylandOutput *WaylandBackend::createOutput(const QString &name, const QSize &size, qreal scale, bool fullscreen)
{
    WaylandOutput *waylandOutput = new WaylandOutput(name, this);
    waylandOutput->init(size, scale, fullscreen);

    // Wait until the output window is configured by the host compositor.
    while (!waylandOutput->isReady()) {
        wl_display_roundtrip(m_display->nativeDisplay());
    }

    return waylandOutput;
}

void WaylandBackend::destroyOutputs()
{
    while (!m_outputs.isEmpty()) {
        WaylandOutput *output = m_outputs.takeLast();
        Q_EMIT outputRemoved(output);
        delete output;
    }
}

std::unique_ptr<InputBackend> WaylandBackend::createInputBackend()
{
    return std::make_unique<WaylandInputBackend>(this);
}

std::unique_ptr<EglBackend> WaylandBackend::createOpenGLBackend()
{
    return std::make_unique<WaylandEglBackend>(this);
}

std::unique_ptr<QPainterBackend> WaylandBackend::createQPainterBackend()
{
    return std::make_unique<WaylandQPainterBackend>(this);
}

WaylandOutput *WaylandBackend::findOutput(KWayland::Client::Surface *nativeSurface) const
{
    for (WaylandOutput *output : m_outputs) {
        const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
        const bool isALayer = std::ranges::any_of(layers, [nativeSurface](OutputLayer *layer) {
            if (layer->type() == OutputLayerType::CursorOnly) {
                return false;
            }
            return static_cast<WaylandLayer *>(layer)->surface() == nativeSurface;
        });
        if (isALayer) {
            return output;
        }
        if (output->surface() == nativeSurface) {
            return output;
        }
    }
    return nullptr;
}

KWayland::Client::SubSurface *WaylandBackend::findSubSurface(KWayland::Client::Surface *nativeSurface) const
{
    for (WaylandOutput *output : m_outputs) {
        const auto layers = Compositor::self()->backend()->compatibleOutputLayers(output);
        const auto it = std::ranges::find_if(layers, [nativeSurface](OutputLayer *layer) {
            // cursor-only layers are a different class
            // and can't be a subsurface
            if (layer->type() == OutputLayerType::CursorOnly) {
                return false;
            }
            return static_cast<WaylandLayer *>(layer)->surface() == nativeSurface;
        });
        if (it != layers.end()) {
            return static_cast<WaylandLayer *>(*it)->subSurface();
        }
        if (output->surface() == nativeSurface) {
            return nullptr;
        }
    }
    return nullptr;
}

bool WaylandBackend::supportsPointerLock()
{
    return m_display->pointerConstraints() && m_display->relativePointerManager();
}

void WaylandBackend::togglePointerLock()
{
    if (!supportsPointerLock()) {
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

    for (auto output : std::as_const(m_outputs)) {
        output->lockPointer(m_seat->pointerDevice()->nativePointer(), !m_pointerLockRequested);
    }
    m_pointerLockRequested = !m_pointerLockRequested;
}

QList<CompositingType> WaylandBackend::supportedCompositors() const
{
    QList<CompositingType> ret;
    if (m_display->linuxDmabuf() && m_drmDevice) {
        ret.append(OpenGLCompositing);
    }
    ret.append(QPainterCompositing);
    return ret;
}

Outputs WaylandBackend::outputs() const
{
    return m_outputs;
}

Output *WaylandBackend::createVirtualOutput(const QString &name, const QString &description, const QSize &size, double scale)
{
    return createOutput(name, size * scale, scale, false);
}

void WaylandBackend::removeVirtualOutput(Output *output)
{
    WaylandOutput *waylandOutput = dynamic_cast<WaylandOutput *>(output);
    if (waylandOutput && m_outputs.removeAll(waylandOutput)) {
        Q_EMIT outputRemoved(waylandOutput);
        Q_EMIT outputsQueried();
        waylandOutput->unref();
    }
}

static wl_buffer *importDmaBufBuffer(WaylandDisplay *display, const DmaBufAttributes *attributes)
{
    zwp_linux_buffer_params_v1 *params = zwp_linux_dmabuf_v1_create_params(display->linuxDmabuf()->handle());
    for (int i = 0; i < attributes->planeCount; ++i) {
        zwp_linux_buffer_params_v1_add(params,
                                       attributes->fd[i].get(),
                                       i,
                                       attributes->offset[i],
                                       attributes->pitch[i],
                                       attributes->modifier >> 32,
                                       attributes->modifier & 0xffffffff);
    }

    wl_buffer *buffer = zwp_linux_buffer_params_v1_create_immed(params, attributes->width, attributes->height, attributes->format, 0);
    zwp_linux_buffer_params_v1_destroy(params);

    return buffer;
}

static wl_buffer *importShmBuffer(WaylandDisplay *display, const ShmAttributes *attributes)
{
    wl_shm_format format;
    switch (attributes->format) {
    case DRM_FORMAT_ARGB8888:
        format = WL_SHM_FORMAT_ARGB8888;
        break;
    case DRM_FORMAT_XRGB8888:
        format = WL_SHM_FORMAT_XRGB8888;
        break;
    default:
        format = static_cast<wl_shm_format>(attributes->format);
        break;
    }

    wl_shm_pool *pool = wl_shm_create_pool(display->shm(), attributes->fd.get(), attributes->size.height() * attributes->stride);
    wl_buffer *buffer = wl_shm_pool_create_buffer(pool,
                                                  attributes->offset,
                                                  attributes->size.width(),
                                                  attributes->size.height(),
                                                  attributes->stride,
                                                  format);
    wl_shm_pool_destroy(pool);

    return buffer;
}

wl_buffer *WaylandBackend::importBuffer(GraphicsBuffer *graphicsBuffer)
{
    auto &buffer = m_buffers[graphicsBuffer];
    if (!buffer) {
        wl_buffer *handle = nullptr;
        if (const DmaBufAttributes *attributes = graphicsBuffer->dmabufAttributes()) {
            handle = importDmaBufBuffer(m_display.get(), attributes);
        } else if (const ShmAttributes *attributes = graphicsBuffer->shmAttributes()) {
            handle = importShmBuffer(m_display.get(), attributes);
        } else {
            qCWarning(KWIN_WAYLAND_BACKEND) << graphicsBuffer << "has unknown type";
            return nullptr;
        }

        buffer = std::make_unique<WaylandBuffer>(handle, graphicsBuffer);
        connect(buffer.get(), &WaylandBuffer::defunct, this, [this, graphicsBuffer]() {
            m_buffers.erase(graphicsBuffer);
        });

        static const wl_buffer_listener listener = {
            .release = [](void *userData, wl_buffer *buffer) {
                WaylandBuffer *slot = static_cast<WaylandBuffer *>(userData);
                slot->unlock();
            },
        };
        wl_buffer_add_listener(handle, &listener, buffer.get());
    }

    buffer->lock();
    return buffer->handle();
}

void WaylandBackend::setEglDisplay(std::unique_ptr<EglDisplay> &&display)
{
    m_eglDisplay = std::move(display);
}

EglDisplay *WaylandBackend::sceneEglDisplayObject() const
{
    return m_eglDisplay.get();
}

DrmDevice *WaylandBackend::drmDevice() const
{
    return m_drmDevice.get();
}

WaylandBuffer::WaylandBuffer(wl_buffer *handle, GraphicsBuffer *graphicsBuffer)
    : m_graphicsBuffer(graphicsBuffer)
    , m_handle(handle)
{
    connect(graphicsBuffer, &GraphicsBuffer::destroyed, this, &WaylandBuffer::defunct);
}

WaylandBuffer::~WaylandBuffer()
{
    m_graphicsBuffer->disconnect(this);
    if (m_locked) {
        m_graphicsBuffer->unref();
    }
    wl_buffer_destroy(m_handle);
}

wl_buffer *WaylandBuffer::handle() const
{
    return m_handle;
}

void WaylandBuffer::lock()
{
    if (!m_locked) {
        m_locked = true;
        m_graphicsBuffer->ref();
    }
}

void WaylandBuffer::unlock()
{
    if (m_locked) {
        m_locked = false;
        m_graphicsBuffer->unref();
    }
}
}

} // KWin

#include "moc_wayland_backend.cpp"
