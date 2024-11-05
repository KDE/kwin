/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgshell.h"
#include "xdgshell_p.h"

#include "display.h"
#include "output.h"
#include "seat.h"
#include "utils/resource.h"

#include <QTimer>

namespace KWin
{
static const int s_version = 6;

XdgShellInterfacePrivate::XdgShellInterfacePrivate(XdgShellInterface *shell)
    : q(shell)
{
}

XdgShellInterfacePrivate::Resource *XdgShellInterfacePrivate::resourceForXdgSurface(XdgSurfaceInterface *surface) const
{
    return xdgSurfaces.value(surface);
}

void XdgShellInterfacePrivate::unregisterXdgSurface(XdgSurfaceInterface *surface)
{
    xdgSurfaces.remove(surface);
}

/**
 * @todo Whether the ping is delayed or has timed out is out of domain of the XdgShellInterface.
 * Such matter must be handled somewhere else, e.g. XdgToplevelWindow, not here!
 */
void XdgShellInterfacePrivate::registerPing(quint32 serial)
{
    QTimer *timer = new QTimer(q);
    // we'll run the timer twice.
    timer->setInterval(pingTimeout / 2);
    QObject::connect(timer, &QTimer::timeout, q, [this, serial, attempt = 0]() mutable {
        ++attempt;
        if (attempt == 1) {
            Q_EMIT q->pingDelayed(serial);
            return;
        }
        Q_EMIT q->pingTimeout(serial);
        delete pings.take(serial);
    });
    pings.insert(serial, timer);
    timer->start();
}

XdgShellInterfacePrivate *XdgShellInterfacePrivate::get(XdgShellInterface *shell)
{
    return shell->d.get();
}

void XdgShellInterfacePrivate::xdg_wm_base_destroy_resource(Resource *resource)
{
    const QList<XdgSurfaceInterface *> surfaces = xdgSurfaces.keys(resource);
    qDeleteAll(surfaces);
}

void XdgShellInterfacePrivate::xdg_wm_base_destroy(Resource *resource)
{
    if (xdgSurfaces.key(resource)) {
        wl_resource_post_error(resource->handle, error_defunct_surfaces, "xdg_wm_base was destroyed before children");
        return;
    }
    wl_resource_destroy(resource->handle);
}

void XdgShellInterfacePrivate::xdg_wm_base_create_positioner(Resource *resource, uint32_t id)
{
    wl_resource *positionerResource = wl_resource_create(resource->client(), &xdg_positioner_interface, resource->version(), id);
    new XdgPositionerPrivate(positionerResource);
}

void XdgShellInterfacePrivate::xdg_wm_base_get_xdg_surface(Resource *resource, uint32_t id, ::wl_resource *surfaceResource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surfaceResource);

    if (surface->buffer()) {
        wl_resource_post_error(resource->handle, XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER, "xdg_surface must not have a buffer at creation");
        return;
    }

    wl_resource *xdgSurfaceResource = wl_resource_create(resource->client(), &xdg_surface_interface, resource->version(), id);

    XdgSurfaceInterface *xdgSurface = new XdgSurfaceInterface(q, surface, xdgSurfaceResource);
    xdgSurfaces.insert(xdgSurface, resource);
}

void XdgShellInterfacePrivate::xdg_wm_base_pong(Resource *resource, uint32_t serial)
{
    if (QTimer *timer = pings.take(serial)) {
        delete timer;
    }
    Q_EMIT q->pongReceived(serial);
}

XdgShellInterface::XdgShellInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new XdgShellInterfacePrivate(this))
{
    d->display = display;
    d->init(*display, s_version);
}

XdgShellInterface::~XdgShellInterface()
{
}

Display *XdgShellInterface::display() const
{
    return d->display;
}

quint32 XdgShellInterface::ping(XdgSurfaceInterface *surface)
{
    XdgShellInterfacePrivate::Resource *clientResource = d->resourceForXdgSurface(surface);
    if (!clientResource)
        return 0;

    quint32 serial = d->display->nextSerial();
    d->send_ping(clientResource->handle, serial);
    d->registerPing(serial);

    return serial;
}

std::chrono::milliseconds XdgShellInterface::pingTimeoutInterval() const
{
    return d->pingTimeout;
}

void XdgShellInterface::setPingTimeoutInterval(std::chrono::milliseconds pingTimeout)
{
    d->pingTimeout = pingTimeout;
}

XdgSurfaceInterfacePrivate::XdgSurfaceInterfacePrivate(XdgSurfaceInterface *xdgSurface)
    : q(xdgSurface)
{
}

void XdgSurfaceInterfacePrivate::apply(XdgSurfaceCommit *commit)
{
    if (surface->buffer()) {
        firstBufferAttached = true;
    }

    if (commit->acknowledgedConfigure.has_value()) {
        Q_EMIT q->configureAcknowledged(commit->acknowledgedConfigure.value());
    }

    if (commit->windowGeometry.has_value()) {
        windowGeometry = commit->windowGeometry.value();
        Q_EMIT q->windowGeometryChanged(windowGeometry);
    }
}

