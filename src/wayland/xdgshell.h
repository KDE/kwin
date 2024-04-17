/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QSharedDataPointer>

#include <chrono>
#include <memory>

struct wl_resource;

namespace KWin
{
class Display;
class OutputInterface;
class SeatInterface;
class SurfaceInterface;
class SurfaceRole;
class XdgShellInterfacePrivate;
class XdgSurfaceInterfacePrivate;
class XdgToplevelInterfacePrivate;
class XdgPopupInterfacePrivate;
class XdgPositionerData;
class XdgToplevelInterface;
class XdgPopupInterface;
class XdgSurfaceInterface;

/**
 * The XdgShellInterface class represents an extension for destrop-style user interfaces.
 *
 * The XdgShellInterface class provides a way for a client to extend a regular Wayland surface
 * with functionality required to construct user interface elements, e.g. toplevel windows or
 * menus.
 *
 * XdgShellInterface corresponds to the WaylandInterface \c xdg_wm_base.
 */
class KWIN_EXPORT XdgShellInterface : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs an XdgShellInterface object with the given wayland display \a display.
     */
    XdgShellInterface(Display *display, QObject *parent = nullptr);
    /**
     * Destructs the XdgShellInterface object.
     */
    ~XdgShellInterface() override;

    /**
     * Returns the wayland display of the XdgShellInterface.
     */
    Display *display() const;

    /**
     * Sends a ping event to the client with the given xdg-surface \a surface. If the client
     * replies to the event within a reasonable amount of time, pongReceived signal will be
     * emitted.
     */
    quint32 ping(XdgSurfaceInterface *surface);

    /**
     * Returns the ping timeout.
     */
    std::chrono::milliseconds pingTimeoutInterval() const;

    /**
     * Set the ping timeout.
     */
    void setPingTimeoutInterval(std::chrono::milliseconds pingTimeout);

Q_SIGNALS:
    /**
     * This signal is emitted when a new XdgToplevelInterface object is created.
     */
    void toplevelCreated(XdgToplevelInterface *toplevel);

    /**
     * This signal is emitted when a new XdgPopupInterface object is created.
     */
    void popupCreated(XdgPopupInterface *popup);

    /**
     * This signal is emitted when the client has responded to a ping event with serial \a serial.
     */
    void pongReceived(quint32 serial);

    /**
     * @todo Drop this signal.
     *
     * This signal is emitted when the client has not responded to a ping event with serial
     * \a serial within a reasonable amount of time and the compositor gave up on it.
     */
    void pingTimeout(quint32 serial);

    /**
     * This signal is emitted when the client has not responded to a ping event with serial
     * \a serial within a reasonable amount of time.
     */
    void pingDelayed(quint32 serial);

private:
    std::unique_ptr<XdgShellInterfacePrivate> d;
    friend class XdgShellInterfacePrivate;
};

/**
 * The XdgSurfaceInterface class provides a base set of functionality required to construct
 * user interface elements.
 *
 * XdgSurfaceInterface corresponds to the Wayland interface \c xdg_surface.
 */
class KWIN_EXPORT XdgSurfaceInterface : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs an XdgSurfaceInterface for the given \a shell and \a surface.
     */
    XdgSurfaceInterface(XdgShellInterface *shell, SurfaceInterface *surface, ::wl_resource *resource);
    /**
     * Destructs the XdgSurfaceInterface object.
     */
    ~XdgSurfaceInterface() override;

    /**
     * Returns the XdgToplevelInterface associated with this XdgSurfaceInterface.
     *
     * This method will return \c null if no xdg_toplevel object is associated with this surface.
     */
    XdgToplevelInterface *toplevel() const;

    /**
     * Returns the XdgPopupInterface associated with this XdgSurfaceInterface.
     *
     * This method will return \c null if no xdg_popup object is associated with this surface.
     */
    XdgPopupInterface *popup() const;

    /**
     * Returns the XdgShellInterface associated with this XdgSurfaceInterface.
     */
    XdgShellInterface *shell() const;

    /**
     * Returns the SurfaceInterface assigned to this XdgSurfaceInterface.
     */
    SurfaceInterface *surface() const;

    /**
     * Returns \c true if the surface has been configured; otherwise returns \c false.
     */
    bool isConfigured() const;

    /**
     * Returns the window geometry of the XdgSurfaceInterface.
     *
     * This method will return an invalid QRect if the window geometry is not set by the client.
     */
    QRect windowGeometry() const;

    /**
     * Returns the XdgSurfaceInterface for the specified wayland resource object \a resource.
     */
    static XdgSurfaceInterface *get(::wl_resource *resource);

