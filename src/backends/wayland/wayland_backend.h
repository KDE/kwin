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

struct wl_display;
struct gbm_device;
struct gbm_bo;

namespace KWayland
{
namespace Client
{
class Keyboard;
class Pointer;
class PointerSwipeGesture;
class PointerPinchGesture;
class RelativePointer;
class Seat;
class Surface;
class Touch;
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
class WaylandDisplay;

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
};

class WaylandInputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit WaylandInputBackend(WaylandBackend *backend, QObject *parent = nullptr);

    void initialize() override;

private:
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

struct WaylandBackendOptions
{
    QString socketName;
    int outputCount = 1;
    qreal outputScale = 1;
    QSize outputSize = QSize(1024, 768);
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
    explicit WaylandBackend(const WaylandBackendOptions &options, QObject *parent = nullptr);
    ~WaylandBackend() override;
    bool initialize() override;

    std::unique_ptr<InputBackend> createInputBackend() override;
    std::unique_ptr<OpenGLBackend> createOpenGLBackend() override;
    std::unique_ptr<QPainterBackend> createQPainterBackend() override;

    WaylandDisplay *display() const
    {
        return m_display.get();
    }
    WaylandSeat *seat() const
    {
        return m_seat.get();
    }

    bool supportsPointerLock();
    void togglePointerLock();

    QVector<CompositingType> supportedCompositors() const override;

    WaylandOutput *findOutput(KWayland::Client::Surface *nativeSurface) const;
    Outputs outputs() const override;
    QVector<WaylandOutput *> waylandOutputs() const
    {
        return m_outputs;
    }
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
    void pointerLockChanged(bool locked);

private:
    void createOutputs();
    void destroyOutputs();
    WaylandOutput *createOutput(const QString &name, const QSize &size, qreal scale);

    WaylandBackendOptions m_options;
    std::unique_ptr<WaylandDisplay> m_display;
    std::unique_ptr<WaylandSeat> m_seat;
    WaylandEglBackend *m_eglBackend = nullptr;
    QVector<WaylandOutput *> m_outputs;
    std::unique_ptr<DpmsInputEventFilter> m_dpmsFilter;
    bool m_pointerLockRequested = false;
    FileDescriptor m_drmFileDescriptor;
    gbm_device *m_gbmDevice;
};

} // namespace Wayland
} // namespace KWin