void XdgSurfaceInterfacePrivate::reset()
{
    firstBufferAttached = false;
    isConfigured = false;
    isInitialized = false;
    windowGeometry = QRect();
    Q_EMIT q->resetOccurred();
}

XdgSurfaceInterfacePrivate *XdgSurfaceInterfacePrivate::get(XdgSurfaceInterface *surface)
{
    return surface->d.get();
}

void XdgSurfaceInterfacePrivate::xdg_surface_destroy_resource(Resource *resource)
{
    delete q;
}

void XdgSurfaceInterfacePrivate::xdg_surface_destroy(Resource *resource)
{
    if (toplevel || popup) {
        qWarning() << "Tried to destroy xdg_surface before its role object";
    }
    wl_resource_destroy(resource->handle);
}

void XdgSurfaceInterfacePrivate::xdg_surface_get_toplevel(Resource *resource, uint32_t id)
{
    if (const SurfaceRole *role = surface->role()) {
        if (role != XdgToplevelInterface::role()) {
            wl_resource_post_error(resource->handle, error_already_constructed, "the surface already has a role assigned %s", role->name().constData());
            return;
        }
    } else {
        surface->setRole(XdgToplevelInterface::role());
    }

    wl_resource *toplevelResource = wl_resource_create(resource->client(), &xdg_toplevel_interface, resource->version(), id);

    auto toplevel = new XdgToplevelInterface(q, toplevelResource);
    Q_EMIT shell->toplevelCreated(toplevel);
}

void XdgSurfaceInterfacePrivate::xdg_surface_get_popup(Resource *resource, uint32_t id, ::wl_resource *parentResource, ::wl_resource *positionerResource)
{
    if (const SurfaceRole *role = surface->role()) {
        if (role != XdgPopupInterface::role()) {
            wl_resource_post_error(resource->handle, error_already_constructed, "the surface already has a role assigned %s", role->name().constData());
            return;
        }
    } else {
        surface->setRole(XdgPopupInterface::role());
    }

    XdgPositioner positioner = XdgPositioner::get(positionerResource);
    if (!positioner.isComplete()) {
        auto shellPrivate = XdgShellInterfacePrivate::get(shell);
        wl_resource_post_error(shellPrivate->resourceForXdgSurface(q)->handle,
                               QtWaylandServer::xdg_wm_base::error_invalid_positioner,
                               "xdg_positioner is incomplete");
        return;
    }

    XdgSurfaceInterface *parentXdgSurface = XdgSurfaceInterface::get(parentResource);
    SurfaceInterface *parentSurface = nullptr;
    if (parentXdgSurface) {
        if (!parentXdgSurface->surface()->role()) {
            auto shellPrivate = XdgShellInterfacePrivate::get(shell);
            wl_resource_post_error(shellPrivate->resourceForXdgSurface(q)->handle,
                                   QtWaylandServer::xdg_wm_base::error_invalid_popup_parent,
                                   "parent surface has no surface role");
            return;
        }
        parentSurface = parentXdgSurface->surface();
    }

    wl_resource *popupResource = wl_resource_create(resource->client(), &xdg_popup_interface, resource->version(), id);

    auto popup = new XdgPopupInterface(q, parentSurface, positioner, popupResource);
    Q_EMIT shell->popupCreated(popup);
}

void XdgSurfaceInterfacePrivate::xdg_surface_set_window_geometry(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (Q_UNLIKELY(!pending)) {
        wl_resource_post_error(resource->handle, error_not_constructed, "xdg_surface must have a role");
        return;
    }

    if (width < 1 || height < 1) {
        wl_resource_post_error(resource->handle, -1, "invalid window geometry size (%dx%d)", width, height);
        return;
    }

    pending->windowGeometry = QRect(x, y, width, height);
}

void XdgSurfaceInterfacePrivate::xdg_surface_ack_configure(Resource *resource, uint32_t serial)
{
    if (Q_UNLIKELY(!pending)) {
        wl_resource_post_error(resource->handle, error_not_constructed, "xdg_surface must have a role");
        return;
    }

    pending->acknowledgedConfigure = serial;
}

XdgSurfaceInterface::XdgSurfaceInterface(XdgShellInterface *shell, SurfaceInterface *surface, ::wl_resource *resource)
    : d(new XdgSurfaceInterfacePrivate(this))
{
    d->shell = shell;
    d->surface = surface;
    d->init(resource);
}

XdgSurfaceInterface::~XdgSurfaceInterface()
{
    delete d->toplevel;
    delete d->popup;

    Q_EMIT aboutToBeDestroyed();
    XdgShellInterfacePrivate::get(d->shell)->unregisterXdgSurface(this);
}

XdgToplevelInterface *XdgSurfaceInterface::toplevel() const
{
    return d->toplevel;
}

XdgPopupInterface *XdgSurfaceInterface::popup() const
{
    return d->popup;
}

XdgShellInterface *XdgSurfaceInterface::shell() const
{
    return d->shell;
}

SurfaceInterface *XdgSurfaceInterface::surface() const
{
    return d->surface;
}

bool XdgSurfaceInterface::isConfigured() const
{
    return d->isConfigured;
}

