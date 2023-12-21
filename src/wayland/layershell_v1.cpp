/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "layershell_v1.h"
#include "display.h"
#include "output.h"
#include "surface.h"
#include "utils/common.h"
#include "xdgshell_p.h"

#include <QPointer>
#include <QQueue>

#include "qwayland-server-wlr-layer-shell-unstable-v1.h"

namespace KWin
{
static const int s_version = 5;

class LayerShellV1InterfacePrivate : public QtWaylandServer::zwlr_layer_shell_v1
{
public:
    LayerShellV1InterfacePrivate(LayerShellV1Interface *q, Display *display);

    LayerShellV1Interface *q;
    Display *display;

protected:
    void zwlr_layer_shell_v1_get_layer_surface(Resource *resource,
                                               uint32_t id,
                                               struct ::wl_resource *surface_resource,
                                               struct ::wl_resource *output_resource,
                                               uint32_t layer,
                                               const QString &scope) override;
    void zwlr_layer_shell_v1_destroy(Resource *resource) override;
};

struct LayerSurfaceV1Commit
{
    std::optional<LayerSurfaceV1Interface::Layer> layer;
    std::optional<Qt::Edges> anchor;
    std::optional<QMargins> margins;
    std::optional<QSize> desiredSize;
    std::optional<int> exclusiveZone;
    std::optional<Qt::Edge> exclusiveEdge;
    std::optional<quint32> acknowledgedConfigure;
    std::optional<bool> acceptsFocus;
};

struct LayerSurfaceV1State
{
    QQueue<quint32> serials;
    LayerSurfaceV1Interface::Layer layer = LayerSurfaceV1Interface::BottomLayer;
    Qt::Edges anchor;
    QMargins margins;
    QSize desiredSize = QSize(0, 0);
    int exclusiveZone = 0;
    Qt::Edge exclusiveEdge = Qt::Edge();
    bool acceptsFocus = false;
    bool configured = false;
    bool closed = false;
    bool committed = false;
    bool firstBufferAttached = false;
};

class LayerSurfaceV1InterfacePrivate : public SurfaceExtension<LayerSurfaceV1Commit>, public QtWaylandServer::zwlr_layer_surface_v1
{
public:
    LayerSurfaceV1InterfacePrivate(LayerSurfaceV1Interface *q, SurfaceInterface *surface);

    void apply(LayerSurfaceV1Commit *commit) override;

    LayerSurfaceV1Interface *q;
    LayerShellV1Interface *shell;
    QPointer<SurfaceInterface> surface;
    QPointer<OutputInterface> output;
    QString scope;
    LayerSurfaceV1State state;

protected:
    void zwlr_layer_surface_v1_destroy_resource(Resource *resource) override;
    void zwlr_layer_surface_v1_set_size(Resource *resource, uint32_t width, uint32_t height) override;
    void zwlr_layer_surface_v1_set_anchor(Resource *resource, uint32_t anchor) override;
    void zwlr_layer_surface_v1_set_exclusive_edge(Resource *resource, uint32_t edge) override;
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

void LayerShellV1InterfacePrivate::zwlr_layer_shell_v1_get_layer_surface(Resource *resource,
                                                                         uint32_t id,
                                                                         wl_resource *surface_resource,
                                                                         wl_resource *output_resource,
                                                                         uint32_t layer,
                                                                         const QString &scope)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    OutputInterface *output = OutputInterface::get(output_resource);

    if (surface->buffer()) {
        wl_resource_post_error(resource->handle, error_already_constructed, "the wl_surface already has a buffer attached");
        return;
    }

    if (layer > layer_overlay) {
        wl_resource_post_error(resource->handle, error_invalid_layer, "invalid layer %d", layer);
        return;
    }

    if (const SurfaceRole *role = surface->role()) {
        if (role != LayerSurfaceV1Interface::role()) {
            wl_resource_post_error(resource->handle, error_role,
                                   "the wl_surface already has a role assigned %s", role->name().constData());
            return;
        }
    } else {
        surface->setRole(LayerSurfaceV1Interface::role());
    }