Q_SIGNALS:
    /**
     * This signal is emitted when the xdg-surface is about to be destroyed.
     */
    void aboutToBeDestroyed();

    /**
     * This signal is emitted when a configure event with serial \a serial has been acknowledged.
     */
    void configureAcknowledged(quint32 serial);

    /**
     * This signal is emitted when the window geometry has been changed.
     */
    void windowGeometryChanged(const QRect &rect);

    /**
     * This signal is emitted when the surface has been unmapped and its state has been reset.
     */
    void resetOccurred();

private:
    std::unique_ptr<XdgSurfaceInterfacePrivate> d;
    friend class XdgSurfaceInterfacePrivate;
};

/**
 * The XdgToplevelInterface class represents a surface with window-like functionality such
 * as maximize, fullscreen, resizing, minimizing, etc.
 *
 * XdgToplevelInterface corresponds to the Wayland interface \c xdg_toplevel.
 */
class KWIN_EXPORT XdgToplevelInterface : public QObject
{
    Q_OBJECT

public:
    enum State {
        MaximizedHorizontal = 0x1,
        MaximizedVertical = 0x2,
        FullScreen = 0x4,
        Resizing = 0x8,
        Activated = 0x10,
        TiledLeft = 0x20,
        TiledTop = 0x40,
        TiledRight = 0x80,
        TiledBottom = 0x100,
        Suspended = 0x200,
        Maximized = MaximizedHorizontal | MaximizedVertical,
    };
    Q_DECLARE_FLAGS(States, State)

    enum class ResizeAnchor {
        None = 0,
        Top = 1,
        Bottom = 2,
        Left = 4,
        TopLeft = 5,
        BottomLeft = 6,
        Right = 8,
        TopRight = 9,
        BottomRight = 10,
    };
    Q_ENUM(ResizeAnchor)

    enum class Capability {
        WindowMenu = 0x1,
        Maximize = 0x2,
        FullScreen = 0x4,
        Minimize = 0x8,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    /**
     * Constructs an XdgToplevelInterface for the given xdg-surface \a surface.
     */
    XdgToplevelInterface(XdgSurfaceInterface *surface, ::wl_resource *resource);
    /**
     * Destructs the XdgToplevelInterface object.
     */
    ~XdgToplevelInterface() override;

    static SurfaceRole *role();

    /**
     * Returns the XdgShellInterface for this XdgToplevelInterface.
     *
     * This is equivalent to xdgSurface()->shell().
     */
    XdgShellInterface *shell() const;

    /**
     * Returns the XdgSurfaceInterface associated with the XdgToplevelInterface.
     */
    XdgSurfaceInterface *xdgSurface() const;

    /**
     * Returns the SurfaceInterface associated with the XdgToplevelInterface.
     */
    SurfaceInterface *surface() const;

    /**
     * Returns the parent XdgToplevelInterface above which this toplevel is stacked.
     */
    XdgToplevelInterface *parentXdgToplevel() const;

    /**
     * Returns \c true if the toplevel has been configured; otherwise returns \c false.
     */
    bool isConfigured() const;

    /**
     * Returns the window title of the toplevel surface.
     */
    QString windowTitle() const;

    /**
     * Returns the window class of the toplevel surface.
     */
    QString windowClass() const;

    /**
     * Returns the minimum window geometry size of the toplevel surface.
     */
    QSize minimumSize() const;