QRect XdgSurfaceInterface::windowGeometry() const
{
    return d->windowGeometry;
}

XdgSurfaceInterface *XdgSurfaceInterface::get(::wl_resource *resource)
{
    if (auto surfacePrivate = resource_cast<XdgSurfaceInterfacePrivate *>(resource)) {
        return surfacePrivate->q;
    }
    return nullptr;
}

XdgToplevelInterfacePrivate::XdgToplevelInterfacePrivate(XdgToplevelInterface *toplevel, XdgSurfaceInterface *xdgSurface)
    : SurfaceExtension(xdgSurface->surface())
    , q(toplevel)
    , xdgSurface(xdgSurface)
{
}

void XdgToplevelInterfacePrivate::apply(XdgToplevelCommit *commit)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    if (xdgSurfacePrivate->firstBufferAttached && !xdgSurfacePrivate->surface->buffer()) {
        reset();
        return;
    }

    xdgSurfacePrivate->apply(commit);

    const auto minSize = commit->minimumSize.value_or(minimumSize);
    const auto maxSize = commit->maximumSize.value_or(maximumSize);
    if ((minSize.width() > 0) && (maxSize.width() > 0) && (minSize.width() > maxSize.width())) {
        wl_resource_post_error(resource()->handle, error_invalid_size, "minimum width can't be bigger than the maximum width");
        return;
    }
    if ((minSize.height() > 0) && (maxSize.height() > 0) && (minSize.height() > maxSize.height())) {
        wl_resource_post_error(resource()->handle, error_invalid_size, "minimum height can't be bigger than the maximum height");
        return;
    }

    if (commit->minimumSize && commit->minimumSize != minimumSize) {
        minimumSize = commit->minimumSize.value();
        Q_EMIT q->minimumSizeChanged(minimumSize);
    }
    if (commit->maximumSize && commit->maximumSize != maximumSize) {
        maximumSize = commit->maximumSize.value();
        Q_EMIT q->maximumSizeChanged(maximumSize);
    }

    if (!xdgSurfacePrivate->isInitialized) {
        Q_EMIT q->initializeRequested();
        xdgSurfacePrivate->isInitialized = true;
    }
}

void XdgToplevelInterfacePrivate::reset()
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    xdgSurfacePrivate->reset();

    windowTitle = QString();
    windowClass = QString();
    minimumSize = QSize(0, 0);
    maximumSize = QSize(0, 0);
    customIcon = QIcon();
    pending = XdgToplevelCommit{};
    stashed.clear();

    Q_EMIT q->resetOccurred();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_destroy_resource(Resource *resource)
{
    delete q;
}

