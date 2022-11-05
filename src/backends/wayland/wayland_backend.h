/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <config-kwin.h>
// KWin
#include "core/inputbackend.h"
#include "core/inputdevice.h"
#include "core/outputbackend.h"
#include "utils/filedescriptor.h"
#include <kwinglobals.h>
// Qt
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QSize>

class QTemporaryFile;
struct wl_buffer;
struct wl_display;
struct wl_event_queue;
struct wl_seat;
struct gbm_device;
struct gbm_bo;

namespace KWayland
{
namespace Client
{
class Buffer;
class ShmPool;
class Compositor;
class ConnectionThread;
class EventQueue;
class Keyboard;
class Pointer;
class PointerConstraints;
class PointerGestures;
class PointerSwipeGesture;
class PointerPinchGesture;
class Registry;
class RelativePointer;
class RelativePointerManager;
class Seat;
class ServerSideDecorationManager;
class SubCompositor;
class SubSurface;
class Surface;
class Touch;
class XdgShell;
}
}

namespace KWin
{
class DpmsInputEventFilter;

namespace Wayland
{

class WaylandBackend;
class WaylandSeat;
class WaylandOutput;
class WaylandEglBackend;

class WaylandCursor
{
public:
    explicit WaylandCursor(WaylandBackend *backend);
    virtual ~WaylandCursor();

    virtual void init();
    virtual void move(const QPointF &globalPosition)
    {
    }

    void installImage();

protected:
    void resetSurface();
    virtual void doInstallImage(wl_buffer *image, const QSize &size, qreal scale);
    void drawSurface(wl_buffer *image, const QSize &size, qreal scale);

    KWayland::Client::Surface *surface() const
    {
        return m_surface.get();
    }
    WaylandBackend *backend() const
    {
        return m_backend;
    }

private:
    WaylandBackend *const m_backend;
    std::unique_ptr<KWayland::Client::Surface> m_surface;
};

class WaylandSubSurfaceCursor : public WaylandCursor
{
public:
    explicit WaylandSubSurfaceCursor(WaylandBackend *backend);
    ~WaylandSubSurfaceCursor() override;

    void init() override;

    void move(const QPointF &globalPosition) override;

private:
    void changeOutput(WaylandOutput *output);
    void doInstallImage(wl_buffer *image, const QSize &size, qreal scale) override;
    void createSubSurface();

    QPointF absoluteToRelativePosition(const QPointF &position);
    WaylandOutput *m_output = nullptr;
    std::unique_ptr<KWayland::Client::SubSurface> m_subSurface;
};

class WaylandInputDevice : public InputDevice
{
    Q_OBJECT

public:
    WaylandInputDevice(KWayland::Client::Touch *touch, WaylandSeat *seat);
    WaylandInputDevice(KWayland::Client::Keyboard *keyboard, WaylandSeat *seat);
    WaylandInputDevice(KWayland::Client::RelativePointer *relativePointer, WaylandSeat *seat);
    WaylandInputDevice(KWayland::Client::Pointer *pointer, WaylandSeat *seat);
    ~WaylandInputDevice() override;

    QString sysName() const override;
    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    LEDs leds() const override;
    void setLeds(LEDs leds) override;

    bool isKeyboard() const override;
    bool isAlphaNumericKeyboard() const override;
    bool isPointer() const override;
    bool isTouchpad() const override;
    bool isTouch() const override;
    bool isTabletTool() const override;
    bool isTabletPad() const override;
    bool isTabletModeSwitch() const override;
    bool isLidSwitch() const override;

    KWayland::Client::Pointer *nativePointer() const;

private:
    WaylandSeat *const m_seat;

    std::unique_ptr<KWayland::Client::Keyboard> m_keyboard;
    std::unique_ptr<KWayland::Client::Touch> m_touch;
    std::unique_ptr<KWayland::Client::RelativePointer> m_relativePointer;
    std::unique_ptr<KWayland::Client::Pointer> m_pointer;
    std::unique_ptr<KWayland::Client::PointerPinchGesture> m_pinchGesture;
    std::unique_ptr<KWayland::Client::PointerSwipeGesture> m_swipeGesture;

    uint32_t m_enteredSerial = 0;
};

class WaylandInputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit WaylandInputBackend(WaylandBackend *backend, QObject *parent = nullptr);

    void initialize() override;

private:
    void checkSeat();

    WaylandBackend *m_backend;
};

class WaylandSeat : public QObject
{
    Q_OBJECT
public:
    WaylandSeat(KWayland::Client::Seat *nativeSeat, WaylandBackend *backend);
    ~WaylandSeat() override;

    WaylandBackend *backend() const
    {
        return m_backend;
    }

    WaylandInputDevice *pointerDevice() const
    {
        return m_pointerDevice.get();
    }
    WaylandInputDevice *relativePointerDevice() const
    {
        return m_relativePointerDevice.get();
    }
    WaylandInputDevice *keyboardDevice() const
    {
        return m_keyboardDevice.get();
    }
    WaylandInputDevice *touchDevice() const
    {
        return m_touchDevice.get();
    }

