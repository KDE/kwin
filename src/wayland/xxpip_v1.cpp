/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/xxpip_v1.h"
#include "utils/resource.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland/xdgshell_p.h"

#include "qwayland-server-xx-pip-v1.h"

namespace KWin
{

static const int s_version = 1;

class XXPipShellV1InterfacePrivate : public QtWaylandServer::xx_pip_shell_v1
{
public:
    explicit XXPipShellV1InterfacePrivate(XXPipShellV1Interface *q, Display *display);

    XXPipShellV1Interface *q;
    Display *display;

protected:
    void xx_pip_shell_v1_destroy(Resource *resource) override;
    void xx_pip_shell_v1_get_pip(Resource *resource, uint32_t id, struct ::wl_resource *xdg_surface) override;
};

XXPipShellV1InterfacePrivate::XXPipShellV1InterfacePrivate(XXPipShellV1Interface *q, Display *display)
    : QtWaylandServer::xx_pip_shell_v1(*display, s_version)
    , q(q)
    , display(display)
{
}

void XXPipShellV1InterfacePrivate::xx_pip_shell_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXPipShellV1InterfacePrivate::xx_pip_shell_v1_get_pip(Resource *resource, uint32_t id, struct ::wl_resource *xdg_surface)
{
    XdgSurfaceInterface *xdgSurface = XdgSurfaceInterface::get(xdg_surface);

    if (const SurfaceRole *role = xdgSurface->surface()->role()) {
        if (role != XXPipV1Interface::role()) {
            wl_resource_post_error(resource->handle, error_already_constructed, "the surface already has a role assigned %s", role->name().constData());
            return;
        }
    } else {
        xdgSurface->surface()->setRole(XXPipV1Interface::role());
    }

    wl_resource *pipResource = wl_resource_create(resource->client(), &xx_pip_v1_interface, resource->version(), id);
    auto pip = new XXPipV1Interface(q, xdgSurface, pipResource);

    Q_EMIT q->pipCreated(pip);
}

XXPipShellV1Interface::XXPipShellV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<XXPipShellV1InterfacePrivate>(this, display))
{
}

XXPipShellV1Interface::~XXPipShellV1Interface()
{
}

Display *XXPipShellV1Interface::display() const
{
    return d->display;
}

class XXPipV1Commit : public SurfaceAttachedState<XXPipV1Commit>, public XdgSurfaceCommit
{
public:
    QPointer<SurfaceInterface> origin;
    QRect originRect;
};

class XXPipV1InterfacePrivate : public SurfaceExtension<XXPipV1InterfacePrivate, XXPipV1Commit>, public QtWaylandServer::xx_pip_v1
{
public:
    XXPipV1InterfacePrivate(XXPipV1Interface *q, XXPipShellV1Interface *shell, XdgSurfaceInterface *xdgSurface);

    void apply(XXPipV1Commit *comit);
    void reset();

    XXPipV1Interface *q;
    XXPipShellV1Interface *shell;
    XdgSurfaceInterface *xdgSurface;
    QString applicationId;
    QPointer<SurfaceInterface> origin;
    QRect originRect;

protected:
    void xx_pip_v1_destroy_resource(Resource *resource) override;
    void xx_pip_v1_destroy(Resource *resource) override;
    void xx_pip_v1_set_app_id(Resource *resource, const QString &app_id) override;
    void xx_pip_v1_set_origin(Resource *resource, struct ::wl_resource *origin) override;
    void xx_pip_v1_set_origin_rect(Resource *resource, int32_t x, int32_t y, uint32_t width, uint32_t height) override;
    void xx_pip_v1_move(Resource *resource, struct ::wl_resource *seat, uint32_t serial) override;
    void xx_pip_v1_resize(Resource *resource, struct ::wl_resource *seat, uint32_t serial, uint32_t edges) override;
};

XXPipV1InterfacePrivate::XXPipV1InterfacePrivate(XXPipV1Interface *q, XXPipShellV1Interface *shell, XdgSurfaceInterface *xdgSurface)
    : SurfaceExtension(xdgSurface->surface())
    , q(q)
    , shell(shell)
    , xdgSurface(xdgSurface)
{
}

void XXPipV1InterfacePrivate::apply(XXPipV1Commit *commit)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    if (xdgSurfacePrivate->firstBufferAttached && !xdgSurfacePrivate->surface->buffer()) {
        reset();
        return;
    }

    if (!commit->origin.isNull()) {
        origin = commit->origin;
    }
    if (!commit->originRect.isNull()) {
        originRect = commit->originRect;
    }

    xdgSurfacePrivate->apply(commit);

    if (!xdgSurfacePrivate->isConfigured) {
        Q_EMIT q->initializeRequested();
    }
}

void XXPipV1InterfacePrivate::reset()
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    xdgSurfacePrivate->reset();

    Q_EMIT q->resetOccurred();
}

