/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "layershell_v1_interface.h"
#include "display.h"
#include "logging.h"
#include "surface_interface.h"
#include "surfacerole_p.h"
#include "xdgshell_interface_p.h"

#include <QPointer>
#include <QQueue>

#include "qwayland-server-wlr-layer-shell-unstable-v1.h"

namespace KWaylandServer
{

static const int s_version = 3;

class LayerShellV1InterfacePrivate : public QtWaylandServer::zwlr_layer_shell_v1
{
public:
    LayerShellV1InterfacePrivate(LayerShellV1Interface *q, Display *display);

    LayerShellV1Interface *q;
    Display *display;

protected:
    void zwlr_layer_shell_v1_get_layer_surface(Resource *resource, uint32_t id,
                                               struct ::wl_resource *surface_resource,
                                               struct ::wl_resource *output_resource,
                                               uint32_t layer, const QString &scope) override;
    void zwlr_layer_shell_v1_destroy(Resource *resource) override;
};

class LayerSurfaceV1State
{
public:
    LayerSurfaceV1Interface::Layer layer = LayerSurfaceV1Interface::BottomLayer;
    Qt::Edges anchor;
    QMargins margins;
    QSize desiredSize = QSize(0, 0);
    int exclusiveZone = 0;
    bool acceptsFocus = false;
};

class LayerSurfaceV1InterfacePrivate : public SurfaceRole, public QtWaylandServer::zwlr_layer_surface_v1
{
public:
    LayerSurfaceV1InterfacePrivate(LayerSurfaceV1Interface *q, SurfaceInterface *surface);

    void commit() override;

    LayerSurfaceV1Interface *q;
    LayerShellV1Interface *shell;
    QPointer<SurfaceInterface> surface;
    QPointer<OutputInterface> output;
    LayerSurfaceV1State current;
    LayerSurfaceV1State pending;
    QQueue<quint32> serials;
    QString scope;
    bool isClosed = false;
    bool isConfigured = false;
    bool isCommitted = false;

protected:
    void zwlr_layer_surface_v1_destroy_resource(Resource *resource) override;
    void zwlr_layer_surface_v1_set_size(Resource *resource, uint32_t width, uint32_t height) override;
    void zwlr_layer_surface_v1_set_anchor(Resource *resource, uint32_t anchor) override;
    void zwlr_layer_surface_v1_set_exclusive_zone(Resource *resource, int32_t zone) override;
    void zwlr_layer_surface_v1_set_margin(Resource *resource, int32_t top, int32_t right, int32_t bottom, int32_t left) override;
    void zwlr_layer_surface_v1_set_keyboard_interactivity(Resource *resource, uint32_t keyboard_interactivity) override;
    void zwlr_layer_surface_v1_get_popup(Resource *resource, struct ::wl_resource *popup) override;
    void zwlr_layer_surface_v1_ack_configure(Resource *resource, uint32_t serial) override;
    void zwlr_layer_surface_v1_destroy(Resource *resource) override;
    void zwlr_layer_surface_v1_set_layer(Resource *resource, uint32_t layer) override;
};

LayerShellV1InterfacePrivate::LayerShellV1InterfacePrivate(LayerShellV1Interface *q, Display *display)
    : QtWaylandServer::zwlr_layer_shell_v1(*display, s_version)
    , q(q)
    , display(display)
{
}

void LayerShellV1InterfacePrivate::zwlr_layer_shell_v1_get_layer_surface(Resource *resource, uint32_t id,
                                                                         wl_resource *surface_resource,
                                                                         wl_resource *output_resource,
                                                                         uint32_t layer, const QString &scope)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    OutputInterface *output = OutputInterface::get(output_resource);

    if (surface->buffer()) {
        wl_resource_post_error(resource->handle, error_already_constructed,
                               "the wl_surface already has a buffer attached");
        return;
    }

    if (layer > layer_overlay) {
        wl_resource_post_error(resource->handle, error_invalid_layer,
                               "invalid layer %d", layer);
        return;
    }

    SurfaceRole *surfaceRole = SurfaceRole::get(surface);
    if (surfaceRole) {
        wl_resource_post_error(resource->handle, error_role,
                               "the wl_surface already has a role assigned %s",
                               surfaceRole->name().constData());
        return;
    }