    void createRelativePointer();
    void destroyRelativePointer();

Q_SIGNALS:
    void deviceAdded(WaylandInputDevice *device);
    void deviceRemoved(WaylandInputDevice *device);

private:
    void createPointerDevice();
    void destroyPointerDevice();
    void createKeyboardDevice();
    void destroyKeyboardDevice();
    void createTouchDevice();
    void destroyTouchDevice();

    KWayland::Client::Seat *m_seat;
    WaylandBackend *m_backend;

    std::unique_ptr<WaylandInputDevice> m_pointerDevice;
    std::unique_ptr<WaylandInputDevice> m_relativePointerDevice;
    std::unique_ptr<WaylandInputDevice> m_keyboardDevice;
    std::unique_ptr<WaylandInputDevice> m_touchDevice;
};

/**
 * @brief Class encapsulating all Wayland data structures needed by the Egl backend.
 *
 * It creates the connection to the Wayland Compositor, sets up the registry and creates
 * the Wayland output surfaces and its shell mappings.
 */
class KWIN_EXPORT WaylandBackend : public OutputBackend
{
    Q_OBJECT

public:
    explicit WaylandBackend(QObject *parent = nullptr);
    ~WaylandBackend() override;
    bool initialize() override;
    wl_display *display();
    KWayland::Client::Compositor *compositor();
    KWayland::Client::SubCompositor *subCompositor();
    KWayland::Client::ShmPool *shmPool();

    std::unique_ptr<InputBackend> createInputBackend() override;
    std::unique_ptr<OpenGLBackend> createOpenGLBackend() override;
    std::unique_ptr<QPainterBackend> createQPainterBackend() override;

    void flush();

    WaylandSeat *seat() const
    {
        return m_seat.get();
    }
    KWayland::Client::PointerGestures *pointerGestures() const
    {
        return m_pointerGestures;
    }
    KWayland::Client::PointerConstraints *pointerConstraints() const
    {
        return m_pointerConstraints;
    }
    KWayland::Client::RelativePointerManager *relativePointerManager() const
    {
        return m_relativePointerManager;
    }

    bool supportsPointerLock();
    void togglePointerLock();
    bool pointerIsLocked();

    QVector<CompositingType> supportedCompositors() const override;

    WaylandOutput *getOutputAt(const QPointF &globalPosition);
    WaylandOutput *findOutput(KWayland::Client::Surface *nativeSurface) const;
    Outputs outputs() const override;
    QVector<WaylandOutput *> waylandOutputs() const
    {
        return m_outputs;
    }
    void addConfiguredOutput(WaylandOutput *output);
    void createDpmsFilter();
    void clearDpmsFilter();

    Output *createVirtualOutput(const QString &name, const QSize &size, double scale) override;
    void removeVirtualOutput(Output *output) override;

    std::optional<DmaBufParams> testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers) override;
    std::shared_ptr<DmaBufTexture> createDmaBufTexture(const QSize &size, quint32 format, uint64_t modifier) override;

    gbm_device *gbmDevice() const
    {
        return m_gbmDevice;
    }

    void setEglBackend(WaylandEglBackend *eglBackend)
    {
        m_eglBackend = eglBackend;
    }

Q_SIGNALS:
    void systemCompositorDied();
    void connectionFailed();

    void seatCreated();
    void pointerLockSupportedChanged();
    void pointerLockChanged(bool locked);

private:
    void initConnection();
    void createOutputs();
    void destroyOutputs();

    WaylandOutput *createOutput(const QString &name, const QSize &size);

    wl_display *m_display;
    std::unique_ptr<KWayland::Client::EventQueue> m_eventQueue;
    std::unique_ptr<KWayland::Client::Registry> m_registry;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::SubCompositor> m_subCompositor;
    std::unique_ptr<KWayland::Client::XdgShell> m_xdgShell;
    std::unique_ptr<KWayland::Client::ShmPool> m_shm;
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connectionThreadObject;

    std::unique_ptr<WaylandSeat> m_seat;
    KWayland::Client::RelativePointerManager *m_relativePointerManager = nullptr;
    KWayland::Client::PointerConstraints *m_pointerConstraints = nullptr;
    KWayland::Client::PointerGestures *m_pointerGestures = nullptr;
    WaylandEglBackend *m_eglBackend = nullptr;

    std::unique_ptr<QThread> m_connectionThread;
    QVector<WaylandOutput *> m_outputs;
    int m_pendingInitialOutputs = 0;

    std::unique_ptr<WaylandCursor> m_waylandCursor;

    std::unique_ptr<DpmsInputEventFilter> m_dpmsFilter;

    bool m_pointerLockRequested = false;
    KWayland::Client::ServerSideDecorationManager *m_ssdManager = nullptr;
    KWayland::Client::ServerSideDecorationManager *ssdManager();
    int m_nextId = 0;
#if HAVE_WAYLAND_EGL
    FileDescriptor m_drmFileDescriptor;
    gbm_device *m_gbmDevice;
#endif
};

inline wl_display *WaylandBackend::display()
{
    return m_display;
}

inline KWayland::Client::Compositor *WaylandBackend::compositor()
{
    return m_compositor.get();
}

inline KWayland::Client::SubCompositor *WaylandBackend::subCompositor()
{
    return m_subCompositor.get();
}

inline KWayland::Client::ShmPool *WaylandBackend::shmPool()
{
    return m_shm.get();
}

} // namespace Wayland
} // namespace KWin
