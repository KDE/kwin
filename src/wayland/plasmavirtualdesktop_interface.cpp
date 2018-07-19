/****************************************************************************
Copyright 2018  Marco Martin <notmart@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/
#include "plasmavirtualdesktop_interface.h"
#include "display.h"
#include "global_p.h"
#include "resource_p.h"

#include <QDebug>
#include <QTimer>

#include <wayland-server.h>
#include <wayland-plasma-virtual-desktop-server-protocol.h>

namespace KWayland
{
namespace Server
{

class Q_DECL_HIDDEN PlasmaVirtualDesktopInterface::Private
{
public:
    Private(PlasmaVirtualDesktopInterface *q, PlasmaVirtualDesktopManagementInterface *c);
    ~Private();
    void createResource(wl_resource *parent, quint32 serial);

    PlasmaVirtualDesktopInterface *q;
    PlasmaVirtualDesktopManagementInterface *vdm;

    QVector<wl_resource*> resources;
    QString id;
    QString name;
    bool active = false;

private:
    static void unbind(wl_resource *resource);
    static void requestActivateCallback(wl_client *client, wl_resource *resource);

    static Private *cast(wl_resource *resource) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    }

    wl_listener listener;
    static const struct org_kde_plasma_virtual_desktop_interface s_interface;
};


class Q_DECL_HIDDEN PlasmaVirtualDesktopManagementInterface::Private : public Global::Private
{
public:
    Private(PlasmaVirtualDesktopManagementInterface *q, Display *d);

    QVector<wl_resource*> resources;
    QList<PlasmaVirtualDesktopInterface*> desktops;
    quint32 rows = 0;
    quint32 columns = 0;

    inline QList<PlasmaVirtualDesktopInterface*>::const_iterator constFindDesktop(const QString &id);
    inline QList<PlasmaVirtualDesktopInterface*>::iterator findDesktop(const QString &id);
private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void getVirtualDesktopCallback(wl_client *client, wl_resource *resource, uint32_t serial, const char *id);
    static void requestCreateVirtualDesktopCallback(wl_client *client, wl_resource *resource, const char *name, uint32_t position);
    static void requestRemoveVirtualDesktopCallback(wl_client *client, wl_resource *resource, const char *id);

    PlasmaVirtualDesktopManagementInterface *q;

    static const struct org_kde_plasma_virtual_desktop_management_interface s_interface;
    static const quint32 s_version;
};

const quint32 PlasmaVirtualDesktopManagementInterface::Private::s_version = 1;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_plasma_virtual_desktop_management_interface PlasmaVirtualDesktopManagementInterface::Private::s_interface = {
    getVirtualDesktopCallback,
    requestCreateVirtualDesktopCallback,
    requestRemoveVirtualDesktopCallback
};
#endif

inline QList<PlasmaVirtualDesktopInterface*>::const_iterator PlasmaVirtualDesktopManagementInterface::Private::constFindDesktop(const QString &id)
{
    return std::find_if( desktops.constBegin(),
                         desktops.constEnd(),
                         [id]( const PlasmaVirtualDesktopInterface *desk ){ return desk->id() == id; } );
}

inline QList<PlasmaVirtualDesktopInterface*>::iterator PlasmaVirtualDesktopManagementInterface::Private::findDesktop(const QString &id)
{
    return std::find_if( desktops.begin(),
                         desktops.end(),
                         [id]( const PlasmaVirtualDesktopInterface *desk ){ return desk->id() == id; } );
}

void PlasmaVirtualDesktopManagementInterface::Private::getVirtualDesktopCallback(wl_client *client, wl_resource *resource, uint32_t serial, const char *id)
{
    Q_UNUSED(client)
    auto s = cast(resource);

    auto i = s->constFindDesktop(QString::fromUtf8(id));
    if (i == s->desktops.constEnd()) {
        return;
    }

    (*i)->d->createResource(resource, serial);
}

void PlasmaVirtualDesktopManagementInterface::Private::requestCreateVirtualDesktopCallback(wl_client *client, wl_resource *resource, const char *name, uint32_t position)
{
    Q_UNUSED(client)
    auto s = cast(resource);
    emit s->q->desktopCreateRequested(QString::fromUtf8(name), qBound<quint32>(0, position, (quint32)s->desktops.count()));
}

void PlasmaVirtualDesktopManagementInterface::Private::requestRemoveVirtualDesktopCallback(wl_client *client, wl_resource *resource, const char *id)
{
    Q_UNUSED(client)
    auto s = cast(resource);
    emit s->q->desktopRemoveRequested(QString::fromUtf8(id));
}

PlasmaVirtualDesktopManagementInterface::Private::Private(PlasmaVirtualDesktopManagementInterface *q, Display *d)
    : Global::Private(d, &org_kde_plasma_virtual_desktop_management_interface, s_version)
    , q(q)
{
}

void PlasmaVirtualDesktopManagementInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_plasma_virtual_desktop_management_interface, qMin(version, s_version), id);

    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    resources << resource;

    wl_resource_set_implementation(resource, &s_interface, this, unbind);

    quint32 i = 0;
    for (auto it = desktops.constBegin(); it != desktops.constEnd(); ++it) {
        org_kde_plasma_virtual_desktop_management_send_desktop_created(resource, (*it)->id().toUtf8().constData(), i++);
    }
    org_kde_plasma_virtual_desktop_management_send_done(resource);
}

void PlasmaVirtualDesktopManagementInterface::Private::unbind(wl_resource *resource)
{
    auto dm = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    dm->resources.removeAll(resource);
}

PlasmaVirtualDesktopManagementInterface::PlasmaVirtualDesktopManagementInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

PlasmaVirtualDesktopManagementInterface::~PlasmaVirtualDesktopManagementInterface()
{}

PlasmaVirtualDesktopManagementInterface::Private *PlasmaVirtualDesktopManagementInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

PlasmaVirtualDesktopInterface *PlasmaVirtualDesktopManagementInterface::desktop(const QString &id)
{
    Q_D();
    auto i = d->constFindDesktop(id);
    if (i != d->desktops.constEnd()) {
        return *i;
    }
    return nullptr;
}

PlasmaVirtualDesktopInterface *PlasmaVirtualDesktopManagementInterface::createDesktop(const QString &id, quint32 position)
{
    Q_D();
    auto i = d->constFindDesktop(id);
    if (i != d->desktops.constEnd()) {
        return *i;
    }

    const quint32 actualPosition = qMin(position, (quint32)d->desktops.count());
 
    PlasmaVirtualDesktopInterface *desktop = new PlasmaVirtualDesktopInterface(this);
    desktop->d->id = id;
    for (auto it = desktop->d->resources.constBegin(); it != desktop->d->resources.constEnd(); ++it) {
        org_kde_plasma_virtual_desktop_send_desktop_id(*it, id.toUtf8().constData());
    }

    //activate the first desktop TODO: to be done here?
    if (d->desktops.isEmpty()) {
        desktop->d->active = true;
    }

    d->desktops.insert(actualPosition, desktop);
    //NOTE: this in case the desktop has been deleted but not trough removedesktop
    connect(desktop, &QObject::destroyed, this,
        [this, id] {
            Q_D();
            auto i = d->findDesktop(id);
            if (i != d->desktops.end()) {
                for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
                    org_kde_plasma_virtual_desktop_management_send_desktop_removed(*it, id.toUtf8().constData());
                }

                d->desktops.erase(i);
            }
        }
    );

    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        org_kde_plasma_virtual_desktop_management_send_desktop_created(*it, id.toUtf8().constData(), actualPosition);
    }

    return desktop;
}

void PlasmaVirtualDesktopManagementInterface::removeDesktop(const QString &id)
{
    Q_D();
    auto deskIt = d->findDesktop(id);
    if (deskIt == d->desktops.end()) {
        return;
    }

    for (auto it = (*deskIt)->d->resources.constBegin(); it != (*deskIt)->d->resources.constEnd(); ++it) {
        org_kde_plasma_virtual_desktop_send_removed(*it);
    }

    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        org_kde_plasma_virtual_desktop_management_send_desktop_removed(*it, id.toUtf8().constData());
    }

    d->desktops.erase(deskIt);
    (*deskIt)->deleteLater();
}

QList <PlasmaVirtualDesktopInterface *> PlasmaVirtualDesktopManagementInterface::desktops() const
{
    Q_D();
    return d->desktops;
}

void PlasmaVirtualDesktopManagementInterface::sendDone()
{
    Q_D();
    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        org_kde_plasma_virtual_desktop_management_send_done(*it);
    }
}

//// PlasmaVirtualDesktopInterface

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_plasma_virtual_desktop_interface PlasmaVirtualDesktopInterface::Private::s_interface = {
    requestActivateCallback
};
#endif

void PlasmaVirtualDesktopInterface::Private::requestActivateCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    auto s = cast(resource);
    emit s->q->activateRequested();
}

PlasmaVirtualDesktopInterface::Private::Private(PlasmaVirtualDesktopInterface *q, PlasmaVirtualDesktopManagementInterface *c)
    : q(q),
      vdm(c)
{
}

PlasmaVirtualDesktopInterface::Private::~Private()
{
   // need to copy, as destroy goes through the destroy listener and modifies the list as we iterate
    const auto c = resources;
    for (const auto &r : c) {
        auto client = wl_resource_get_client(r);
        org_kde_plasma_virtual_desktop_send_removed(r);
        wl_resource_destroy(r);
        wl_client_flush(client);
    }
}

void PlasmaVirtualDesktopInterface::Private::unbind(wl_resource *resource)
{
    Private *p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    p->resources.removeAll(resource);
}

void PlasmaVirtualDesktopInterface::Private::createResource(wl_resource *parent, quint32 serial)
{
    ClientConnection *c = vdm->display()->getConnection(wl_resource_get_client(parent));
    wl_resource *resource = c->createResource(&org_kde_plasma_virtual_desktop_interface, wl_resource_get_version(parent), serial);
    if (!resource) {
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    resources << resource;

    org_kde_plasma_virtual_desktop_send_desktop_id(resource, id.toUtf8().constData());
    if (!name.isEmpty()) {
        org_kde_plasma_virtual_desktop_send_name(resource, name.toUtf8().constData());
    }

    if (active) {
        org_kde_plasma_virtual_desktop_send_activated(resource);
    }

    c->flush();
}

PlasmaVirtualDesktopInterface::PlasmaVirtualDesktopInterface(PlasmaVirtualDesktopManagementInterface *parent)
    : QObject(parent),
      d(new Private(this, parent))
{
}

PlasmaVirtualDesktopInterface::~PlasmaVirtualDesktopInterface()
{}

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
    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        org_kde_plasma_virtual_desktop_send_name(*it, name.toUtf8().constData());
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
    if (active) {
        for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
            org_kde_plasma_virtual_desktop_send_activated(*it);
        }
    } else {
        for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
            org_kde_plasma_virtual_desktop_send_deactivated(*it);
        }
    }
}

bool PlasmaVirtualDesktopInterface::isActive() const
{
    return d->active;
}

void PlasmaVirtualDesktopInterface::sendDone()
{
    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        org_kde_plasma_virtual_desktop_send_done(*it);
    }
}

}
}

