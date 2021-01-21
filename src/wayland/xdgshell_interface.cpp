/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgshell_interface.h"
#include "xdgshell_interface_p.h"

#include "display.h"
#include "output_interface.h"
#include "seat_interface.h"
#include "utils.h"

#include <QTimer>

namespace KWaylandServer
{

static const int s_version = 3;

XdgShellInterfacePrivate::XdgShellInterfacePrivate(XdgShellInterface *shell)
    : q(shell)
{
}

static wl_client *clientFromXdgSurface(XdgSurfaceInterface *surface)
{
    return XdgSurfaceInterfacePrivate::get(surface)->resource()->client();
}

XdgShellInterfacePrivate::Resource *XdgShellInterfacePrivate::resourceForXdgSurface(XdgSurfaceInterface *surface) const
{
    return resourceMap().value(clientFromXdgSurface(surface));
}

void XdgShellInterfacePrivate::registerXdgSurface(XdgSurfaceInterface *surface)
{
    xdgSurfaces.insert(clientFromXdgSurface(surface), surface);
}

void XdgShellInterfacePrivate::unregisterXdgSurface(XdgSurfaceInterface *surface)
{
    xdgSurfaces.remove(clientFromXdgSurface(surface), surface);
}

/**
 * @todo Whether the ping is delayed or has timed out is out of domain of the XdgShellInterface.
 * Such matter must be handled somewhere else, e.g. XdgToplevelClient, not here!
 */
void XdgShellInterfacePrivate::registerPing(quint32 serial)
{
    QTimer *timer = new QTimer(q);
    timer->setInterval(1000);
    QObject::connect(timer, &QTimer::timeout, q, [this, serial, attempt = 0]() mutable {
        ++attempt;
        if (attempt == 1) {
            emit q->pingDelayed(serial);
            return;
        }
        emit q->pingTimeout(serial);
        delete pings.take(serial);
    });
    pings.insert(serial, timer);
    timer->start();
}

XdgShellInterfacePrivate *XdgShellInterfacePrivate::get(XdgShellInterface *shell)
{
    return shell->d.data();
}

void XdgShellInterfacePrivate::xdg_wm_base_destroy(Resource *resource)
{
    if (xdgSurfaces.contains(resource->client())) {
        wl_resource_post_error(resource->handle, error_defunct_surfaces,
                               "xdg_wm_base was destroyed before children");
        return;
    }
    wl_resource_destroy(resource->handle);
}

void XdgShellInterfacePrivate::xdg_wm_base_create_positioner(Resource *resource, uint32_t id)
{
    wl_resource *positionerResource = wl_resource_create(resource->client(), &xdg_positioner_interface,
                                                         resource->version(), id);
    new XdgPositionerPrivate(positionerResource);
}

void XdgShellInterfacePrivate::xdg_wm_base_get_xdg_surface(Resource *resource, uint32_t id,
                                                           ::wl_resource *surfaceResource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surfaceResource);

    if (surface->buffer()) {
        wl_resource_post_error(resource->handle, XDG_SURFACE_ERROR_UNCONFIGURED_BUFFER,
                               "xdg_surface must not have a buffer at creation");
        return;
    }

    wl_resource *xdgSurfaceResource = wl_resource_create(resource->client(), &xdg_surface_interface,
                                                         resource->version(), id);

    XdgSurfaceInterface *xdgSurface = new XdgSurfaceInterface(q, surface, xdgSurfaceResource);
    registerXdgSurface(xdgSurface);
}

void XdgShellInterfacePrivate::xdg_wm_base_pong(Resource *resource, uint32_t serial)
{
    Q_UNUSED(resource)
    if (QTimer *timer = pings.take(serial)) {
        delete timer;
        emit q->pongReceived(serial);
    }
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

XdgSurfaceInterfacePrivate::XdgSurfaceInterfacePrivate(XdgSurfaceInterface *xdgSurface)
    : q(xdgSurface)
{
}

void XdgSurfaceInterfacePrivate::commit()
{
    if (current.windowGeometry != next.windowGeometry) {
        current.windowGeometry = next.windowGeometry;
        emit q->windowGeometryChanged(current.windowGeometry);
    }
    isMapped = surface->buffer();
}

void XdgSurfaceInterfacePrivate::reset()
{
    isConfigured = false;
    current = next = State();
    emit q->resetOccurred();
}

XdgSurfaceInterfacePrivate *XdgSurfaceInterfacePrivate::get(XdgSurfaceInterface *surface)
{
    return surface->d.data();
}

void XdgSurfaceInterfacePrivate::xdg_surface_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    XdgShellInterfacePrivate::get(shell)->unregisterXdgSurface(q);
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
    const SurfaceRole *surfaceRole = SurfaceRole::get(surface);
    if (surfaceRole) {
        wl_resource_post_error(resource->handle, error_already_constructed,
                               "the surface already has a role assigned %s",
                               surfaceRole->name().constData());
        return;
    }

    wl_resource *toplevelResource = wl_resource_create(resource->client(), &xdg_toplevel_interface,
                                                       resource->version(), id);

    toplevel = new XdgToplevelInterface(q, toplevelResource);
    emit shell->toplevelCreated(toplevel);
}