void XXPipV1InterfacePrivate::xx_pip_v1_destroy_resource(Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void XXPipV1InterfacePrivate::xx_pip_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXPipV1InterfacePrivate::xx_pip_v1_set_app_id(Resource *resource, const QString &app_id)
{
    if (applicationId != app_id) {
        applicationId = app_id;
        Q_EMIT q->applicationIdChanged();
    }
}

void XXPipV1InterfacePrivate::xx_pip_v1_set_origin(Resource *resource, struct ::wl_resource *origin_resource)
{
    SurfaceInterface *origin = SurfaceInterface::get(origin_resource);
    if (origin == xdgSurface->surface()) {
        wl_resource_post_error(resource->handle, error_invalid_origin, "pip surface cannot be its own origin");
        return;
    }

    pending->origin = origin;
}

void XXPipV1InterfacePrivate::xx_pip_v1_set_origin_rect(Resource *resource, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    pending->originRect = QRect(x, y, width, height);
}

void XXPipV1InterfacePrivate::xx_pip_v1_move(Resource *resource, struct ::wl_resource *seat_resource, uint32_t serial)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    if (!xdgSurfacePrivate->isConfigured) {
        wl_resource_post_error(resource->handle, QtWaylandServer::xdg_surface::error_not_constructed, "surface has not been configured yet");
        return;
    }

    Q_EMIT q->moveRequested(SeatInterface::get(seat_resource), serial);
}

void XXPipV1InterfacePrivate::xx_pip_v1_resize(Resource *resource, struct ::wl_resource *seat_resource, uint32_t serial, uint32_t edges)
{
    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    if (!xdgSurfacePrivate->isConfigured) {
        wl_resource_post_error(resource->handle, QtWaylandServer::xdg_surface::error_not_constructed, "surface has not been configured yet");
        return;
    }

    Gravity gravity;
    switch (edges) {
    case resize_edge_none:
        gravity = Gravity::None;
        break;
    case resize_edge_top:
        gravity = Gravity::Top;
        break;
    case resize_edge_bottom:
        gravity = Gravity::Bottom;
        break;
    case resize_edge_left:
        gravity = Gravity::Left;
        break;
    case resize_edge_top_left:
        gravity = Gravity::TopLeft;
        break;
    case resize_edge_bottom_left:
        gravity = Gravity::BottomLeft;
        break;
    case resize_edge_right:
        gravity = Gravity::Right;
        break;
    case resize_edge_top_right:
        gravity = Gravity::TopRight;
        break;
    case resize_edge_bottom_right:
        gravity = Gravity::BottomRight;
        break;
    default:
        wl_resource_post_error(resource->handle, error_invalid_resize_edge, "invalid resize edge");
        return;
    }

    Q_EMIT q->resizeRequested(SeatInterface::get(seat_resource), gravity, serial);
}

XXPipV1Interface::XXPipV1Interface(XXPipShellV1Interface *shell, XdgSurfaceInterface *xdgSurface, wl_resource *resource)
    : d(std::make_unique<XXPipV1InterfacePrivate>(this, shell, xdgSurface))
{
    XdgSurfaceInterfacePrivate *surfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface);
    surfacePrivate->pip = this;
    surfacePrivate->pending = d->pending;

    d->init(resource);
}

XXPipV1Interface::~XXPipV1Interface()
{
}

SurfaceRole *XXPipV1Interface::role()
{
    static SurfaceRole role(QByteArrayLiteral("xx_pip_v1"));
    return &role;
}

bool XXPipV1Interface::isConfigured() const
{
    return d->xdgSurface->isConfigured();
}

XdgSurfaceInterface *XXPipV1Interface::xdgSurface() const
{
    return d->xdgSurface;
}

SurfaceInterface *XXPipV1Interface::surface() const
{
    return d->xdgSurface->surface();
}

QString XXPipV1Interface::applicationId() const
{
    return d->applicationId;
}

quint32 XXPipV1Interface::sendConfigureSize(const QSizeF &size)
{
    const quint32 serial = d->shell->display()->nextSerial();

    d->send_configure_size(size.width(), size.height());

    auto xdgSurfacePrivate = XdgSurfaceInterfacePrivate::get(xdgSurface());
    xdgSurfacePrivate->send_configure(serial);
    xdgSurfacePrivate->isConfigured = true;

    return serial;
}

void XXPipV1Interface::sendClosed()
{
    d->send_closed();
}

void XXPipV1Interface::sendConfigureBounds(const QSizeF &size)
{
    d->send_configure_bounds(size.width(), size.height());
}

XXPipV1Interface *XXPipV1Interface::get(::wl_resource *resource)
{
    if (auto pipPrivate = resource_cast<XXPipV1InterfacePrivate *>(resource)) {
        return pipPrivate->q;
    }
    return nullptr;
}

} // namespace KWin
