/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/virtualdesktop_v2.h"
#include "wayland/display.h"
#include "wayland/qwayland-server-kde-virtual-desktop-v2.h"

#include <QHash>
#include <QTimer>

namespace KWin
{

static const quint32 s_version = 1;

class VirtualDesktopManagerV2Private : public QtWaylandServer::kde_virtual_desktop_manager_v2
{
public:
    VirtualDesktopManagerV2Private(VirtualDesktopManagerV2 *q, Display *display);

    VirtualDesktopManagerV2 *q;
    QHash<QString, VirtualDesktopV2 *> desktops;
    QTimer doneTimer;
    uint rows = 0;

protected:
    void kde_virtual_desktop_manager_v2_bind_resource(Resource *resource) override;
    void kde_virtual_desktop_manager_v2_create(Resource *resource, const QString &name, uint32_t position) override;
    void kde_virtual_desktop_manager_v2_stop(Resource *resource) override;
};

class VirtualDesktopV2Private : public QtWaylandServer::kde_virtual_desktop_v2
{
public:
    VirtualDesktopV2Private(const QString &id, const QString &name, uint position, VirtualDesktopV2 *q);
    ~VirtualDesktopV2Private() override;

    void introduce(wl_resource *manager);

    void sendId(Resource *resource);
    void sendName(Resource *resource);
    void sendActive(Resource *resource);
    void sendPosition(Resource *resource);

    VirtualDesktopV2 *q;
    QString id;
    QString name;
    bool active = false;
    uint position = 0;

protected:
    void kde_virtual_desktop_v2_destroy(Resource *resource) override;
    void kde_virtual_desktop_v2_activate(Resource *resource) override;
    void kde_virtual_desktop_v2_remove(Resource *resource) override;
};

VirtualDesktopManagerV2Private::VirtualDesktopManagerV2Private(VirtualDesktopManagerV2 *q, Display *display)
    : QtWaylandServer::kde_virtual_desktop_manager_v2(*display, s_version)
    , q(q)
{
}

void VirtualDesktopManagerV2Private::kde_virtual_desktop_manager_v2_bind_resource(Resource *resource)
{
    for (const auto &desktop : desktops) {
        desktop->introduce(resource->handle);
    }

    send_rows(resource->handle, rows);
    send_done(resource->handle);
}

void VirtualDesktopManagerV2Private::kde_virtual_desktop_manager_v2_create(Resource *resource, const QString &name, uint32_t position)
{
    Q_EMIT q->createRequested(name, position);
}

void VirtualDesktopManagerV2Private::kde_virtual_desktop_manager_v2_stop(Resource *resource)
{
    send_finished(resource->handle);
    wl_resource_destroy(resource->handle);
}

VirtualDesktopManagerV2::VirtualDesktopManagerV2(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<VirtualDesktopManagerV2Private>(this, display))
{
    d->doneTimer.setSingleShot(true);
    d->doneTimer.setInterval(0);
    connect(&d->doneTimer, &QTimer::timeout, this, [this]() {
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->send_done(resource->handle);
        }
    });
}

VirtualDesktopManagerV2::~VirtualDesktopManagerV2()
{
    const auto desktops = std::exchange(d->desktops, {});
    for (const auto &desktop : desktops) {
        desktop->destroy();
    }
}

void VirtualDesktopManagerV2::setRows(uint rows)
{
    if (d->rows == rows) {
        return;
    }

    d->rows = rows;

    const auto clients = d->resourceMap();
    for (const auto client : clients) {
        d->send_rows(client->handle, rows);
    }
}

QHash<QString, VirtualDesktopV2 *> VirtualDesktopManagerV2::desktops() const
{
    return d->desktops;
}

VirtualDesktopV2 *VirtualDesktopManagerV2::add(const QString &id, const QString &name, uint position)
{
    auto desktop = new VirtualDesktopV2(id, name, position, this);
    d->desktops[id] = desktop;

    const auto clients = d->resourceMap();
    for (const auto &client : clients) {
        desktop->introduce(client->handle);
    }

    return desktop;
}

void VirtualDesktopManagerV2::remove(const QString &id)
{
    if (VirtualDesktopV2 *desktop = d->desktops.take(id)) {
        desktop->remove();
    }
}

void VirtualDesktopManagerV2::scheduleDone()
{
    if (!d->doneTimer.isActive()) {
        d->doneTimer.start();
    }
}

VirtualDesktopV2Private::VirtualDesktopV2Private(const QString &id, const QString &name, uint position, VirtualDesktopV2 *q)
    : q(q)
    , id(id)
    , name(name)
    , position(position)
{
}

VirtualDesktopV2Private::~VirtualDesktopV2Private()
{
}

void VirtualDesktopV2Private::sendId(Resource *resource)
{
    send_id(resource->handle, id);
}

void VirtualDesktopV2Private::sendName(Resource *resource)
{
    send_name(resource->handle, name);
}

void VirtualDesktopV2Private::sendActive(Resource *resource)
{
    send_active(resource->handle, active);
}

void VirtualDesktopV2Private::sendPosition(Resource *resource)
{
    send_position(resource->handle, position);
}

void VirtualDesktopV2Private::kde_virtual_desktop_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void VirtualDesktopV2Private::kde_virtual_desktop_v2_activate(Resource *resource)
{
    Q_EMIT q->activateRequested();
}

void VirtualDesktopV2Private::kde_virtual_desktop_v2_remove(Resource *resource)
{
    Q_EMIT q->removeRequested();
}

VirtualDesktopV2::VirtualDesktopV2(const QString &id, const QString &name, uint position, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<VirtualDesktopV2Private>(id, name, position, this))
{
}

VirtualDesktopV2::~VirtualDesktopV2()
{
}

void VirtualDesktopV2::introduce(wl_resource *manager)
{
    auto desktop = d->add(wl_resource_get_client(manager), 0, wl_resource_get_version(manager));
    kde_virtual_desktop_manager_v2_send_desktop(manager, desktop->handle);

    d->sendId(desktop);
    d->sendName(desktop);
    d->sendActive(desktop);
    d->sendPosition(desktop);
}

void VirtualDesktopV2::destroy()
{
    delete this;
}

void VirtualDesktopV2::remove()
{
    const auto desktops = d->resourceMap();
    for (const auto &desktop : desktops) {
        d->send_removed(desktop->handle);
    }

    delete this;
}

void VirtualDesktopV2::setName(const QString &name)
{
    if (d->name == name) {
        return;
    }

    d->name = name;

    const auto desktops = d->resourceMap();
    for (const auto &desktop : desktops) {
        d->sendName(desktop);
    }
}

void VirtualDesktopV2::setActive(bool active)
{
    if (d->active == active) {
        return;
    }

    d->active = active;

    const auto desktops = d->resourceMap();
    for (const auto &desktop : desktops) {
        d->sendActive(desktop);
    }
}

void VirtualDesktopV2::setPosition(uint position)
{
    if (d->position == position) {
        return;
    }

    d->position = position;

    const auto desktops = d->resourceMap();
    for (const auto &desktop : desktops) {
        d->sendPosition(desktop);
    }
}

} // namespace KWin

#include "moc_virtualdesktop_v2.cpp"