void XdgSurfaceInterfacePrivate::xdg_surface_get_popup(Resource *resource, uint32_t id,
                                                       ::wl_resource *parentResource,
                                                       ::wl_resource *positionerResource)
{
    const SurfaceRole *surfaceRole = SurfaceRole::get(surface);
    if (surfaceRole) {
        wl_resource_post_error(resource->handle, error_already_constructed,
                               "the surface already has a role assigned %s",
                               surfaceRole->name().constData());
        return;
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
        parentSurface = parentXdgSurface->surface();
    }

    wl_resource *popupResource = wl_resource_create(resource->client(), &xdg_popup_interface,
                                                    resource->version(), id);

    popup = new XdgPopupInterface(q, parentSurface, positioner, popupResource);
    emit shell->popupCreated(popup);
}

void XdgSurfaceInterfacePrivate::xdg_surface_set_window_geometry(Resource *resource,
                                                                 int32_t x, int32_t y,
                                                                 int32_t width, int32_t height)
{
    if (!toplevel && !popup) {
        wl_resource_post_error(resource->handle, error_not_constructed,
                               "xdg_surface must have a role");
        return;
    }

    if (width < 1 || height < 1) {
        wl_resource_post_error(resource->handle, -1, "invalid window geometry size (%dx%d)",
                               width, height);
        return;
    }

    next.windowGeometry = QRect(x, y, width, height);
}

void XdgSurfaceInterfacePrivate::xdg_surface_ack_configure(Resource *resource, uint32_t serial)
{
    Q_UNUSED(resource)
    emit q->configureAcknowledged(serial);
}

XdgSurfaceInterface::XdgSurfaceInterface(XdgShellInterface *shell, SurfaceInterface *surface,
                                         ::wl_resource *resource)
    : d(new XdgSurfaceInterfacePrivate(this))
{
    d->shell = shell;
    d->surface = surface;
    d->init(resource);
}

XdgSurfaceInterface::~XdgSurfaceInterface()
{
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
    return d->current.windowGeometry;
}

XdgSurfaceInterface *XdgSurfaceInterface::get(::wl_resource *resource)
{
    if (auto surfacePrivate = resource_cast<XdgSurfaceInterfacePrivate *>(resource)) {
        return surfacePrivate->q;
    }
    return nullptr;
}

XdgToplevelInterfacePrivate::XdgToplevelInterfacePrivate(XdgToplevelInterface *toplevel,
                                                         XdgSurfaceInterface *surface)
    : SurfaceRole(surface->surface(), QByteArrayLiteral("xdg_toplevel"))
    , q(toplevel)
    , xdgSurface(surface)
{
}

void XdgToplevelInterfacePrivate::commit()
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);

    bool isResettable = xdgSurfacePrivate->isConfigured && xdgSurfacePrivate->isMapped;

    if (xdgSurfacePrivate->isConfigured) {
        xdgSurfacePrivate->commit();
    } else {
        emit q->initializeRequested();
        return;
    }

    if (isResettable && !xdgSurfacePrivate->isMapped) {
        reset();
        return;
    }

    if (current.minimumSize != next.minimumSize) {
        current.minimumSize = next.minimumSize;
        emit q->minimumSizeChanged(current.minimumSize);
    }
    if (current.maximumSize != next.maximumSize) {
        current.maximumSize = next.maximumSize;
        emit q->maximumSizeChanged(current.maximumSize);
    }
}