    wl_resource *layerSurfaceResource = wl_resource_create(resource->client(),
                                                           &zwlr_layer_surface_v1_interface,
                                                           resource->version(), id);
    if (!layerSurfaceResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto layerSurface = new LayerSurfaceV1Interface(q, surface, output,
                                                    LayerSurfaceV1Interface::Layer(layer),
                                                    scope, layerSurfaceResource);
    emit q->surfaceCreated(layerSurface);
}

void LayerShellV1InterfacePrivate::zwlr_layer_shell_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

LayerShellV1Interface::LayerShellV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new LayerShellV1InterfacePrivate(this, display))
{
}

LayerShellV1Interface::~LayerShellV1Interface()
{
}

Display *LayerShellV1Interface::display() const
{
    return d->display;
}

LayerSurfaceV1InterfacePrivate::LayerSurfaceV1InterfacePrivate(LayerSurfaceV1Interface *q,
                                                               SurfaceInterface *surface)
    : SurfaceRole(surface, QByteArrayLiteral("layer_surface_v1"))
    , q(q)
    , surface(surface)
{
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    emit q->aboutToBeDestroyed();
    delete q;
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_size(Resource *resource, uint32_t width, uint32_t height)
{
    Q_UNUSED(resource)
    pending.desiredSize = QSize(width, height);
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_anchor(Resource *resource, uint32_t anchor)
{
    const uint32_t anchorMask = anchor_top | anchor_left | anchor_right | anchor_bottom;
    if (anchor > anchorMask) {
        wl_resource_post_error(resource->handle, error_invalid_anchor, "invalid anchor %d", anchor);
        return;
    }

    pending.anchor = Qt::Edges();

    if (anchor & anchor_top) {
        pending.anchor |= Qt::TopEdge;
    }

    if (anchor & anchor_right) {
        pending.anchor |= Qt::RightEdge;
    }

    if (anchor & anchor_bottom) {
        pending.anchor |= Qt::BottomEdge;
    }

    if (anchor & anchor_left) {
        pending.anchor |= Qt::LeftEdge;
    }
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_exclusive_zone(Resource *, int32_t zone)
{
    pending.exclusiveZone = zone;
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_margin(Resource *, int32_t top, int32_t right, int32_t bottom, int32_t left)
{
    pending.margins = QMargins(left, top, right, bottom);
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_keyboard_interactivity(Resource *resource, uint32_t keyboard_interactivity)
{
    Q_UNUSED(resource)
    pending.acceptsFocus = keyboard_interactivity;
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_get_popup(Resource *resource, struct ::wl_resource *popup_resource)
{
    XdgPopupInterface *popup = XdgPopupInterface::get(popup_resource);
    XdgPopupInterfacePrivate *popupPrivate = XdgPopupInterfacePrivate::get(popup);

    if (popup->isConfigured()) {
        wl_resource_post_error(resource->handle, error_invalid_surface_state,
                               "xdg_popup surface is already configured");
        return;
    }

    popupPrivate->parentSurface = surface;
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_ack_configure(Resource *resource, uint32_t serial)
{
    if (!serials.contains(serial)) {
        wl_resource_post_error(resource->handle, error_invalid_surface_state,
                               "invalid configure serial %d", serial);
        return;
    }
    while (!serials.isEmpty()) {
        const quint32 head = serials.takeFirst();
        if (head == serial) {
            break;
        }
    }
    if (!isClosed) {
        emit q->configureAcknowledged(serial);
    }
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_layer(Resource *resource, uint32_t layer)
{
    if (Q_UNLIKELY(layer > LayerShellV1InterfacePrivate::layer_overlay)) {
        wl_resource_post_error(resource->handle, LayerShellV1InterfacePrivate::error_invalid_layer,
                               "invalid layer %d", layer);
        return;
    }
    pending.layer = LayerSurfaceV1Interface::Layer(layer);
}

void LayerSurfaceV1InterfacePrivate::commit()
{
    if (isClosed) {
        return;
    }

    if (Q_UNLIKELY(surface->isMapped() && !isConfigured)) {
        wl_resource_post_error(resource()->handle, error_invalid_surface_state,
                               "a buffer has been attached to a layer surface prior "
                               "to the first layer_surface.configure event");
        return;
    }

    if (Q_UNLIKELY(pending.desiredSize.width() == 0 &&
            (!(pending.anchor & Qt::LeftEdge) || !(pending.anchor & Qt::RightEdge)))) {
        wl_resource_post_error(resource()->handle, error_invalid_size,
                               "the layer surface has a width of 0 but its anchor "
                               "doesn't include the left and the right screen edge");
        return;
    }

    if (Q_UNLIKELY(pending.desiredSize.height() == 0 &&
            (!(pending.anchor & Qt::TopEdge) || !(pending.anchor & Qt::BottomEdge)))) {
        wl_resource_post_error(resource()->handle, error_invalid_size,
                               "the layer surface has a height of 0 but its anchor "
                               "doesn't include the top and the bottom screen edge");
        return;
    }

    if (!surface->isMapped() && isCommitted) {
        isCommitted = false;
        isConfigured = false;

        current = LayerSurfaceV1State();
        pending = LayerSurfaceV1State();

        return;
    }

    const LayerSurfaceV1State previous = std::exchange(current, pending);

    isCommitted = true; // Must set the committed state before emitting any signals.

    if (previous.acceptsFocus != current.acceptsFocus) {
        emit q->acceptsFocusChanged();
    }
    if (previous.layer != current.layer) {
        emit q->layerChanged();
    }
    if (previous.anchor != current.anchor) {
        emit q->anchorChanged();
    }
    if (previous.desiredSize != current.desiredSize) {
        emit q->desiredSizeChanged();
    }
    if (previous.exclusiveZone != current.exclusiveZone) {
        emit q->exclusiveZoneChanged();
    }
    if (previous.margins != current.margins) {
        emit q->marginsChanged();
    }
}

LayerSurfaceV1Interface::LayerSurfaceV1Interface(LayerShellV1Interface *shell,
                                                 SurfaceInterface *surface,
                                                 OutputInterface *output, Layer layer,
                                                 const QString &scope, wl_resource *resource)
    : d(new LayerSurfaceV1InterfacePrivate(this, surface))
{
    d->current.layer = layer;
    d->pending.layer = layer;

    d->shell = shell;
    d->output = output;
    d->scope = scope;

    d->init(resource);
}

LayerSurfaceV1Interface::~LayerSurfaceV1Interface()
{
}

bool LayerSurfaceV1Interface::isCommitted() const
{
    return d->isCommitted;
}

SurfaceInterface *LayerSurfaceV1Interface::surface() const
{
    return d->surface;
}

Qt::Edges LayerSurfaceV1Interface::anchor() const
{
    return d->current.anchor;
}

QSize LayerSurfaceV1Interface::desiredSize() const
{
    return d->current.desiredSize;
}

bool LayerSurfaceV1Interface::acceptsFocus() const
{
    return d->current.acceptsFocus;
}

LayerSurfaceV1Interface::Layer LayerSurfaceV1Interface::layer() const
{
    return d->current.layer;
}

QMargins LayerSurfaceV1Interface::margins() const
{
    return d->current.margins;
}

int LayerSurfaceV1Interface::leftMargin() const
{
    return d->current.margins.left();
}

int LayerSurfaceV1Interface::topMargin() const
{
    return d->current.margins.top();
}

int LayerSurfaceV1Interface::rightMargin() const
{
    return d->current.margins.right();
}

int LayerSurfaceV1Interface::bottomMargin() const
{
    return d->current.margins.bottom();
}

int LayerSurfaceV1Interface::exclusiveZone() const
{
    return d->current.exclusiveZone;
}

Qt::Edge LayerSurfaceV1Interface::exclusiveEdge() const
{
    if (exclusiveZone() <= 0) {
        return Qt::Edge();
    }
    if (anchor() == (Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge) || anchor() == Qt::TopEdge) {
        return Qt::TopEdge;
    }
    if (anchor() == (Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge) || anchor() == Qt::RightEdge) {
        return Qt::RightEdge;
    }
    if (anchor() == (Qt::RightEdge | Qt::BottomEdge | Qt::LeftEdge) || anchor() == Qt::BottomEdge) {
        return Qt::BottomEdge;
    }
    if (anchor() == (Qt::BottomEdge | Qt::LeftEdge | Qt::TopEdge) || anchor() == Qt::LeftEdge) {
        return Qt::LeftEdge;
    }
    return Qt::Edge();
}

OutputInterface *LayerSurfaceV1Interface::output() const
{
    return d->output;
}

QString LayerSurfaceV1Interface::scope() const
{
    return d->scope;
}

quint32 LayerSurfaceV1Interface::sendConfigure(const QSize &size)
{
    if (d->isClosed) {
        qCWarning(KWAYLAND_SERVER) << "Cannot configure a closed layer shell surface";
        return 0;
    }

    const uint32_t serial = d->shell->display()->nextSerial();
    d->serials << serial;

    d->send_configure(serial, size.width(), size.height());
    d->isConfigured = true;

    return serial;
}

void LayerSurfaceV1Interface::sendClosed()
{
    if (!d->isClosed) {
        d->send_closed();
        d->isClosed = true;
    }
}

} // namespace KWaylandServer