void XdgToplevelInterfacePrivate::xdg_toplevel_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_parent(Resource *resource, ::wl_resource *parentResource)
{
    XdgToplevelInterface *parent = XdgToplevelInterface::get(parentResource);
    if (parentXdgToplevel == parent) {
        return;
    }
    parentXdgToplevel = parent;
    Q_EMIT q->parentXdgToplevelChanged();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_title(Resource *resource, const QString &title)
{
    if (windowTitle == title) {
        return;
    }
    windowTitle = title;
    Q_EMIT q->windowTitleChanged(title);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_app_id(Resource *resource, const QString &app_id)
{
    if (windowClass == app_id) {
        return;
    }
    windowClass = app_id;
    Q_EMIT q->windowClassChanged(app_id);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_show_window_menu(Resource *resource, ::wl_resource *seatResource, uint32_t serial, int32_t x, int32_t y)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);

    if (!xdgSurfacePrivate->isConfigured) {
        wl_resource_post_error(resource->handle, QtWaylandServer::xdg_surface::error_not_constructed, "surface has not been configured yet");
        return;
    }

    SeatInterface *seat = SeatInterface::get(seatResource);
    Q_EMIT q->windowMenuRequested(seat, QPoint(x, y), serial);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_move(Resource *resource, ::wl_resource *seatResource, uint32_t serial)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);

    if (!xdgSurfacePrivate->isConfigured) {
        wl_resource_post_error(resource->handle, QtWaylandServer::xdg_surface::error_not_constructed, "surface has not been configured yet");
        return;
    }

    SeatInterface *seat = SeatInterface::get(seatResource);
    Q_EMIT q->moveRequested(seat, serial);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_resize(Resource *resource, ::wl_resource *seatResource, uint32_t serial, uint32_t xdgEdges)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);

    if (!xdgSurfacePrivate->isConfigured) {
        wl_resource_post_error(resource->handle, QtWaylandServer::xdg_surface::error_not_constructed, "surface has not been configured yet");
        return;
    }

    SeatInterface *seat = SeatInterface::get(seatResource);
    Q_EMIT q->resizeRequested(seat, XdgToplevelInterface::ResizeAnchor(xdgEdges), serial);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_max_size(Resource *resource, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle, error_invalid_size, "width and height must be positive or zero");
        return;
    }
    pending.maximumSize = QSize(width, height);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_min_size(Resource *resource, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle, error_invalid_size, "width and height must be positive or zero");
        return;
    }
    pending.minimumSize = QSize(width, height);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_maximized(Resource *resource)
{
    Q_EMIT q->maximizeRequested();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_unset_maximized(Resource *resource)
{
    Q_EMIT q->unmaximizeRequested();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_fullscreen(Resource *resource, ::wl_resource *outputResource)
{
    OutputInterface *output = OutputInterface::get(outputResource);
    Q_EMIT q->fullscreenRequested(output);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_unset_fullscreen(Resource *resource)
{
    Q_EMIT q->unfullscreenRequested();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_minimized(Resource *resource)
{
    Q_EMIT q->minimizeRequested();
}

XdgToplevelInterfacePrivate *XdgToplevelInterfacePrivate::get(XdgToplevelInterface *toplevel)
{
    return toplevel->d.get();
}

XdgToplevelInterfacePrivate *XdgToplevelInterfacePrivate::get(wl_resource *resource)
{
    return resource_cast<XdgToplevelInterfacePrivate *>(resource);
}

XdgToplevelInterface::XdgToplevelInterface(XdgSurfaceInterface *xdgSurface, ::wl_resource *resource)
    : d(new XdgToplevelInterfacePrivate(this, xdgSurface))
{
    XdgSurfaceInterfacePrivate *surfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    surfacePrivate->toplevel = this;
    surfacePrivate->pending = &d->pending;

    d->init(resource);
}

XdgToplevelInterface::~XdgToplevelInterface()
{
    Q_EMIT aboutToBeDestroyed();

    XdgSurfaceInterfacePrivate *surfacePrivate = XdgSurfaceInterfacePrivate::get(d->xdgSurface);
    surfacePrivate->toplevel = nullptr;
    surfacePrivate->pending = nullptr;
}

SurfaceRole *XdgToplevelInterface::role()
{
    static SurfaceRole role(QByteArrayLiteral("xdg_toplevel"));
    return &role;
}

XdgShellInterface *XdgToplevelInterface::shell() const
{
    return d->xdgSurface->shell();
}

XdgSurfaceInterface *XdgToplevelInterface::xdgSurface() const
{
    return d->xdgSurface;
}

SurfaceInterface *XdgToplevelInterface::surface() const
{
    return d->xdgSurface->surface();
}

bool XdgToplevelInterface::isConfigured() const
{
    return d->xdgSurface->isConfigured();
}

XdgToplevelInterface *XdgToplevelInterface::parentXdgToplevel() const
{
    return d->parentXdgToplevel;
}

QString XdgToplevelInterface::windowTitle() const
{
    return d->windowTitle;
}

QString XdgToplevelInterface::windowClass() const
{
    return d->windowClass;
}

QSize XdgToplevelInterface::minimumSize() const
{
    return d->minimumSize;
}

QSize XdgToplevelInterface::maximumSize() const
{
    return d->maximumSize;
}

QIcon XdgToplevelInterface::customIcon() const
{
    return d->customIcon;
}

quint32 XdgToplevelInterface::sendConfigure(const QSize &size, const States &states)
{
    // Note that the states listed in the configure event must be an array of uint32_t.

    uint32_t statesData[9] = {0};
    int i = 0;

    if (states & State::MaximizedHorizontal && states & State::MaximizedVertical) {
        statesData[i++] = QtWaylandServer::xdg_toplevel::state_maximized;
    }
    if (states & State::FullScreen) {
        statesData[i++] = QtWaylandServer::xdg_toplevel::state_fullscreen;
    }
    if (states & State::Resizing) {
        statesData[i++] = QtWaylandServer::xdg_toplevel::state_resizing;
    }
    if (states & State::Activated) {
        statesData[i++] = QtWaylandServer::xdg_toplevel::state_activated;
    }

    if (d->resource()->version() >= XDG_TOPLEVEL_STATE_TILED_LEFT_SINCE_VERSION) {
        if (states & State::TiledLeft) {
            statesData[i++] = QtWaylandServer::xdg_toplevel::state_tiled_left;
        }
        if (states & State::TiledTop) {
            statesData[i++] = QtWaylandServer::xdg_toplevel::state_tiled_top;
        }
        if (states & State::TiledRight) {
            statesData[i++] = QtWaylandServer::xdg_toplevel::state_tiled_right;
        }
        if (states & State::TiledBottom) {
            statesData[i++] = QtWaylandServer::xdg_toplevel::state_tiled_bottom;
        }
    }

    if (d->resource()->version() >= XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_VERSION) {
        if (states & State::Suspended) {
            statesData[i++] = QtWaylandServer::xdg_toplevel::state_suspended;
        }
    }

    const QByteArray xdgStates = QByteArray::fromRawData(reinterpret_cast<char *>(statesData), sizeof(uint32_t) * i);
    const quint32 serial = xdgSurface()->shell()->display()->nextSerial();

    d->send_configure(size.width(), size.height(), xdgStates);

    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface());
    xdgSurfacePrivate->send_configure(serial);
    xdgSurfacePrivate->isConfigured = true;

    return serial;
}

void XdgToplevelInterface::sendClose()
{
    d->send_close();
}

void XdgToplevelInterface::sendConfigureBounds(const QSize &size)
{
    if (d->resource()->version() >= XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION) {
        d->send_configure_bounds(size.width(), size.height());
    }
}

void XdgToplevelInterface::sendWmCapabilities(Capabilities capabilities)
{
    if (d->resource()->version() < XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION) {
        return;
    }
    // Note that the capabilities listed in the event must be an array of uint32_t.

    uint32_t capabilitiesData[4] = {0};
    int i = 0;

    if (capabilities & Capability::WindowMenu) {
        capabilitiesData[i++] = QtWaylandServer::xdg_toplevel::wm_capabilities_window_menu;
    }
    if (capabilities & Capability::Maximize) {
        capabilitiesData[i++] = QtWaylandServer::xdg_toplevel::wm_capabilities_maximize;
    }
    if (capabilities & Capability::FullScreen) {
        capabilitiesData[i++] = QtWaylandServer::xdg_toplevel::wm_capabilities_fullscreen;
    }
    if (capabilities & Capability::Minimize) {
        capabilitiesData[i++] = QtWaylandServer::xdg_toplevel::wm_capabilities_minimize;
    }

    d->send_wm_capabilities(QByteArray::fromRawData(reinterpret_cast<char *>(capabilitiesData), sizeof(uint32_t) * i));
}

XdgToplevelInterface *XdgToplevelInterface::get(::wl_resource *resource)
{
    if (auto toplevelPrivate = resource_cast<XdgToplevelInterfacePrivate *>(resource)) {
        return toplevelPrivate->q;
    }
    return nullptr;
}

XdgPopupInterfacePrivate *XdgPopupInterfacePrivate::get(XdgPopupInterface *popup)
{
    return popup->d.get();
}

XdgPopupInterfacePrivate::XdgPopupInterfacePrivate(XdgPopupInterface *popup, XdgSurfaceInterface *xdgSurface)
    : SurfaceExtension(xdgSurface->surface())
    , q(popup)
    , xdgSurface(xdgSurface)
{
}

void XdgPopupInterfacePrivate::apply(XdgPopupCommit *commit)
{
    if (!parentSurface) {
        auto shellPrivate = XdgShellInterfacePrivate::get(xdgSurface->shell());
        wl_resource_post_error(shellPrivate->resourceForXdgSurface(xdgSurface)->handle,
                               QtWaylandServer::xdg_wm_base::error_invalid_popup_parent,
                               "no xdg_popup parent surface has been specified");
        return;
    }

    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    if (xdgSurfacePrivate->firstBufferAttached && !xdgSurfacePrivate->surface->buffer()) {
        reset();
        return;
    }

    xdgSurfacePrivate->apply(commit);

    if (!xdgSurfacePrivate->isInitialized) {
        Q_EMIT q->initializeRequested();
        xdgSurfacePrivate->isInitialized = true;
    }
}

void XdgPopupInterfacePrivate::reset()
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    pending = XdgPopupCommit{};
    stashed.clear();
    xdgSurfacePrivate->reset();
}

void XdgPopupInterfacePrivate::xdg_popup_destroy_resource(Resource *resource)
{
    delete q;
}

void XdgPopupInterfacePrivate::xdg_popup_destroy(Resource *resource)
{
    // TODO: We need to post an error with the code XDG_WM_BASE_ERROR_NOT_THE_TOPMOST_POPUP if
    // this popup is not the topmost grabbing popup. We most likely need a grab abstraction or
    // something to determine whether the given popup has an explicit grab.
    wl_resource_destroy(resource->handle);
}

void XdgPopupInterfacePrivate::xdg_popup_grab(Resource *resource, ::wl_resource *seatHandle, uint32_t serial)
{
    if (xdgSurface->surface()->buffer()) {
        wl_resource_post_error(resource->handle, error_invalid_grab, "xdg_surface is already mapped");
        return;
    }
    SeatInterface *seat = SeatInterface::get(seatHandle);
    Q_EMIT q->grabRequested(seat, serial);
}

void XdgPopupInterfacePrivate::xdg_popup_reposition(Resource *resource, ::wl_resource *positionerResource, uint32_t token)
{
    positioner = XdgPositioner::get(positionerResource);
    Q_EMIT q->repositionRequested(token);
}

XdgPopupInterface::XdgPopupInterface(XdgSurfaceInterface *xdgSurface, SurfaceInterface *parentSurface, const XdgPositioner &positioner, ::wl_resource *resource)
    : d(new XdgPopupInterfacePrivate(this, xdgSurface))
{
    XdgSurfaceInterfacePrivate *surfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    surfacePrivate->popup = this;
    surfacePrivate->pending = &d->pending;

    d->parentSurface = parentSurface;
    d->positioner = positioner;
    d->init(resource);
}

XdgPopupInterface::~XdgPopupInterface()
{
    Q_EMIT aboutToBeDestroyed();

    XdgSurfaceInterfacePrivate *surfacePrivate = XdgSurfaceInterfacePrivate::get(d->xdgSurface);
    surfacePrivate->popup = nullptr;
    surfacePrivate->pending = nullptr;
}

SurfaceRole *XdgPopupInterface::role()
{
    static SurfaceRole role(QByteArrayLiteral("xdg_popup"));
    return &role;
}

SurfaceInterface *XdgPopupInterface::parentSurface() const
{
    return d->parentSurface;
}

XdgSurfaceInterface *XdgPopupInterface::xdgSurface() const
{
    return d->xdgSurface;
}

SurfaceInterface *XdgPopupInterface::surface() const
{
    return d->xdgSurface->surface();
}

bool XdgPopupInterface::isConfigured() const
{
    return d->xdgSurface->isConfigured();
}

XdgPositioner XdgPopupInterface::positioner() const
{
    return d->positioner;
}

quint32 XdgPopupInterface::sendConfigure(const QRect &rect)
{
    const quint32 serial = xdgSurface()->shell()->display()->nextSerial();

    d->send_configure(rect.x(), rect.y(), rect.width(), rect.height());

    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface());
    xdgSurfacePrivate->send_configure(serial);
    xdgSurfacePrivate->isConfigured = true;

    return serial;
}

void XdgPopupInterface::sendPopupDone()
{
    d->send_popup_done();
}

void XdgPopupInterface::sendRepositioned(quint32 token)
{
    Q_ASSERT(d->resource()->version() >= XDG_POPUP_REPOSITIONED_SINCE_VERSION);
    d->send_repositioned(token);
}

XdgPopupInterface *XdgPopupInterface::get(::wl_resource *resource)
{
    if (auto popupPrivate = resource_cast<XdgPopupInterfacePrivate *>(resource)) {
        return popupPrivate->q;
    }
    return nullptr;
}

XdgPositionerPrivate::XdgPositionerPrivate(::wl_resource *resource)
    : data(new XdgPositionerData)
{
    init(resource);
}

XdgPositionerPrivate *XdgPositionerPrivate::get(wl_resource *resource)
{
    return resource_cast<XdgPositionerPrivate *>(resource);
}

void XdgPositionerPrivate::xdg_positioner_destroy_resource(Resource *resource)
{
    delete this;
}

void XdgPositionerPrivate::xdg_positioner_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgPositionerPrivate::xdg_positioner_set_size(Resource *resource, int32_t width, int32_t height)
{
    if (width < 1 || height < 1) {
        wl_resource_post_error(resource->handle, error_invalid_input, "width and height must be positive and non-zero");
        return;
    }
    data->size = QSize(width, height);
}

void XdgPositionerPrivate::xdg_positioner_set_anchor_rect(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (width < 1 || height < 1) {
        wl_resource_post_error(resource->handle, error_invalid_input, "width and height must be positive and non-zero");
        return;
    }
    data->anchorRect = QRect(x, y, width, height);
}

void XdgPositionerPrivate::xdg_positioner_set_anchor(Resource *resource, uint32_t anchor)
{
    if (anchor > anchor_bottom_right) {
        wl_resource_post_error(resource->handle, error_invalid_input, "unknown anchor point");
        return;
    }

    switch (anchor) {
    case anchor_top:
        data->anchorEdges = Qt::TopEdge;
        break;
    case anchor_top_right:
        data->anchorEdges = Qt::TopEdge | Qt::RightEdge;
        break;
    case anchor_right:
        data->anchorEdges = Qt::RightEdge;
        break;
    case anchor_bottom_right:
        data->anchorEdges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case anchor_bottom:
        data->anchorEdges = Qt::BottomEdge;
        break;
    case anchor_bottom_left:
        data->anchorEdges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case anchor_left:
        data->anchorEdges = Qt::LeftEdge;
        break;
    case anchor_top_left:
        data->anchorEdges = Qt::TopEdge | Qt::LeftEdge;
        break;
    default:
        data->anchorEdges = Qt::Edges();
        break;
    }
}

void XdgPositionerPrivate::xdg_positioner_set_parent_size(Resource *resource, int32_t width, int32_t height)
{
    data->parentSize = QSize(width, height);
}

void XdgPositionerPrivate::xdg_positioner_set_reactive(Resource *resource)
{
    data->isReactive = true;
}

void XdgPositionerPrivate::xdg_positioner_set_parent_configure(Resource *resource, uint32_t serial)
{
    data->parentConfigure = serial;
}

void XdgPositionerPrivate::xdg_positioner_set_gravity(Resource *resource, uint32_t gravity)
{
    if (gravity > gravity_bottom_right) {
        wl_resource_post_error(resource->handle, error_invalid_input, "unknown gravity direction");
        return;
    }

    switch (gravity) {
    case gravity_top:
        data->gravityEdges = Qt::TopEdge;
        break;
    case gravity_top_right:
        data->gravityEdges = Qt::TopEdge | Qt::RightEdge;
        break;
    case gravity_right:
        data->gravityEdges = Qt::RightEdge;
        break;
    case gravity_bottom_right:
        data->gravityEdges = Qt::BottomEdge | Qt::RightEdge;
        break;
    case gravity_bottom:
        data->gravityEdges = Qt::BottomEdge;
        break;
    case gravity_bottom_left:
        data->gravityEdges = Qt::BottomEdge | Qt::LeftEdge;
        break;
    case gravity_left:
        data->gravityEdges = Qt::LeftEdge;
        break;
    case gravity_top_left:
        data->gravityEdges = Qt::TopEdge | Qt::LeftEdge;
        break;
    default:
        data->gravityEdges = Qt::Edges();
        break;
    }
}

void XdgPositionerPrivate::xdg_positioner_set_constraint_adjustment(Resource *resource, uint32_t constraint_adjustment)
{
    if (constraint_adjustment & constraint_adjustment_flip_x) {
        data->flipConstraintAdjustments |= Qt::Horizontal;
    } else {
        data->flipConstraintAdjustments &= ~Qt::Horizontal;
    }

    if (constraint_adjustment & constraint_adjustment_flip_y) {
        data->flipConstraintAdjustments |= Qt::Vertical;
    } else {
        data->flipConstraintAdjustments &= ~Qt::Vertical;
    }

    if (constraint_adjustment & constraint_adjustment_slide_x) {
        data->slideConstraintAdjustments |= Qt::Horizontal;
    } else {
        data->slideConstraintAdjustments &= ~Qt::Horizontal;
    }

    if (constraint_adjustment & constraint_adjustment_slide_y) {
        data->slideConstraintAdjustments |= Qt::Vertical;
    } else {
        data->slideConstraintAdjustments &= ~Qt::Vertical;
    }

    if (constraint_adjustment & constraint_adjustment_resize_x) {
        data->resizeConstraintAdjustments |= Qt::Horizontal;
    } else {
        data->resizeConstraintAdjustments &= ~Qt::Horizontal;
    }

    if (constraint_adjustment & constraint_adjustment_resize_y) {
        data->resizeConstraintAdjustments |= Qt::Vertical;
    } else {
        data->resizeConstraintAdjustments &= ~Qt::Vertical;
    }
}

void XdgPositionerPrivate::xdg_positioner_set_offset(Resource *resource, int32_t x, int32_t y)
{
    data->offset = QPoint(x, y);
}

XdgPositioner::XdgPositioner()
    : d(new XdgPositionerData)
{
}

XdgPositioner::XdgPositioner(const XdgPositioner &other)
    : d(other.d)
{
}

XdgPositioner::~XdgPositioner()
{
}

XdgPositioner &XdgPositioner::operator=(const XdgPositioner &other)
{
    d = other.d;
    return *this;
}

bool XdgPositioner::isComplete() const
{
    return d->size.isValid() && d->anchorRect.isValid();
}

QSize XdgPositioner::size() const
{
    return d->size;
}

bool XdgPositioner::isReactive() const
{
    return d->isReactive;
}

static QPointF popupOffset(const QRectF &anchorRect, const Qt::Edges anchorEdge, const Qt::Edges gravity, const QSizeF popupSize)
{
    QPointF anchorPoint;
    switch (anchorEdge & (Qt::LeftEdge | Qt::RightEdge)) {
    case Qt::LeftEdge:
        anchorPoint.setX(anchorRect.x());
        break;
    case Qt::RightEdge:
        anchorPoint.setX(anchorRect.x() + anchorRect.width());
        break;
    default:
        anchorPoint.setX(qRound(anchorRect.x() + anchorRect.width() / 2.0));
    }
    switch (anchorEdge & (Qt::TopEdge | Qt::BottomEdge)) {
    case Qt::TopEdge:
        anchorPoint.setY(anchorRect.y());
        break;
    case Qt::BottomEdge:
        anchorPoint.setY(anchorRect.y() + anchorRect.height());
        break;
    default:
        anchorPoint.setY(qRound(anchorRect.y() + anchorRect.height() / 2.0));
    }

    // calculate where the top left point of the popup will end up with the applied gravity
    // gravity indicates direction. i.e if gravitating towards the top the popup's bottom edge
    // will next to the anchor point
    QPointF popupPosAdjust;
    switch (gravity & (Qt::LeftEdge | Qt::RightEdge)) {
    case Qt::LeftEdge:
        popupPosAdjust.setX(-popupSize.width());
        break;
    case Qt::RightEdge:
        popupPosAdjust.setX(0);
        break;
    default:
        popupPosAdjust.setX(qRound(-popupSize.width() / 2.0));
    }
    switch (gravity & (Qt::TopEdge | Qt::BottomEdge)) {
    case Qt::TopEdge:
        popupPosAdjust.setY(-popupSize.height());
        break;
    case Qt::BottomEdge:
        popupPosAdjust.setY(0);
        break;
    default:
        popupPosAdjust.setY(qRound(-popupSize.height() / 2.0));
    }

    return anchorPoint + popupPosAdjust;
}

QRectF XdgPositioner::placement(const QRectF &bounds) const
{
    // returns if a target is within the supplied bounds, optional edges argument states which side to check
    auto inBounds = [bounds](const QRectF &target, Qt::Edges edges = Qt::LeftEdge | Qt::RightEdge | Qt::TopEdge | Qt::BottomEdge) -> bool {
        if (edges & Qt::LeftEdge && target.left() < bounds.left()) {
            return false;
        }
        if (edges & Qt::TopEdge && target.top() < bounds.top()) {
            return false;
        }
        if (edges & Qt::RightEdge && target.right() > bounds.right()) {
            // normal QRect::right issue cancels out
            return false;
        }
        if (edges & Qt::BottomEdge && target.bottom() > bounds.bottom()) {
            return false;
        }
        return true;
    };

    QRectF popupRect(popupOffset(d->anchorRect, d->anchorEdges, d->gravityEdges, d->size) + d->offset, d->size);

    // if that fits, we don't need to do anything
    if (inBounds(popupRect)) {
        return popupRect;
    }
    // otherwise apply constraint adjustment per axis in order XDG Shell Popup states

    if (d->flipConstraintAdjustments & Qt::Horizontal) {
        if (!inBounds(popupRect, Qt::LeftEdge | Qt::RightEdge)) {
            // flip both edges (if either bit is set, XOR both)
            auto flippedAnchorEdge = d->anchorEdges;
            if (flippedAnchorEdge & (Qt::LeftEdge | Qt::RightEdge)) {
                flippedAnchorEdge ^= (Qt::LeftEdge | Qt::RightEdge);
            }
            auto flippedGravity = d->gravityEdges;
            if (flippedGravity & (Qt::LeftEdge | Qt::RightEdge)) {
                flippedGravity ^= (Qt::LeftEdge | Qt::RightEdge);
            }
            auto flippedPopupRect = QRectF(popupOffset(d->anchorRect, flippedAnchorEdge, flippedGravity, d->size) + d->offset, d->size);

            // if it still doesn't fit we should continue with the unflipped version
            if (inBounds(flippedPopupRect, Qt::LeftEdge | Qt::RightEdge)) {
                popupRect.moveLeft(flippedPopupRect.left());
            }
        }
    }
    if (d->slideConstraintAdjustments & Qt::Horizontal) {
        if (!inBounds(popupRect, Qt::LeftEdge)) {
            popupRect.moveLeft(bounds.left());
        }
        if (!inBounds(popupRect, Qt::RightEdge)) {
            popupRect.moveRight(bounds.right());
        }
    }
    if (d->resizeConstraintAdjustments & Qt::Horizontal) {
        QRectF unconstrainedRect = popupRect;

        if (!inBounds(unconstrainedRect, Qt::LeftEdge)) {
            unconstrainedRect.setLeft(bounds.left());
        }
        if (!inBounds(unconstrainedRect, Qt::RightEdge)) {
            unconstrainedRect.setRight(bounds.right());
        }

        if (unconstrainedRect.isValid()) {
            popupRect = unconstrainedRect;
        }
    }

    if (d->flipConstraintAdjustments & Qt::Vertical) {
        if (!inBounds(popupRect, Qt::TopEdge | Qt::BottomEdge)) {
            // flip both edges (if either bit is set, XOR both)
            auto flippedAnchorEdge = d->anchorEdges;
            if (flippedAnchorEdge & (Qt::TopEdge | Qt::BottomEdge)) {
                flippedAnchorEdge ^= (Qt::TopEdge | Qt::BottomEdge);
            }
            auto flippedGravity = d->gravityEdges;
            if (flippedGravity & (Qt::TopEdge | Qt::BottomEdge)) {
                flippedGravity ^= (Qt::TopEdge | Qt::BottomEdge);
            }
            auto flippedPopupRect = QRectF(popupOffset(d->anchorRect, flippedAnchorEdge, flippedGravity, d->size) + d->offset, d->size);

            // if it still doesn't fit we should continue with the unflipped version
            if (inBounds(flippedPopupRect, Qt::TopEdge | Qt::BottomEdge)) {
                popupRect.moveTop(flippedPopupRect.top());
            }
        }
    }
    if (d->slideConstraintAdjustments & Qt::Vertical) {
        if (!inBounds(popupRect, Qt::TopEdge)) {
            popupRect.moveTop(bounds.top());
        }
        if (!inBounds(popupRect, Qt::BottomEdge)) {
            popupRect.moveBottom(bounds.bottom());
        }
    }
    if (d->resizeConstraintAdjustments & Qt::Vertical) {
        QRectF unconstrainedRect = popupRect;

        if (!inBounds(unconstrainedRect, Qt::TopEdge)) {
            unconstrainedRect.setTop(bounds.top());
        }
        if (!inBounds(unconstrainedRect, Qt::BottomEdge)) {
            unconstrainedRect.setBottom(bounds.bottom());
        }

        if (unconstrainedRect.isValid()) {
            popupRect = unconstrainedRect;
        }
    }

    return popupRect;
}

XdgPositioner XdgPositioner::get(::wl_resource *resource)
{
    XdgPositionerPrivate *xdgPositionerPrivate = XdgPositionerPrivate::get(resource);
    if (xdgPositionerPrivate)
        return XdgPositioner(xdgPositionerPrivate->data);
    return XdgPositioner();
}

XdgPositioner::XdgPositioner(const QSharedDataPointer<XdgPositionerData> &data)
    : d(data)
{
}

} // namespace KWin

#include "moc_xdgshell.cpp"
