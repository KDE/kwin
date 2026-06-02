/*
    SPDX-FileCopyrightText: 2026 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "foreigntoplevel_v1.h"

#include "display.h"
#include "output.h"
#include "seat.h"
#include "surface.h"

#include <QPointer>
#include <QSet>

#include <qwayland-server-wlr-foreign-toplevel-management-unstable-v1.h>

namespace KWin
{

static constexpr uint32_t s_version = 3;

class ForeignToplevelHandleV1InterfacePrivate : public QtWaylandServer::zwlr_foreign_toplevel_handle_v1
{
public:
    ForeignToplevelHandleV1InterfacePrivate(ForeignToplevelManagerV1Interface *manager, ForeignToplevelHandleV1Interface *q);
    ~ForeignToplevelHandleV1InterfacePrivate() override;

    void sendInitialState(Resource *resource);
    void sendDone(wl_resource *resource);
    void sendState(Resource *resource);
    void setState(QtWaylandServer::zwlr_foreign_toplevel_handle_v1::state flag, bool enabled);
    wl_resource *resourceForParent(ForeignToplevelHandleV1Interface *parentHandle, Resource *child) const;

    ForeignToplevelManagerV1Interface *manager;
    ForeignToplevelHandleV1Interface *q;
    QString title;
    QString appId;
    uint32_t state = 0;
    QPointer<ForeignToplevelHandleV1Interface> parent;
    QMetaObject::Connection parentDestroyConnection;
    QPointer<OutputInterface> output;

protected:
    void zwlr_foreign_toplevel_handle_v1_set_maximized(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_unset_maximized(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_set_minimized(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_unset_minimized(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_activate(Resource *resource, wl_resource *seat) override;
    void zwlr_foreign_toplevel_handle_v1_close(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_set_rectangle(Resource *resource, wl_resource *surface, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void zwlr_foreign_toplevel_handle_v1_destroy(Resource *resource) override;
    void zwlr_foreign_toplevel_handle_v1_set_fullscreen(Resource *resource, wl_resource *output) override;
    void zwlr_foreign_toplevel_handle_v1_unset_fullscreen(Resource *resource) override;
};

class ForeignToplevelManagerV1InterfacePrivate : public QtWaylandServer::zwlr_foreign_toplevel_manager_v1
{
public:
    ForeignToplevelManagerV1InterfacePrivate(ForeignToplevelManagerV1Interface *q, Display *display);

    QList<ForeignToplevelHandleV1Interface *> toplevels;
    QSet<wl_resource *> stoppedResources;
    ForeignToplevelManagerV1Interface *q;

protected:
    void zwlr_foreign_toplevel_manager_v1_bind_resource(Resource *resource) override;
    void zwlr_foreign_toplevel_manager_v1_stop(Resource *resource) override;
    void zwlr_foreign_toplevel_manager_v1_destroy_resource(Resource *resource) override;
};

ForeignToplevelHandleV1InterfacePrivate::ForeignToplevelHandleV1InterfacePrivate(ForeignToplevelManagerV1Interface *manager, ForeignToplevelHandleV1Interface *q)
    : manager(manager)
    , q(q)
{
}

ForeignToplevelHandleV1InterfacePrivate::~ForeignToplevelHandleV1InterfacePrivate()
{
    const auto resources = resourceMap();
    for (Resource *resource : resources) {
        send_closed(resource->handle);
    }
}

void ForeignToplevelHandleV1InterfacePrivate::sendState(Resource *resource)
{
    QByteArray states;

    auto appendState = [&states](uint32_t value) {
        states.append(reinterpret_cast<const char *>(&value), sizeof(value));
    };

    if (state & (1u << state_maximized)) {
        appendState(state_maximized);
    }
    if (state & (1u << state_minimized)) {
        appendState(state_minimized);
    }
    if (state & (1u << state_activated)) {
        appendState(state_activated);
    }
    if (state & (1u << state_fullscreen)) {
        appendState(state_fullscreen);
    }

    send_state(resource->handle, states);
}

void ForeignToplevelHandleV1InterfacePrivate::sendDone(wl_resource *resource)
{
    send_done(resource);
}

wl_resource *ForeignToplevelHandleV1InterfacePrivate::resourceForParent(ForeignToplevelHandleV1Interface *parentHandle, Resource *child) const
{
    if (!parentHandle) {
        return nullptr;
    }

    const auto parentResources = parentHandle->d->resourceMap().values(child->client());
    if (parentResources.isEmpty()) {
        return nullptr;
    }
    return parentResources.constFirst()->handle;
}

void ForeignToplevelHandleV1InterfacePrivate::sendInitialState(Resource *resource)
{
    if (!title.isEmpty()) {
        send_title(resource->handle, title);
    }
    if (!appId.isEmpty()) {
        send_app_id(resource->handle, appId);
    }
    if (output) {
        const auto outputResources = output->clientResources(resource->client());
        for (wl_resource *outputResource : outputResources) {
            send_output_enter(resource->handle, outputResource);
        }
    }
    sendState(resource);
    send_parent(resource->handle, resourceForParent(parent, resource));
    sendDone(resource->handle);
}

void ForeignToplevelHandleV1InterfacePrivate::setState(QtWaylandServer::zwlr_foreign_toplevel_handle_v1::state flag, bool enabled)
{
    const uint32_t mask = 1u << static_cast<uint32_t>(flag);
    if (((state & mask) != 0) == enabled) {
        return;
    }

    if (enabled) {
        state |= mask;
    } else {
        state &= ~mask;
    }

    const auto resources = resourceMap();
    for (Resource *resource : resources) {
        sendState(resource);
        sendDone(resource->handle);
    }
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_set_maximized(Resource *resource)
{
    Q_EMIT q->maximizeRequested(true);
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_unset_maximized(Resource *resource)
{
    Q_EMIT q->maximizeRequested(false);
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_set_minimized(Resource *resource)
{
    Q_EMIT q->minimizeRequested(true);
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_unset_minimized(Resource *resource)
{
    Q_EMIT q->minimizeRequested(false);
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_activate(Resource *resource, wl_resource *seatResource)
{
    Q_EMIT q->activateRequested(SeatInterface::get(seatResource));
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_close(Resource *resource)
{
    Q_EMIT q->closeRequested();
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_set_rectangle(Resource *resource, wl_resource *surfaceResource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle, error_invalid_rectangle, "Invalid rectangle passed to set_rectangle");
        return;
    }
    Q_EMIT q->rectangleRequested(SurfaceInterface::get(surfaceResource), Rect(x, y, width, height));
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_set_fullscreen(Resource *resource, wl_resource *outputResource)
{
    Q_EMIT q->fullscreenRequested(true, OutputInterface::get(outputResource));
}

void ForeignToplevelHandleV1InterfacePrivate::zwlr_foreign_toplevel_handle_v1_unset_fullscreen(Resource *resource)
{
    Q_EMIT q->fullscreenRequested(false, nullptr);
}

ForeignToplevelManagerV1InterfacePrivate::ForeignToplevelManagerV1InterfacePrivate(ForeignToplevelManagerV1Interface *q, Display *display)
    : QtWaylandServer::zwlr_foreign_toplevel_manager_v1(*display, s_version)
    , q(q)
{
}

void ForeignToplevelManagerV1InterfacePrivate::zwlr_foreign_toplevel_manager_v1_bind_resource(Resource *resource)
{
    for (ForeignToplevelHandleV1Interface *toplevel : std::as_const(toplevels)) {
        auto *handleResource = toplevel->d->add(resource->client(), 0, resource->version());
        send_toplevel(resource->handle, handleResource->handle);
        toplevel->d->sendInitialState(handleResource);
    }
}

void ForeignToplevelManagerV1InterfacePrivate::zwlr_foreign_toplevel_manager_v1_stop(Resource *resource)
{
    if (stoppedResources.contains(resource->handle)) {
        return;
    }
    stoppedResources.insert(resource->handle);
    send_finished(resource->handle);
}

void ForeignToplevelManagerV1InterfacePrivate::zwlr_foreign_toplevel_manager_v1_destroy_resource(Resource *resource)
{
    stoppedResources.remove(resource->handle);
}

ForeignToplevelManagerV1Interface::ForeignToplevelManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ForeignToplevelManagerV1InterfacePrivate(this, display))
{
}

ForeignToplevelManagerV1Interface::~ForeignToplevelManagerV1Interface() = default;

ForeignToplevelHandleV1Interface *ForeignToplevelManagerV1Interface::createToplevel(QObject *parent)
{
    auto *toplevel = new ForeignToplevelHandleV1Interface(this, parent);
    d->toplevels.push_back(toplevel);

    connect(toplevel, &QObject::destroyed, this, [this, toplevel] {
        d->toplevels.removeAll(toplevel);
    });

    const auto resources = d->resourceMap();
    for (ForeignToplevelManagerV1InterfacePrivate::Resource *resource : resources) {
        if (d->stoppedResources.contains(resource->handle)) {
            continue;
        }
        auto *handleResource = toplevel->d->add(resource->client(), 0, resource->version());
        d->send_toplevel(resource->handle, handleResource->handle);
        toplevel->d->sendInitialState(handleResource);
    }

    return toplevel;
}

QList<ForeignToplevelHandleV1Interface *> ForeignToplevelManagerV1Interface::toplevels() const
{
    return d->toplevels;
}

ForeignToplevelHandleV1Interface::ForeignToplevelHandleV1Interface(ForeignToplevelManagerV1Interface *manager, QObject *parent)
    : QObject(parent)
    , d(new ForeignToplevelHandleV1InterfacePrivate(manager, this))
{
}

ForeignToplevelHandleV1Interface::~ForeignToplevelHandleV1Interface() = default;

void ForeignToplevelHandleV1Interface::setTitle(const QString &title)
{
    if (d->title == title) {
        return;
    }
    d->title = title;

    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_title(resource->handle, title);
        d->sendDone(resource->handle);
    }
}

void ForeignToplevelHandleV1Interface::setAppId(const QString &appId)
{
    if (d->appId == appId) {
        return;
    }
    d->appId = appId;

    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_app_id(resource->handle, appId);
        d->sendDone(resource->handle);
    }
}

void ForeignToplevelHandleV1Interface::setActive(bool active)
{
    d->setState(ForeignToplevelHandleV1InterfacePrivate::state_activated, active);
}

void ForeignToplevelHandleV1Interface::setMinimized(bool minimized)
{
    d->setState(ForeignToplevelHandleV1InterfacePrivate::state_minimized, minimized);
}

void ForeignToplevelHandleV1Interface::setMaximized(bool maximized)
{
    d->setState(ForeignToplevelHandleV1InterfacePrivate::state_maximized, maximized);
}

void ForeignToplevelHandleV1Interface::setFullscreen(bool fullscreen)
{
    d->setState(ForeignToplevelHandleV1InterfacePrivate::state_fullscreen, fullscreen);
}

void ForeignToplevelHandleV1Interface::setParent(ForeignToplevelHandleV1Interface *parent)
{
    if (d->parent == parent) {
        return;
    }

    d->parentDestroyConnection = {};
    d->parent = parent;
    if (parent) {
        d->parentDestroyConnection = connect(parent, &QObject::destroyed, this, [this] {
            setParent(nullptr);
        });
    }

    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        d->send_parent(resource->handle, d->resourceForParent(parent, resource));
        d->sendDone(resource->handle);
    }
}

void ForeignToplevelHandleV1Interface::setOutput(OutputInterface *output)
{
    if (d->output == output) {
        return;
    }

    const auto resources = d->resourceMap();
    for (auto *resource : resources) {
        if (d->output) {
            const auto outputResources = d->output->clientResources(resource->client());
            for (wl_resource *outputResource : outputResources) {
                d->send_output_leave(resource->handle, outputResource);
            }
        }
        if (output) {
            const auto outputResources = output->clientResources(resource->client());
            for (wl_resource *outputResource : outputResources) {
                d->send_output_enter(resource->handle, outputResource);
            }
        }
        d->sendDone(resource->handle);
    }

    d->output = output;
}

}

#include "moc_foreigntoplevel_v1.cpp"