void XdgToplevelInterfacePrivate::reset()
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    xdgSurfacePrivate->reset();

    windowTitle = QString();
    windowClass = QString();
    current = next = State();

    emit q->resetOccurred();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void XdgToplevelInterfacePrivate::xdg_toplevel_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_parent(Resource *resource,
                                                          ::wl_resource *parentResource)
{
    Q_UNUSED(resource)
    XdgToplevelInterface *parent = XdgToplevelInterface::get(parentResource);
    if (parentXdgToplevel == parent) {
        return;
    }
    parentXdgToplevel = parent;
    emit q->parentXdgToplevelChanged();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_title(Resource *resource, const QString &title)
{
    Q_UNUSED(resource)
    if (windowTitle == title) {
        return;
    }
    windowTitle = title;
    emit q->windowTitleChanged(title);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_app_id(Resource *resource, const QString &app_id)
{
    Q_UNUSED(resource)
    if (windowClass == app_id) {
        return;
    }
    windowClass = app_id;
    emit q->windowClassChanged(app_id);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_show_window_menu(Resource *resource, ::wl_resource *seatResource,
                                                                uint32_t serial, int32_t x, int32_t y)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);

    if (!xdgSurfacePrivate->isConfigured) {
        wl_resource_post_error(resource->handle, QtWaylandServer::xdg_surface::error_not_constructed,
                               "surface has not been configured yet");
        return;
    }

    SeatInterface *seat = SeatInterface::get(seatResource);
    emit q->windowMenuRequested(seat, QPoint(x, y), serial);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_move(Resource *resource, ::wl_resource *seatResource, uint32_t serial)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);

    if (!xdgSurfacePrivate->isConfigured) {
        wl_resource_post_error(resource->handle, QtWaylandServer::xdg_surface::error_not_constructed,
                               "surface has not been configured yet");
        return;
    }

    SeatInterface *seat = SeatInterface::get(seatResource);
    emit q->moveRequested(seat, serial);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_resize(Resource *resource, ::wl_resource *seatResource,
                                                      uint32_t serial, uint32_t xdgEdges)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);

    if (!xdgSurfacePrivate->isConfigured) {
        wl_resource_post_error(resource->handle, QtWaylandServer::xdg_surface::error_not_constructed,
                               "surface has not been configured yet");
        return;
    }

    SeatInterface *seat = SeatInterface::get(seatResource);

    Qt::Edges edges;
    if (xdgEdges & XDG_TOPLEVEL_RESIZE_EDGE_TOP) {
        edges |= Qt::TopEdge;
    }
    if (xdgEdges & XDG_TOPLEVEL_RESIZE_EDGE_RIGHT) {
        edges |= Qt::RightEdge;
    }
    if (xdgEdges & XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM) {
        edges |= Qt::BottomEdge;
    }
    if (xdgEdges & XDG_TOPLEVEL_RESIZE_EDGE_LEFT) {
        edges |= Qt::LeftEdge;
    }

    emit q->resizeRequested(seat, edges, serial);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_max_size(Resource *resource, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle, -1, "width and height must be positive or zero");
        return;
    }
    next.maximumSize = QSize(width, height);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_min_size(Resource *resource, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle, -1, "width and height must be positive or zero");
        return;
    }
    next.minimumSize = QSize(width, height);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_maximized(Resource *resource)
{
    Q_UNUSED(resource)
    emit q->maximizeRequested();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_unset_maximized(Resource *resource)
{
    Q_UNUSED(resource)
    emit q->unmaximizeRequested();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_fullscreen(Resource *resource, ::wl_resource *outputResource)
{
    Q_UNUSED(resource)
    OutputInterface *output = OutputInterface::get(outputResource);
    emit q->fullscreenRequested(output);
}

void XdgToplevelInterfacePrivate::xdg_toplevel_unset_fullscreen(Resource *resource)
{
    Q_UNUSED(resource)
    emit q->unfullscreenRequested();
}

void XdgToplevelInterfacePrivate::xdg_toplevel_set_minimized(Resource *resource)
{
    Q_UNUSED(resource)
    emit q->minimizeRequested();
}

XdgToplevelInterfacePrivate *XdgToplevelInterfacePrivate::get(XdgToplevelInterface *toplevel)
{
    return toplevel->d.data();
}

XdgToplevelInterfacePrivate *XdgToplevelInterfacePrivate::get(wl_resource *resource)
{
    return resource_cast<XdgToplevelInterfacePrivate *>(resource);
}

XdgToplevelInterface::XdgToplevelInterface(XdgSurfaceInterface *surface, ::wl_resource *resource)
    : d(new XdgToplevelInterfacePrivate(this, surface))
{
    d->init(resource);
}

XdgToplevelInterface::~XdgToplevelInterface()
{
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
    return d->current.minimumSize.isEmpty() ? QSize(0, 0) : d->current.minimumSize;
}

QSize XdgToplevelInterface::maximumSize() const
{
    return d->current.maximumSize.isEmpty() ? QSize(INT_MAX, INT_MAX) : d->current.maximumSize;
}

quint32 XdgToplevelInterface::sendConfigure(const QSize &size, const States &states)
{
    // Note that the states listed in the configure event must be an array of uint32_t.

    uint32_t statesData[8] = { 0 };
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

    const QByteArray xdgStates = QByteArray::fromRawData(reinterpret_cast<char *>(statesData),
                                                         sizeof(uint32_t) * i);
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

XdgToplevelInterface *XdgToplevelInterface::get(::wl_resource *resource)
{
    if (auto toplevelPrivate = resource_cast<XdgToplevelInterfacePrivate *>(resource)) {
        return toplevelPrivate->q;
    }
    return nullptr;
}

XdgPopupInterfacePrivate *XdgPopupInterfacePrivate::get(XdgPopupInterface *popup)
{
    return popup->d.data();
}

XdgPopupInterfacePrivate::XdgPopupInterfacePrivate(XdgPopupInterface *popup,
                                                   XdgSurfaceInterface *surface)
    : SurfaceRole(surface->surface(), QByteArrayLiteral("xdg_popup"))
    , q(popup)
    , xdgSurface(surface)
{
}

void XdgPopupInterfacePrivate::commit()
{
    if (!parentSurface) {
        auto shellPrivate = XdgShellInterfacePrivate::get(xdgSurface->shell());
        wl_resource_post_error(shellPrivate->resourceForXdgSurface(xdgSurface)->handle,
                               QtWaylandServer::xdg_wm_base::error_invalid_popup_parent,
                               "no xdg_popup parent surface has been specified");
        return;
    }

    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    bool isResettable = xdgSurfacePrivate->isConfigured && xdgSurfacePrivate->isMapped;

    if (xdgSurfacePrivate->isConfigured) {
        xdgSurfacePrivate->commit();
    } else {
        emit q->initializeRequested();
        return;
    }

    if (isResettable && !xdgSurfacePrivate->isMapped) {
        reset();
    }
}

void XdgPopupInterfacePrivate::reset()
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    xdgSurfacePrivate->reset();
}

void XdgPopupInterfacePrivate::xdg_popup_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
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
        wl_resource_post_error(resource->handle, error_invalid_grab,
                               "xdg_surface is already mapped");
        return;
    }
    SeatInterface *seat = SeatInterface::get(seatHandle);
    emit q->grabRequested(seat, serial);
}

