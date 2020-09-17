/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_WAYLAND_BACKEND_H
#define KWIN_WAYLAND_BACKEND_H
// KWin
#include "platform.h"
#include <config-kwin.h>
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
class SubCompositor;
class SubSurface;
class Surface;
class Touch;
class XdgShell;
}
}

namespace KWin
{
namespace Wayland
{

class WaylandBackend;
class WaylandSeat;
class WaylandOutput;

class WaylandCursor : public QObject
{
    Q_OBJECT
public:
    explicit WaylandCursor(WaylandBackend *backend);
    ~WaylandCursor() override;

    virtual void init();
    virtual void move(const QPointF &globalPosition) {
        Q_UNUSED(globalPosition)
    }

    void installImage();

protected:
    void resetSurface();
    virtual void doInstallImage(wl_buffer *image, const QSize &size);
    void drawSurface(wl_buffer *image, const QSize &size);

    KWayland::Client::Surface *surface() const {
        return m_surface;
    }
    WaylandBackend *backend() const {
        return m_backend;
    }

private:
    WaylandBackend *m_backend;
    KWayland::Client::Pointer *m_pointer;
    KWayland::Client::Surface *m_surface = nullptr;
};

class WaylandSubSurfaceCursor : public WaylandCursor
{
    Q_OBJECT
public:
    explicit WaylandSubSurfaceCursor(WaylandBackend *backend);
    ~WaylandSubSurfaceCursor() override;

    void init() override;

    void move(const QPointF &globalPosition) override;

private:
    void changeOutput(WaylandOutput *output);
    void doInstallImage(wl_buffer *image, const QSize &size) override;
    void createSubSurface();

    QPointF absoluteToRelativePosition(const QPointF &position);
    WaylandOutput *m_output = nullptr;
    KWayland::Client::SubSurface *m_subSurface = nullptr;
};

class WaylandSeat : public QObject
{
    Q_OBJECT
public:
    WaylandSeat(wl_seat *seat, WaylandBackend *backend);
    ~WaylandSeat() override;

    KWayland::Client::Pointer *pointer() const {
        return m_pointer;
    }

    void installGesturesInterface(KWayland::Client::PointerGestures *gesturesInterface) {
        m_gesturesInterface = gesturesInterface;
        setupPointerGestures();
    }

private:
    void destroyPointer();
    void destroyKeyboard();
    void destroyTouch();
    void setupPointerGestures();

    KWayland::Client::Seat *m_seat;
    KWayland::Client::Pointer *m_pointer;
    KWayland::Client::Keyboard *m_keyboard;
    KWayland::Client::Touch *m_touch;
    KWayland::Client::PointerGestures *m_gesturesInterface = nullptr;
    KWayland::Client::PointerPinchGesture *m_pinchGesture = nullptr;
    KWayland::Client::PointerSwipeGesture *m_swipeGesture = nullptr;

    uint32_t m_enteredSerial;

    WaylandBackend *m_backend;
};

/**
* @brief Class encapsulating all Wayland data structures needed by the Egl backend.
*
* It creates the connection to the Wayland Compositor, sets up the registry and creates
* the Wayland output surfaces and its shell mappings.
*/
class KWIN_EXPORT WaylandBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "wayland.json")
public:
    explicit WaylandBackend(QObject *parent = nullptr);
    ~WaylandBackend() override;
    void init() override;
    wl_display *display();
    KWayland::Client::Compositor *compositor();
    KWayland::Client::SubCompositor *subCompositor();
    KWayland::Client::ShmPool *shmPool();

    Screens *createScreens(QObject *parent = nullptr) override;
    OpenGLBackend *createOpenGLBackend() override;
    QPainterBackend *createQPainterBackend() override;
    DmaBufTexture *createDmaBufTexture(const QSize &size) override;

    void flush();

    WaylandSeat *seat() const {
        return m_seat;
    }
    KWayland::Client::PointerConstraints *pointerConstraints() const {
        return m_pointerConstraints;
    }

    void pointerMotionRelativeToOutput(const QPointF &position, quint32 time);

    bool supportsPointerLock();
    void togglePointerLock();
    bool pointerIsLocked();

    QVector<CompositingType> supportedCompositors() const override;

    void checkBufferSwap();

    WaylandOutput* getOutputAt(const QPointF &globalPosition);
    Outputs outputs() const override;
    Outputs enabledOutputs() const override;
    QVector<WaylandOutput*> waylandOutputs() const {
        return m_outputs;
    }

Q_SIGNALS:
    void outputAdded(WaylandOutput *output);
    void outputRemoved(WaylandOutput *output);

    void systemCompositorDied();
    void connectionFailed();

    void pointerLockSupportedChanged();
    void pointerLockChanged(bool locked);

private:
    void initConnection();
    void createOutputs();

    void updateScreenSize(WaylandOutput *output);
    void relativeMotionHandler(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 timestamp);

    wl_display *m_display;
    KWayland::Client::EventQueue *m_eventQueue;
    KWayland::Client::Registry *m_registry;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::SubCompositor *m_subCompositor;
    KWayland::Client::XdgShell *m_xdgShell = nullptr;
    KWayland::Client::ShmPool *m_shm;
    KWayland::Client::ConnectionThread *m_connectionThreadObject;

    WaylandSeat *m_seat = nullptr;
    KWayland::Client::RelativePointer *m_relativePointer = nullptr;
    KWayland::Client::RelativePointerManager *m_relativePointerManager = nullptr;
    KWayland::Client::PointerConstraints *m_pointerConstraints = nullptr;

    QThread *m_connectionThread;
    QVector<WaylandOutput*> m_outputs;

    WaylandCursor *m_waylandCursor = nullptr;

    bool m_pointerLockRequested = false;
    int m_drmFileDescriptor = 0;
    gbm_device *m_gbmDevice;
};

inline
wl_display *WaylandBackend::display()
{
    return m_display;
}

inline
KWayland::Client::Compositor *WaylandBackend::compositor()
{
    return m_compositor;
}

inline
KWayland::Client::SubCompositor *WaylandBackend::subCompositor()
{
    return m_subCompositor;
}

inline
KWayland::Client::ShmPool* WaylandBackend::shmPool()
{
    return m_shm;
}

} // namespace Wayland
} // namespace KWin

#endif //  KWIN_WAYLAND_BACKEND_H
