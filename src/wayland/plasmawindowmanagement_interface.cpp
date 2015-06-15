/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
*********************************************************************/
#include "plasmawindowmanagement_interface.h"
#include "global_p.h"
#include "resource_p.h"
#include "display.h"

#include <QList>
#include <QVector>

#include <wayland-server.h>
#include <wayland-plasma-window-management-server-protocol.h>

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 1;

class PlasmaWindowManagementInterface::Private : public Global::Private
{
public:
    Private(PlasmaWindowManagementInterface *q, Display *d);
    void sendShowingDesktopState();

    ShowingDesktopState state = ShowingDesktopState::Disabled;
    QVector<wl_resource*> resources;
    QList<PlasmaWindowInterface*> windows;

private:
    static void unbind(wl_resource *resource);
    static void showDesktopCallback(wl_client *client, wl_resource *resource, uint32_t state);

    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void sendShowingDesktopState(wl_resource *r);

    PlasmaWindowManagementInterface *q;
    static const struct org_kde_plasma_window_management_interface s_interface;
};

class PlasmaWindowInterface::Private
{
public:
    Private(PlasmaWindowManagementInterface *wm, PlasmaWindowInterface *q);
    ~Private();

    void createResource(wl_resource *parent);
    void setTitle(const QString &title);
    void setAppId(const QString &appId);
    void setVirtualDesktop(quint32 desktop);

    struct WindowResource {
        wl_resource *resource;
        wl_listener *destroyListener;
    };
    QList<WindowResource> resources;

private:
    static void unbind(wl_resource *resource);
    static void destroyListenerCallback(wl_listener *listener, void *data);

    PlasmaWindowInterface *q;
    PlasmaWindowManagementInterface *wm;
    QString m_title;
    QString m_appId;
    quint32 m_virtualDesktop = 0;
    wl_listener listener;
//     static const struct org_kde_plasma_window_interface s_interface;
};

PlasmaWindowManagementInterface::Private::Private(PlasmaWindowManagementInterface *q, Display *d)
    : Global::Private(d, &org_kde_plasma_window_management_interface, s_version)
    , q(q)
{
}

const struct org_kde_plasma_window_management_interface PlasmaWindowManagementInterface::Private::s_interface = {
    showDesktopCallback
};

void PlasmaWindowManagementInterface::Private::sendShowingDesktopState()
{
    for (wl_resource *r : resources) {
        sendShowingDesktopState(r);
    }
}

void PlasmaWindowManagementInterface::Private::sendShowingDesktopState(wl_resource *r)
{
    uint32_t s = 0;
    switch (state) {
    case ShowingDesktopState::Enabled:
        s = ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED;
        break;
    case ShowingDesktopState::Disabled:
        s = ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    org_kde_plasma_window_management_send_show_desktop_changed(r, s);
}

void PlasmaWindowManagementInterface::Private::showDesktopCallback(wl_client *client, wl_resource *resource, uint32_t state)
{
    Q_UNUSED(client)
    ShowingDesktopState s = ShowingDesktopState::Disabled;
    switch (state) {
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED:
        s = ShowingDesktopState::Enabled;
        break;
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED:
    default:
        s = ShowingDesktopState::Disabled;
        break;
    }
    emit reinterpret_cast<Private*>(wl_resource_get_user_data(resource))->q->requestChangeShowingDesktop(s);
}

PlasmaWindowManagementInterface::PlasmaWindowManagementInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

PlasmaWindowManagementInterface::~PlasmaWindowManagementInterface() = default;

void PlasmaWindowManagementInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *shell = c->createResource(&org_kde_plasma_window_management_interface, qMin(version, s_version), id);
    if (!shell) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(shell, &s_interface, this, unbind);
    resources << shell;
    for (auto it = windows.constBegin(); it != windows.constEnd(); ++it) {
        (*it)->d->createResource(shell);
    }
}

void PlasmaWindowManagementInterface::Private::unbind(wl_resource *resource)
{
    auto wm = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    wm->resources.removeAll(resource);
}

void PlasmaWindowManagementInterface::setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState state)
{
    Q_D();
    if (d->state == state) {
        return;
    }
    d->state = state;
    d->sendShowingDesktopState();
}