void XdgPopupInterfacePrivate::xdg_popup_reposition(Resource *resource, ::wl_resource *positionerResource, uint32_t token)
{
    Q_UNUSED(resource)
    positioner = XdgPositioner::get(positionerResource);
    emit q->repositionRequested(token);
}

XdgPopupInterface::XdgPopupInterface(XdgSurfaceInterface *surface,
                                     SurfaceInterface *parentSurface,
                                     const XdgPositioner &positioner,
                                     ::wl_resource *resource)
    : d(new XdgPopupInterfacePrivate(this, surface))
{
    d->parentSurface = parentSurface;
    d->positioner = positioner;
    d->init(resource);
}

XdgPopupInterface::~XdgPopupInterface()
{
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
    Q_UNUSED(resource)
    delete this;
}

void XdgPositionerPrivate::xdg_positioner_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgPositionerPrivate::xdg_positioner_set_size(Resource *resource, int32_t width, int32_t height)
{
    if (width < 1 || height < 1) {
        wl_resource_post_error(resource->handle, error_invalid_input,
                               "width and height must be positive and non-zero");
        return;
    }
    data->size = QSize(width, height);
}

void XdgPositionerPrivate::xdg_positioner_set_anchor_rect(Resource *resource, int32_t x, int32_t y,
                                                   int32_t width, int32_t height)
{
    if (width < 1 || height < 1) {
        wl_resource_post_error(resource->handle, error_invalid_input,
                               "width and height must be positive and non-zero");
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
    Q_UNUSED(resource)
    data->parentSize = QSize(width, height);
}

void XdgPositionerPrivate::xdg_positioner_set_reactive(Resource *resource)
{
    Q_UNUSED(resource)
    data->isReactive = true;
}

void XdgPositionerPrivate::xdg_positioner_set_parent_configure(Resource *resource, uint32_t serial)
{
    Q_UNUSED(resource)
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

void XdgPositionerPrivate::xdg_positioner_set_constraint_adjustment(Resource *resource,
                                                                    uint32_t constraint_adjustment)
{
    Q_UNUSED(resource)

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
    Q_UNUSED(resource)
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

Qt::Orientations XdgPositioner::slideConstraintAdjustments() const
{
    return d->slideConstraintAdjustments;
}

Qt::Orientations XdgPositioner::flipConstraintAdjustments() const
{
    return d->flipConstraintAdjustments;
}

Qt::Orientations XdgPositioner::resizeConstraintAdjustments() const
{
    return d->resizeConstraintAdjustments;
}

Qt::Edges XdgPositioner::anchorEdges() const
{
    return d->anchorEdges;
}

Qt::Edges XdgPositioner::gravityEdges() const
{
    return d->gravityEdges;
}

QSize XdgPositioner::size() const
{
    return d->size;
}

QRect XdgPositioner::anchorRect() const
{
    return d->anchorRect;
}

QPoint XdgPositioner::offset() const
{
    return d->offset;
}

QSize XdgPositioner::parentSize() const
{
    return d->parentSize;
}

bool XdgPositioner::isReactive() const
{
    return d->isReactive;
}

quint32 XdgPositioner::parentConfigure() const
{
    return d->parentConfigure;
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

} // namespace KWaylandServer