    /**
     * Returns the maximum window geometry size of the toplevel surface.
     */
    QSize maximumSize() const;

    QIcon customIcon() const;

    /**
     * Sends a configure event to the client. \a size specifies the new window geometry size. A size
     * of zero means the client should decide its own window dimensions.
     */
    quint32 sendConfigure(const QSize &size, const States &states);

    /**
     * Sends a close event to the client. The client may choose to ignore this request.
     */
    void sendClose();

    /**
     * Sends an event to the client specifying the maximum bounds for the surface size. Must be
     * called before sendConfigure().
     */
    void sendConfigureBounds(const QSize &size);

    /**
     * Sends an event to the client specifying allowed actions by the compositor.
     */
    void sendWmCapabilities(Capabilities capabilities);

    /**
     * Returns the XdgToplevelInterface for the specified wayland resource object \a resource.
     */
    static XdgToplevelInterface *get(::wl_resource *resource);

Q_SIGNALS:
    /**
     * This signal is emitted when the xdg-toplevel is about to be destroyed.
     */
    void aboutToBeDestroyed();

    /**
     * This signal is emitted when the xdg-toplevel has commited the initial state and wants to
     * be configured. After initializing the toplevel, you must send a configure event.
     */
    void initializeRequested();

    /**
     * This signal is emitted when the toplevel has been unmapped and its state has been reset.
     */
    void resetOccurred();

    /**
     * This signal is emitted when the toplevel's title has been changed.
     */
    void windowTitleChanged(const QString &windowTitle);

    /**
     * This signal is emitted when the toplevel's application id has been changed.
     */
    void windowClassChanged(const QString &windowClass);

    /**
     * This signal is emitted when the toplevel has requested the window menu to be shown at
     * \a pos. The \a seat and the \a serial indicate the user action that triggerred the request.
     */
    void windowMenuRequested(KWin::SeatInterface *seat, const QPoint &pos, quint32 serial);

    /**
     * This signal is emitted when the toplevel's minimum size has been changed.
     */
    void minimumSizeChanged(const QSize &size);

    /**
     * This signal is emitted when the toplevel's maximum size has been changed.
     */
    void maximumSizeChanged(const QSize &size);

    void customIconChanged();

    /**
     * This signal is emitted when the toplevel wants to be interactively moved. The \a seat and
     * the \a serial indicate the user action in response to which this request has been issued.
     */
    void moveRequested(KWin::SeatInterface *seat, quint32 serial);

    /**
     * This signal is emitted when the toplevel wants to be interactively resized by dragging
     * the specified \a anchor. The \a seat and the \a serial indicate the user action
     * in response to which this request has been issued.
     */
    void resizeRequested(KWin::SeatInterface *seat, KWin::XdgToplevelInterface::ResizeAnchor anchor, quint32 serial);

    /**
     * This signal is emitted when the toplevel surface wants to become maximized.
     */
    void maximizeRequested();

    /**
     * This signal is emitted when the toplevel surface wants to become unmaximized.
     */
    void unmaximizeRequested();

    /**
     * This signal is emitted when the toplevel wants to be shown in the full screen mode.
     */
    void fullscreenRequested(KWin::OutputInterface *output);

    /**
     * This signal is emitted when the toplevel surface wants to leave the full screen mode.
     */
    void unfullscreenRequested();

    /**
     * This signal is emitted when the toplevel wants to be iconified.
     */
    void minimizeRequested();

    /**
     * This signal is emitted when the parent toplevel has changed.
     */
    void parentXdgToplevelChanged();

private:
    std::unique_ptr<XdgToplevelInterfacePrivate> d;
    friend class XdgToplevelInterfacePrivate;
};

/**
 * The XdgPositioner class provides a collection of rules for the placement of a popup surface.
 *
 * XdgPositioner corresponds to the Wayland interface \c xdg_positioner.
 */
class KWIN_EXPORT XdgPositioner
{
public:
    /**
     * Constructs an incomplete XdgPositioner object.
     */
    XdgPositioner();
    /**
     * Constructs a copy of the XdgPositioner object.
     */
    XdgPositioner(const XdgPositioner &other);
    /**
     * Destructs the XdgPositioner object.
     */
    ~XdgPositioner();