PlasmaWindowManagementInterface::Private *PlasmaWindowManagementInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

PlasmaWindowInterface *PlasmaWindowManagementInterface::createWindow(QObject *parent)
{
    Q_D();
    PlasmaWindowInterface *window = new PlasmaWindowInterface(this, parent);
    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        window->d->createResource(*it);
    }
    d->windows << window;
    connect(window, &QObject::destroyed, this,
        [this, window] {
            Q_D();
            d->windows.removeAll(window);
        }
    );
    return window;
}

// const struct org_kde_plasma_window_interface PlasmaWindowInterface::Private::s_interface = {
// };

PlasmaWindowInterface::Private::Private(PlasmaWindowManagementInterface *wm, PlasmaWindowInterface *q)
    : q(q)
    , wm(wm)
{
}

PlasmaWindowInterface::Private::~Private()
{
    // need to copy, as destroy goes through the destroy listener and modifies the list as we iterate
    const auto c = resources;
    for (const auto &r : c) {
        org_kde_plasma_window_send_unmapped(r.resource);
        wl_resource_destroy(r.resource);
    }
}

void PlasmaWindowInterface::Private::unbind(wl_resource *resource)
{
    Private *p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    auto it = p->resources.begin();
    while (it != p->resources.end()) {
        if ((*it).resource == resource) {
            wl_list_remove(&(*it).destroyListener->link);
            delete (*it).destroyListener;
            it = p->resources.erase(it);
        } else {
            it++;
        }
    }
}

void PlasmaWindowInterface::Private::destroyListenerCallback(wl_listener *listener, void *data)
{
    Q_UNUSED(listener);
    Private::unbind(reinterpret_cast<wl_resource*>(data));
}

void PlasmaWindowInterface::Private::createResource(wl_resource *parent)
{
    ClientConnection *c = wm->display()->getConnection(wl_resource_get_client(parent));
    wl_resource *resource = c->createResource(&org_kde_plasma_window_interface, wl_resource_get_version(parent), 0);
    if (!resource) {
        return;
    }
    WindowResource r;
    r.resource = resource;
    r.destroyListener = new wl_listener;
    r.destroyListener->notify = destroyListenerCallback;
    r.destroyListener->link.prev = nullptr;
    r.destroyListener->link.next = nullptr;
    wl_resource_set_implementation(resource, nullptr, this, unbind);
    wl_resource_add_destroy_listener(resource, r.destroyListener);
    resources << r;

    org_kde_plasma_window_management_send_window_created(parent, resource);
    org_kde_plasma_window_send_virtual_desktop_changed(resource, m_virtualDesktop);
    if (!m_appId.isEmpty()) {
        org_kde_plasma_window_send_app_id_changed(resource, m_appId.toUtf8().constData());
    }
    if (!m_title.isEmpty()) {
        org_kde_plasma_window_send_title_changed(resource, m_title.toUtf8().constData());
    }
    c->flush();
}

void PlasmaWindowInterface::Private::setAppId(const QString &appId)
{
    if (m_appId == appId) {
        return;
    }
    m_appId = appId;
    const QByteArray utf8 = m_appId.toUtf8();
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_app_id_changed((*it).resource, utf8.constData());
    }
}

void PlasmaWindowInterface::Private::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    const QByteArray utf8 = m_title.toUtf8();
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_title_changed((*it).resource, utf8.constData());
    }
}

void PlasmaWindowInterface::Private::setVirtualDesktop(quint32 desktop)
{
    if (m_virtualDesktop == desktop) {
        return;
    }
    m_virtualDesktop = desktop;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_virtual_desktop_changed((*it).resource, m_virtualDesktop);
    }
}

PlasmaWindowInterface::PlasmaWindowInterface(PlasmaWindowManagementInterface *wm, QObject *parent)
    : QObject(parent)
    , d(new Private(wm, this))
{
}

PlasmaWindowInterface::~PlasmaWindowInterface() = default;

void PlasmaWindowInterface::setAppId(const QString &appId)
{
    d->setAppId(appId);
}

void PlasmaWindowInterface::setTitle(const QString &title)
{
    d->setTitle(title);
}

void PlasmaWindowInterface::setVirtualDesktop(quint32 desktop)
{
    d->setVirtualDesktop(desktop);
}

}
}