    wl_resource *layerSurfaceResource = wl_resource_create(resource->client(), &zwlr_layer_surface_v1_interface, resource->version(), id);
    if (!layerSurfaceResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    auto layerSurface = new LayerSurfaceV1Interface(q, surface, output, LayerSurfaceV1Interface::Layer(layer), scope, layerSurfaceResource);
    Q_EMIT q->surfaceCreated(layerSurface);
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

LayerSurfaceV1InterfacePrivate::LayerSurfaceV1InterfacePrivate(LayerSurfaceV1Interface *q, SurfaceInterface *surface)
    : SurfaceExtension(surface)
    , q(q)
    , surface(surface)
{
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_destroy_resource(Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_size(Resource *resource, uint32_t width, uint32_t height)
{
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
        *pending.anchor |= Qt::TopEdge;
    }

    if (anchor & anchor_right) {
        *pending.anchor |= Qt::RightEdge;
    }

    if (anchor & anchor_bottom) {
        *pending.anchor |= Qt::BottomEdge;
    }

    if (anchor & anchor_left) {
        *pending.anchor |= Qt::LeftEdge;
    }
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_exclusive_edge(Resource *resource, uint32_t edge)
{
    if (!edge) {
        pending.exclusiveEdge = Qt::Edge();
    } else if (edge == anchor_top) {
        pending.exclusiveEdge = Qt::TopEdge;
    } else if (edge == anchor_right) {
        pending.exclusiveEdge = Qt::RightEdge;
    } else if (edge == anchor_bottom) {
        pending.exclusiveEdge = Qt::BottomEdge;
    } else if (edge == anchor_left) {
        pending.exclusiveEdge = Qt::LeftEdge;
    } else {
        wl_resource_post_error(resource->handle, error_invalid_exclusive_edge, "Invalid exclusive edge: %d", edge);
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
    pending.acceptsFocus = keyboard_interactivity;
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_get_popup(Resource *resource, struct ::wl_resource *popup_resource)
{
    XdgPopupInterface *popup = XdgPopupInterface::get(popup_resource);
    XdgPopupInterfacePrivate *popupPrivate = XdgPopupInterfacePrivate::get(popup);

    if (popup->isConfigured()) {
        wl_resource_post_error(resource->handle, error_invalid_surface_state, "xdg_popup surface is already configured");
        return;
    }

    popupPrivate->parentSurface = surface;
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_ack_configure(Resource *resource, uint32_t serial)
{
    if (!state.serials.contains(serial)) {
        wl_resource_post_error(resource->handle, error_invalid_surface_state, "invalid configure serial %d", serial);
        return;
    }
    while (!state.serials.isEmpty()) {
        const quint32 head = state.serials.takeFirst();
        if (head == serial) {
            break;
        }
    }
    if (!state.closed) {
        pending.acknowledgedConfigure = serial;
    }
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LayerSurfaceV1InterfacePrivate::zwlr_layer_surface_v1_set_layer(Resource *resource, uint32_t layer)
{
    if (Q_UNLIKELY(layer > LayerShellV1InterfacePrivate::layer_overlay)) {
        wl_resource_post_error(resource->handle, LayerShellV1InterfacePrivate::error_invalid_layer, "invalid layer %d", layer);
        return;
    }
    pending.layer = LayerSurfaceV1Interface::Layer(layer);
}

void LayerSurfaceV1InterfacePrivate::apply(LayerSurfaceV1Commit *commit)
{
    if (state.closed) {
        return;
    }

    if (commit->acknowledgedConfigure.has_value()) {
        Q_EMIT q->configureAcknowledged(commit->acknowledgedConfigure.value());
    }

    if (Q_UNLIKELY(surface->isMapped() && !state.configured)) {
        wl_resource_post_error(resource()->handle,
                               error_invalid_surface_state,
                               "a buffer has been attached to a layer surface prior "
                               "to the first layer_surface.configure event");
        return;
    }

    if (commit->desiredSize && commit->desiredSize->width() == 0) {
        const Qt::Edges anchor = commit->anchor.value_or(state.anchor);
        if (!(anchor & Qt::LeftEdge) || !(anchor & Qt::RightEdge)) {
            wl_resource_post_error(resource()->handle,
                                   error_invalid_size,
                                   "the layer surface has a width of 0 but its anchor "
                                   "doesn't include the left and the right screen edge");
            return;
        }
    }

    if (commit->desiredSize && commit->desiredSize->height() == 0) {
        const Qt::Edges anchor = commit->anchor.value_or(state.anchor);
        if (!(anchor & Qt::TopEdge) || !(anchor & Qt::BottomEdge)) {
            wl_resource_post_error(resource()->handle,
                                   error_invalid_size,
                                   "the layer surface has a height of 0 but its anchor "
                                   "doesn't include the top and the bottom screen edge");
            return;
        }
    }

    if (commit->exclusiveEdge.has_value() || commit->anchor.has_value()) {
        const quint32 exclusiveEdge = commit->exclusiveEdge.value_or(state.exclusiveEdge);
        const quint32 anchor = commit->anchor.value_or(state.anchor);
        if (exclusiveEdge && !(exclusiveEdge & anchor)) {
            wl_resource_post_error(resource()->handle, error_invalid_exclusive_edge, "Exclusive edge is not of the anchors");
            return;
        }
    }

    // detect reset
    if (!surface->isMapped() && state.firstBufferAttached) {
        state = LayerSurfaceV1State();
        pending = LayerSurfaceV1Commit();
        stashed.clear();

        return;
    }

    const LayerSurfaceV1State previous = state;

    state.committed = true; // Must set the committed state before emitting any signals.
    if (surface->isMapped()) {
        state.firstBufferAttached = true;
    }

    if (commit->layer.has_value()) {
        state.layer = commit->layer.value();
    }
    if (commit->anchor.has_value()) {
        state.anchor = commit->anchor.value();
    }
    if (commit->margins.has_value()) {
        state.margins = commit->margins.value();
    }
    if (commit->desiredSize.has_value()) {
        state.desiredSize = commit->desiredSize.value();
    }
    if (commit->exclusiveZone.has_value()) {
        state.exclusiveZone = commit->exclusiveZone.value();
    }
    if (commit->exclusiveEdge.has_value()) {
        state.exclusiveEdge = commit->exclusiveEdge.value();
    }

    if (commit->acceptsFocus.has_value()) {
        state.acceptsFocus = commit->acceptsFocus.value();
    }

    if (previous.acceptsFocus != state.acceptsFocus) {
        Q_EMIT q->acceptsFocusChanged();
    }
    if (previous.layer != state.layer) {
        Q_EMIT q->layerChanged();
    }
    if (previous.anchor != state.anchor) {
        Q_EMIT q->anchorChanged();
    }
    if (previous.desiredSize != state.desiredSize) {
        Q_EMIT q->desiredSizeChanged();
    }
    if (previous.exclusiveZone != state.exclusiveZone) {
        Q_EMIT q->exclusiveZoneChanged();
    }
    if (previous.margins != state.margins) {
        Q_EMIT q->marginsChanged();
    }
}

LayerSurfaceV1Interface::LayerSurfaceV1Interface(LayerShellV1Interface *shell,
                                                 SurfaceInterface *surface,
                                                 OutputInterface *output,
                                                 Layer layer,
                                                 const QString &scope,
                                                 wl_resource *resource)
    : d(new LayerSurfaceV1InterfacePrivate(this, surface))
{
    d->state.layer = layer;

    d->shell = shell;
    d->output = output;
    d->scope = scope;

    d->init(resource);
}

LayerSurfaceV1Interface::~LayerSurfaceV1Interface()
{
}

SurfaceRole *LayerSurfaceV1Interface::role()
{
    static SurfaceRole role(QByteArrayLiteral("layer_surface_v1"));
    return &role;
}

bool LayerSurfaceV1Interface::isCommitted() const
{
    return d->state.committed;
}

SurfaceInterface *LayerSurfaceV1Interface::surface() const
{
    return d->surface;
}

Qt::Edges LayerSurfaceV1Interface::anchor() const
{
    return d->state.anchor;
}

QSize LayerSurfaceV1Interface::desiredSize() const
{
    return d->state.desiredSize;
}

bool LayerSurfaceV1Interface::acceptsFocus() const
{
    return d->state.acceptsFocus;
}

LayerSurfaceV1Interface::Layer LayerSurfaceV1Interface::layer() const
{
    return d->state.layer;
}

QMargins LayerSurfaceV1Interface::margins() const
{
    return d->state.margins;
}

int LayerSurfaceV1Interface::leftMargin() const
{
    return d->state.margins.left();
}

int LayerSurfaceV1Interface::topMargin() const
{
    return d->state.margins.top();
}

int LayerSurfaceV1Interface::rightMargin() const
{
    return d->state.margins.right();
}

int LayerSurfaceV1Interface::bottomMargin() const
{
    return d->state.margins.bottom();
}

int LayerSurfaceV1Interface::exclusiveZone() const
{
    return d->state.exclusiveZone;
}

Qt::Edge LayerSurfaceV1Interface::exclusiveEdge() const
{
    if (exclusiveZone() <= 0) {
        return Qt::Edge();
    }

    if (d->state.exclusiveEdge) {
        return d->state.exclusiveEdge;
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
    if (d->state.closed) {
        qCWarning(KWIN_CORE) << "Cannot configure a closed layer shell surface";
        return 0;
    }

    const uint32_t serial = d->shell->display()->nextSerial();
    d->state.serials << serial;

    d->send_configure(serial, size.width(), size.height());
    d->state.configured = true;

    return serial;
}

void LayerSurfaceV1Interface::sendClosed()
{
    if (!d->state.closed) {
        d->send_closed();
        d->state.closed = true;
    }
}

} // namespace KWin

#include "moc_layershell_v1.cpp"