    /**
     * Assigns the value of \a other to this XdgPositioner object.
     */
    XdgPositioner &operator=(const XdgPositioner &other);

    /**
     * Returns \c true if the positioner object is complete; otherwise returns \c false.
     *
     * An xdg positioner considered complete if it has a valid size and a valid anchor rect.
     */
    bool isComplete() const;

    /**
     * Returns the window geometry size of the surface that is to be positioned.
     */
    QSize size() const;

    /**
     * Returns whether the surface should respond to movements in its parent window.
     */
    bool isReactive() const;

    /**
     * Returns the unconstrained geometry of the popup. The \a bounds is in the parent local
     * coordinate space.
     */
    QRectF placement(const QRectF &bounds) const;

    /**
     * Returns the current state of the xdg positioner object identified by \a resource.
     */
    static XdgPositioner get(::wl_resource *resource);

private:
    XdgPositioner(const QSharedDataPointer<XdgPositionerData> &data);
    QSharedDataPointer<XdgPositionerData> d;
};

/**
 * The XdgPopupInterface class represents a surface that can be used to implement context menus,
 * popovers and other similar short-lived user interface elements.
 *
 * XdgPopupInterface corresponds to the Wayland interface \c xdg_popup.
 */
class KWIN_EXPORT XdgPopupInterface : public QObject
{
    Q_OBJECT

public:
    XdgPopupInterface(XdgSurfaceInterface *surface, SurfaceInterface *parentSurface, const XdgPositioner &positioner, ::wl_resource *resource);
    /**
     * Destructs the XdgPopupInterface object.
     */
    ~XdgPopupInterface() override;

    static SurfaceRole *role();

    XdgShellInterface *shell() const;

    /**
     * Returns the parent surface for this popup surface. If the initial state hasn't been
     * committed yet, this function may return \c null.
     */
    SurfaceInterface *parentSurface() const;

    /**
     * Returns the XdgSurfaceInterface associated with the XdgPopupInterface.
     */
    XdgSurfaceInterface *xdgSurface() const;

    /**
     * Returns the SurfaceInterface associated with the XdgPopupInterface.
     */
    SurfaceInterface *surface() const;

    /**
     * Returns the XdgPositioner assigned to this XdgPopupInterface.
     */
    XdgPositioner positioner() const;

    /**
     * Returns \c true if the popup has been configured; otherwise returns \c false.
     */
    bool isConfigured() const;

    /**
     * Sends a configure event to the client and returns the serial number of the event.
     */
    quint32 sendConfigure(const QRect &rect);

    /**
     * Sends a popup done event to the client.
     */
    void sendPopupDone();

    /**
     * Sends a popup repositioned event to the client.
     */
    void sendRepositioned(quint32 token);

    /**
     * Returns the XdgPopupInterface for the specified wayland resource object \a resource.
     */
    static XdgPopupInterface *get(::wl_resource *resource);

Q_SIGNALS:
    /**
     * This signal is emitted when the xdg-popup is about to be destroyed.
     */
    void aboutToBeDestroyed();

    /**
     * This signal is emitted when the xdg-popup has commited the initial state and wants to
     * be configured. After initializing the popup, you must send a configure event.
     */
    void initializeRequested();
    void grabRequested(SeatInterface *seat, quint32 serial);
    void repositionRequested(quint32 token);

private:
    std::unique_ptr<XdgPopupInterfacePrivate> d;
    friend class XdgPopupInterfacePrivate;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::XdgToplevelInterface::States)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::XdgToplevelInterface::Capabilities)
Q_DECLARE_METATYPE(KWin::XdgToplevelInterface::State)
Q_DECLARE_METATYPE(KWin::XdgToplevelInterface::States)
