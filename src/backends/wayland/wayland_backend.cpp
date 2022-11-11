/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_backend.h"

#if HAVE_WAYLAND_EGL
#include "wayland_egl_backend.h"
#include <gbm.h>
#endif
#include "wayland_display.h"
#include "wayland_logging.h"
#include "wayland_output.h"
#include "wayland_qpainter_backend.h"

#include "composite.h"
#include "cursor.h"
#include "dpmsinputeventfilter.h"
#include "input.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "scene.h"

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/pointergestures.h>
#include <KWayland/Client/relativepointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/subcompositor.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

#include <QAbstractEventDispatcher>

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include "../drm/gbm_dmabuf.h"
#include <cmath>
#include <drm_fourcc.h>

#define QSIZE_TO_QPOINT(size) QPointF(size.width(), size.height())

namespace KWin
{
namespace Wayland
{

using namespace KWayland::Client;

WaylandCursor::WaylandCursor(WaylandBackend *backend)
    : m_backend(backend)
{
    resetSurface();
}

WaylandCursor::~WaylandCursor() = default;

void WaylandCursor::resetSurface()
{
    m_surface.reset(backend()->display()->compositor()->createSurface());
}

void WaylandCursor::init()
{
    installImage();
}

void WaylandCursor::installImage()
{
    const QImage image = Cursors::self()->currentCursor()->image();
    if (image.isNull() || image.size().isEmpty()) {
        doInstallImage(nullptr, QSize(), 1);
        return;
    }

    auto buffer = m_backend->display()->shmPool()->createBuffer(image).toStrongRef();
    wl_buffer *imageBuffer = *buffer.data();
    doInstallImage(imageBuffer, image.size(), image.devicePixelRatio());
}

void WaylandCursor::doInstallImage(wl_buffer *image, const QSize &size, qreal scale)
{
    auto *pointer = m_backend->seat()->pointerDevice()->nativePointer();
    if (!pointer || !pointer->isValid()) {
        return;
    }
    pointer->setCursor(m_surface.get(), image ? Cursors::self()->currentCursor()->hotspot() : QPoint());
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

WaylandSubSurfaceCursor::~WaylandSubSurfaceCursor() = default;

void WaylandSubSurfaceCursor::init()
{
    if (auto *pointer = backend()->seat()->pointerDevice()->nativePointer()) {
        pointer->hideCursor();
    }
}

void WaylandSubSurfaceCursor::changeOutput(WaylandOutput *output)
{
    m_subSurface.reset();
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
    m_subSurface.reset(backend()->display()->subCompositor()->createSubSurface(surface(), m_output->surface()));
    m_subSurface->setMode(SubSurface::Mode::Desynchronized);
}

void WaylandSubSurfaceCursor::doInstallImage(wl_buffer *image, const QSize &size, qreal scale)
{
    if (!image) {
        m_subSurface.reset();
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
    : m_seat(seat)
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
    connect(keyboard, &Keyboard::modifiersChanged, this, [](quint32 depressed, quint32 latched, quint32 locked, quint32 group) {
        input()->keyboard()->processModifiers(depressed, latched, locked, group);
    });
    connect(keyboard, &Keyboard::keymapChanged, this, [](int fd, quint32 size) {
        input()->keyboard()->processKeymapChange(fd, size);
    });
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::Pointer *pointer, WaylandSeat *seat)
    : m_seat(seat)
    , m_pointer(pointer)
{
    connect(pointer, &Pointer::entered, this, [this](quint32 serial, const QPointF &relativeToSurface) {
        m_enteredSerial = serial;
    });
    connect(pointer, &Pointer::motion, this, [this](const QPointF &relativeToSurface, quint32 time) {
        WaylandOutput *output = m_seat->backend()->findOutput(m_pointer->enteredSurface());
        Q_ASSERT(output);
        const QPointF absolutePos = output->geometry().topLeft() + relativeToSurface;
        Q_EMIT pointerMotionAbsolute(absolutePos, time, this);
    });
    connect(pointer, &Pointer::buttonStateChanged, this, [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState nativeState) {
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

    KWayland::Client::PointerGestures *pointerGestures = m_seat->backend()->display()->pointerGestures();
    if (pointerGestures) {
        m_pinchGesture.reset(pointerGestures->createPinchGesture(m_pointer.get(), this));
        connect(m_pinchGesture.get(), &PointerPinchGesture::started, this, [this](quint32 serial, quint32 time) {
            Q_EMIT pinchGestureBegin(m_pinchGesture->fingerCount(), time, this);
        });
        connect(m_pinchGesture.get(), &PointerPinchGesture::updated, this, [this](const QSizeF &delta, qreal scale, qreal rotation, quint32 time) {
            Q_EMIT pinchGestureUpdate(scale, rotation, QSIZE_TO_QPOINT(delta), time, this);
        });
        connect(m_pinchGesture.get(), &PointerPinchGesture::ended, this, [this](quint32 serial, quint32 time) {
            Q_EMIT pinchGestureEnd(time, this);
        });
        connect(m_pinchGesture.get(), &PointerPinchGesture::cancelled, this, [this](quint32 serial, quint32 time) {
            Q_EMIT pinchGestureCancelled(time, this);
        });

        m_swipeGesture.reset(pointerGestures->createSwipeGesture(m_pointer.get(), this));
        connect(m_swipeGesture.get(), &PointerSwipeGesture::started, this, [this](quint32 serial, quint32 time) {
            Q_EMIT swipeGestureBegin(m_swipeGesture->fingerCount(), time, this);
        });
        connect(m_swipeGesture.get(), &PointerSwipeGesture::updated, this, [this](const QSizeF &delta, quint32 time) {
            Q_EMIT swipeGestureUpdate(QSIZE_TO_QPOINT(delta), time, this);
        });
        connect(m_swipeGesture.get(), &PointerSwipeGesture::ended, this, [this](quint32 serial, quint32 time) {
            Q_EMIT swipeGestureEnd(time, this);
        });
        connect(m_swipeGesture.get(), &PointerSwipeGesture::cancelled, this, [this](quint32 serial, quint32 time) {
            Q_EMIT swipeGestureCancelled(time, this);
        });
    }
}

WaylandInputDevice::WaylandInputDevice(KWayland::Client::RelativePointer *relativePointer, WaylandSeat *seat)
    : m_seat(seat)
    , m_relativePointer(relativePointer)
{
    connect(relativePointer, &RelativePointer::relativeMotion, this, [this](const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 timestamp) {
        Q_EMIT pointerMotion(QSIZE_TO_QPOINT(delta), QSIZE_TO_QPOINT(deltaNonAccelerated), timestamp, timestamp * 1000, this);
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
}

LEDs WaylandInputDevice::leds() const
{
    return LEDs();
}

void WaylandInputDevice::setLeds(LEDs leds)
{
}

bool WaylandInputDevice::isKeyboard() const
{
    return m_keyboard != nullptr;
}

bool WaylandInputDevice::isAlphaNumericKeyboard() const
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

WaylandBackend::WaylandBackend(QObject *parent)
    : OutputBackend(parent)
{
#if HAVE_WAYLAND_EGL
    char const *drm_render_node = "/dev/dri/renderD128";
    m_drmFileDescriptor = FileDescriptor(open(drm_render_node, O_RDWR));
    if (!m_drmFileDescriptor.isValid()) {
        qCWarning(KWIN_WAYLAND_BACKEND) << "Failed to open drm render node" << drm_render_node;
        m_gbmDevice = nullptr;
        return;
    }
    m_gbmDevice = gbm_create_device(m_drmFileDescriptor.get());
#endif
}

WaylandBackend::~WaylandBackend()
{
    if (sceneEglDisplay() != EGL_NO_DISPLAY) {
        eglTerminate(sceneEglDisplay());
    }

    destroyOutputs();

    m_waylandCursor.reset();
    m_seat.reset();
    m_display.reset();

#if HAVE_WAYLAND_EGL
    gbm_device_destroy(m_gbmDevice);
#endif
    qCDebug(KWIN_WAYLAND_BACKEND) << "Destroyed Wayland display";
}

bool WaylandBackend::initialize()
{
    m_display = std::make_unique<WaylandDisplay>();
    if (!m_display->initialize(deviceIdentifier())) {
        return false;
    }

    createOutputs();

    m_seat = std::make_unique<WaylandSeat>(m_display->seat(), this);
    m_waylandCursor = std::make_unique<WaylandCursor>(this);

    QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance();
    QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, m_display.get(), &WaylandDisplay::flush);
    QObject::connect(dispatcher, &QAbstractEventDispatcher::awake, m_display.get(), &WaylandDisplay::flush);

    connect(Cursors::self(), &Cursors::currentCursorChanged, this, [this]() {
        if (!m_seat || !m_waylandCursor) {
            return;
        }
        m_waylandCursor->installImage();
    });
    connect(Cursors::self(), &Cursors::positionChanged, this, [this](Cursor *cursor, const QPoint &position) {
        if (m_waylandCursor) {
            m_waylandCursor->move(position);
        }
    });
    connect(this, &WaylandBackend::pointerLockChanged, this, [this](bool locked) {
        if (locked) {
            m_waylandCursor = std::make_unique<WaylandSubSurfaceCursor>(this);
            m_waylandCursor->move(input()->pointer()->pos());
            m_seat->createRelativePointer();
        } else {
            m_seat->destroyRelativePointer();
            m_waylandCursor = std::make_unique<WaylandCursor>(this);
        }
        m_waylandCursor->init();
    });

    return true;
}

void WaylandBackend::createOutputs()
{
    // we need to multiply the initial window size with the scale in order to
    // create an output window of this size in the end
    const QSize nativeSize = initialWindowSize() * initialOutputScale();
    for (int i = 0; i < initialOutputCount(); i++) {
        WaylandOutput *output = createOutput(QStringLiteral("WL-%1").arg(i), nativeSize);
        m_outputs << output;
        Q_EMIT outputAdded(output);
        output->updateEnabled(true);
    }

    Q_EMIT outputsQueried();
}

WaylandOutput *WaylandBackend::createOutput(const QString &name, const QSize &size)
{
    WaylandOutput *waylandOutput = new WaylandOutput(name, this);
    waylandOutput->init(size);

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
        output->updateEnabled(false);
        Q_EMIT outputRemoved(output);
        delete output;
    }
}

std::unique_ptr<InputBackend> WaylandBackend::createInputBackend()
{
    return std::make_unique<WaylandInputBackend>(this);
}

std::unique_ptr<OpenGLBackend> WaylandBackend::createOpenGLBackend()
{
#if HAVE_WAYLAND_EGL
    return std::make_unique<WaylandEglBackend>(this);
#else
    return nullptr;
#endif
}

std::unique_ptr<QPainterBackend> WaylandBackend::createQPainterBackend()
{
    return std::make_unique<WaylandQPainterBackend>(this);
}

void WaylandBackend::flush()
{
    m_display->flush();
}

WaylandOutput *WaylandBackend::getOutputAt(const QPointF &globalPosition)
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
    flush();
}

QVector<CompositingType> WaylandBackend::supportedCompositors() const
{
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

void WaylandBackend::createDpmsFilter()
{
    if (m_dpmsFilter) {
        // already another output is off
        return;
    }
    m_dpmsFilter = std::make_unique<DpmsInputEventFilter>();
    input()->prependInputEventFilter(m_dpmsFilter.get());
}

void WaylandBackend::clearDpmsFilter()
{
    m_dpmsFilter.reset();
}

Output *WaylandBackend::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    return createOutput(name, size * scale);
}

void WaylandBackend::removeVirtualOutput(Output *output)
{
    WaylandOutput *waylandOutput = dynamic_cast<WaylandOutput *>(output);
    if (waylandOutput && m_outputs.removeAll(waylandOutput)) {
        waylandOutput->updateEnabled(false);
        Q_EMIT outputRemoved(waylandOutput);
        waylandOutput->unref();
    }
}

std::optional<DmaBufParams> WaylandBackend::testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers)
{
    gbm_bo *bo = createGbmBo(m_gbmDevice, size, format, modifiers);
    if (!bo) {
        return {};
    }

    auto ret = dmaBufParamsForBo(bo);
    gbm_bo_destroy(bo);
    return ret;
}

std::shared_ptr<DmaBufTexture> WaylandBackend::createDmaBufTexture(const QSize &size, quint32 format, uint64_t modifier)
{
    gbm_bo *bo = createGbmBo(m_gbmDevice, size, format, {modifier});
    if (!bo) {
        return {};
    }

    // The bo will be kept around until the last fd is closed.
    DmaBufAttributes attributes = dmaBufAttributesForBo(bo);
    gbm_bo_destroy(bo);
    m_eglBackend->makeCurrent();
    return std::make_shared<DmaBufTexture>(m_eglBackend->importDmaBufAsTexture(attributes), std::move(attributes));
}

}

} // KWin
