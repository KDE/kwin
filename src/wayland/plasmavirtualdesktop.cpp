/*
    SPDX-FileCopyrightText: 2018 Marco Martin <notmart@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "plasmavirtualdesktop.h"
#include "display.h"
#include "wayland/quirks.h"

#include <QDebug>
#include <QTimer>

#include <qwayland-server-org-kde-plasma-virtual-desktop.h>
#include <wayland-server.h>

namespace KWin
{
static const quint32 s_version = 2;

class PlasmaVirtualDesktopInterfacePrivate : public QtWaylandServer::org_kde_plasma_virtual_desktop
{
public:
    PlasmaVirtualDesktopInterfacePrivate(PlasmaVirtualDesktopInterface *q);
    ~PlasmaVirtualDesktopInterfacePrivate();

    PlasmaVirtualDesktopInterface *q;
    QString id;
    QString name;
    bool active = false;

protected:
    void org_kde_plasma_virtual_desktop_bind_resource(Resource *resource) override;
    void org_kde_plasma_virtual_desktop_request_activate(Resource *resource) override;
};

class PlasmaVirtualDesktopManagementInterfacePrivate : public QtWaylandServer::org_kde_plasma_virtual_desktop_management
{
public:
    PlasmaVirtualDesktopManagementInterfacePrivate(PlasmaVirtualDesktopManagementInterface *_q, Display *display);

    QList<PlasmaVirtualDesktopInterface *> desktops;
    quint32 rows = 0;
    quint32 columns = 0;
    PlasmaVirtualDesktopManagementInterface *q;

    inline QList<PlasmaVirtualDesktopInterface *>::const_iterator constFindDesktop(const QString &id);
    inline QList<PlasmaVirtualDesktopInterface *>::iterator findDesktop(const QString &id);

protected:
    void org_kde_plasma_virtual_desktop_management_get_virtual_desktop(Resource *resource, uint32_t id, const QString &desktop_id) override;
    void org_kde_plasma_virtual_desktop_management_request_create_virtual_desktop(Resource *resource, const QString &name, uint32_t position) override;
    void org_kde_plasma_virtual_desktop_management_request_remove_virtual_desktop(Resource *resource, const QString &desktop_id) override;
    void org_kde_plasma_virtual_desktop_management_bind_resource(Resource *resource) override;
};

inline QList<PlasmaVirtualDesktopInterface *>::const_iterator PlasmaVirtualDesktopManagementInterfacePrivate::constFindDesktop(const QString &id)
{
    return std::find_if(desktops.constBegin(), desktops.constEnd(), [id](const PlasmaVirtualDesktopInterface *desk) {
        return desk->id() == id;
    });
}

inline QList<PlasmaVirtualDesktopInterface *>::iterator PlasmaVirtualDesktopManagementInterfacePrivate::findDesktop(const QString &id)
{
    return std::find_if(desktops.begin(), desktops.end(), [id](const PlasmaVirtualDesktopInterface *desk) {
        return desk->id() == id;
    });
}

void PlasmaVirtualDesktopManagementInterfacePrivate::org_kde_plasma_virtual_desktop_management_get_virtual_desktop(Resource *resource,
                                                                                                                   uint32_t id,
                                                                                                                   const QString &desktop_id)
{
    auto i = constFindDesktop(desktop_id);
    if (i == desktops.constEnd()) {
        return;
    }

    (*i)->d->add(resource->client(), id, resource->version());
}

void PlasmaVirtualDesktopManagementInterfacePrivate::org_kde_plasma_virtual_desktop_management_request_create_virtual_desktop(Resource *resource,
                                                                                                                              const QString &name,
                                                                                                                              uint32_t position)
{
    Q_EMIT q->desktopCreateRequested(name, std::clamp<quint32>(position, 0, desktops.size()));
}

void PlasmaVirtualDesktopManagementInterfacePrivate::org_kde_plasma_virtual_desktop_management_request_remove_virtual_desktop(Resource *resource,
                                                                                                                              const QString &desktop_id)
{
    Q_EMIT q->desktopRemoveRequested(desktop_id);
}

PlasmaVirtualDesktopManagementInterfacePrivate::PlasmaVirtualDesktopManagementInterfacePrivate(PlasmaVirtualDesktopManagementInterface *_q, Display *display)
    : QtWaylandServer::org_kde_plasma_virtual_desktop_management(*display, s_version)
    , q(_q)
{
}

void PlasmaVirtualDesktopManagementInterfacePrivate::org_kde_plasma_virtual_desktop_management_bind_resource(Resource *resource)
{
    quint32 i = 0;
    for (auto it = desktops.constBegin(); it != desktops.constEnd(); ++it) {
        send_desktop_created(resource->handle, (*it)->id(), i++);
    }

    if (resource->version() >= ORG_KDE_PLASMA_VIRTUAL_DESKTOP_MANAGEMENT_ROWS_SINCE_VERSION) {
        send_rows(resource->handle, rows);
    }

    send_done(resource->handle);
}

PlasmaVirtualDesktopManagementInterface::PlasmaVirtualDesktopManagementInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PlasmaVirtualDesktopManagementInterfacePrivate(this, display))
{
}

PlasmaVirtualDesktopManagementInterface::~PlasmaVirtualDesktopManagementInterface()
{
    while (!d->desktops.isEmpty()) {
        const QString id = d->desktops[0]->id();
        removeDesktop(id);
    }
}

void PlasmaVirtualDesktopManagementInterface::setRows(quint32 rows)
{
    if (rows == 0 || d->rows == rows) {
        return;
    }

    d->rows = rows;

    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() < ORG_KDE_PLASMA_VIRTUAL_DESKTOP_MANAGEMENT_ROWS_SINCE_VERSION) {
            continue;
        }
        d->send_rows(resource->handle, rows);
    }
}

PlasmaVirtualDesktopInterface *PlasmaVirtualDesktopManagementInterface::desktop(const QString &id)
{
    auto i = d->constFindDesktop(id);
    if (i != d->desktops.constEnd()) {
        return *i;
    }
    return nullptr;
}

PlasmaVirtualDesktopInterface *PlasmaVirtualDesktopManagementInterface::createDesktop(const QString &id, quint32 position)
{
    auto i = d->constFindDesktop(id);
    if (i != d->desktops.constEnd()) {
        return *i;
    }

    const quint32 actualPosition = std::min(position, (quint32)d->desktops.count());

    auto desktop = new PlasmaVirtualDesktopInterface();
    desktop->d->id = id;

    const auto desktopClientResources = desktop->d->resourceMap();
    for (auto resource : desktopClientResources) {
        desktop->d->send_desktop_id(resource->handle, id);
    }

    // activate the first desktop TODO: to be done here?
    if (d->desktops.isEmpty()) {
        desktop->d->active = true;
    }

    d->desktops.insert(actualPosition, desktop);

    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_desktop_created(resource->handle, id, actualPosition);
    }

    return desktop;
}

void PlasmaVirtualDesktopManagementInterface::removeDesktop(const QString &id)
{
    auto deskIt = d->findDesktop(id);
    if (deskIt == d->desktops.end()) {
        return;
    }

    PlasmaVirtualDesktopInterface *desktop = *deskIt;
    d->desktops.erase(deskIt);
    delete desktop;

    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_desktop_removed(resource->handle, id);
    }
}

QList<PlasmaVirtualDesktopInterface *> PlasmaVirtualDesktopManagementInterface::desktops() const
{
    return d->desktops;
}

void PlasmaVirtualDesktopManagementInterface::sendDone()
{
    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_done(resource->handle);
    }
}

//// PlasmaVirtualDesktopInterface

void PlasmaVirtualDesktopInterfacePrivate::org_kde_plasma_virtual_desktop_request_activate(Resource *resource)
{
    Q_EMIT q->activateRequested();
}

PlasmaVirtualDesktopInterfacePrivate::PlasmaVirtualDesktopInterfacePrivate(PlasmaVirtualDesktopInterface *q)
    : QtWaylandServer::org_kde_plasma_virtual_desktop()
    , q(q)
{
}

PlasmaVirtualDesktopInterfacePrivate::~PlasmaVirtualDesktopInterfacePrivate()
{
    const auto clientResources = resourceMap();
    for (Resource *resource : clientResources) {
        send_removed(resource->handle);
        wl_resource_destroy(resource->handle);
    }
}

void PlasmaVirtualDesktopInterfacePrivate::org_kde_plasma_virtual_desktop_bind_resource(Resource *resource)
{
    send_desktop_id(resource->handle, id);

    if (!name.isEmpty()) {
        send_name(resource->handle, truncate(name));
    }

    if (active) {
        send_activated(resource->handle);
    }
}

PlasmaVirtualDesktopInterface::PlasmaVirtualDesktopInterface()
    : d(new PlasmaVirtualDesktopInterfacePrivate(this))
{
}

PlasmaVirtualDesktopInterface::~PlasmaVirtualDesktopInterface()
{
}

QString PlasmaVirtualDesktopInterface::id() const
{
    return d->id;
}

void PlasmaVirtualDesktopInterface::setName(const QString &name)
{
    if (d->name == name) {
        return;
    }

    d->name = name;

    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_name(resource->handle, truncate(name));
    }
}

QString PlasmaVirtualDesktopInterface::name() const
{
    return d->name;
}

void PlasmaVirtualDesktopInterface::setActive(bool active)
{
    if (d->active == active) {
        return;
    }

    d->active = active;
    const auto clientResources = d->resourceMap();

    if (active) {
        for (auto resource : clientResources) {
            d->send_activated(resource->handle);
        }
    } else {
        for (auto resource : clientResources) {
            d->send_deactivated(resource->handle);
        }
    }
}

bool PlasmaVirtualDesktopInterface::isActive() const
{
    return d->active;
}

void PlasmaVirtualDesktopInterface::sendDone()
{
    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_done(resource->handle);
    }
}

}

#include "moc_plasmavirtualdesktop.cpp"
