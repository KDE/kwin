/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "plasmawindowmanagement_interface.h"
#include "global_p.h"
#include "resource_p.h"
#include "display.h"
#include "surface_interface.h"
#include "plasmavirtualdesktop_interface.h"

#include <QtConcurrentRun>
#include <QFile>
#include <QIcon>
#include <QList>
#include <QVector>
#include <QRect>
#include <QHash>

#include <wayland-server.h>
#include <wayland-plasma-window-management-server-protocol.h>

namespace KWaylandServer
{

class PlasmaWindowManagementInterface::Private : public Global::Private
{
public:
    Private(PlasmaWindowManagementInterface *q, Display *d);
    void sendShowingDesktopState();
    void sendStackingOrderChanged();

    ShowingDesktopState state = ShowingDesktopState::Disabled;
    QVector<wl_resource*> resources;
    QList<PlasmaWindowInterface*> windows;
    QPointer<PlasmaVirtualDesktopManagementInterface> plasmaVirtualDesktopManagementInterface = nullptr;
    quint32 windowIdCounter = 0;
    QVector<quint32> stackingOrder;

private:
    static void unbind(wl_resource *resource);
    static void showDesktopCallback(wl_client *client, wl_resource *resource, uint32_t state);
    static void getWindowCallback(wl_client *client, wl_resource *resource, uint32_t id, uint32_t internalWindowId);

    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void sendShowingDesktopState(wl_resource *r);
    void sendStackingOrderChanged(wl_resource *r);

    PlasmaWindowManagementInterface *q;
    static const struct org_kde_plasma_window_management_interface s_interface;
    static const quint32 s_version;
};

class PlasmaWindowInterface::Private
{
public:
    Private(PlasmaWindowManagementInterface *wm, PlasmaWindowInterface *q);
    ~Private();

    void createResource(wl_resource *parent, uint32_t id);
    void setTitle(const QString &title);
    void setAppId(const QString &appId);
    void setPid(quint32 pid);
    void setThemedIconName(const QString &iconName);
    void setIcon(const QIcon &icon);
    void setVirtualDesktop(quint32 desktop);
    void unmap();
    void setState(org_kde_plasma_window_management_state flag, bool set);
    void setParentWindow(PlasmaWindowInterface *parent);
    void setGeometry(const QRect &geometry);
    void setApplicationMenuPaths(const QString &service, const QString &object);
    wl_resource *resourceForParent(PlasmaWindowInterface *parent, wl_resource *child) const;

    QVector<wl_resource*> resources;
    quint32 windowId = 0;
    QHash<SurfaceInterface*, QRect> minimizedGeometries;
    PlasmaWindowManagementInterface *wm;

    bool unmapped = false;
    PlasmaWindowInterface *parentWindow = nullptr;
    QMetaObject::Connection parentWindowDestroyConnection;
    QStringList plasmaVirtualDesktops;
    QRect geometry;

private:
    static void unbind(wl_resource *resource);
    static void setStateCallback(wl_client *client, wl_resource *resource, uint32_t flags, uint32_t state);
    static void setVirtualDesktopCallback(wl_client *client, wl_resource *resource, uint32_t number);
    static void closeCallback(wl_client *client, wl_resource *resource);
    static void requestMoveCallback(wl_client *client, wl_resource *resource);
    static void requestResizeCallback(wl_client *client, wl_resource *resource);
    static void setMinimizedGeometryCallback(wl_client *client, wl_resource *resource, wl_resource *panel, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    static void unsetMinimizedGeometryCallback(wl_client *client, wl_resource *resource, wl_resource *panel);
    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void getIconCallback(wl_client *client, wl_resource *resource, int32_t fd);
    static void requestEnterVirtualDesktopCallback(wl_client *client, wl_resource *resource, const char *id);
    static void requestEnterNewVirtualDesktopCallback(wl_client *client, wl_resource *resource);
    static void requestLeaveVirtualDesktopCallback(wl_client *client, wl_resource *resource, const char *id);
    static Private *cast(wl_resource *resource) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    }

    PlasmaWindowInterface *q;
    QString m_title;
    QString m_appId;
    quint32 m_pid = 0;
    QString m_themedIconName;
    QString m_appServiceName;
    QString m_appObjectPath;
    QIcon m_icon;
    quint32 m_virtualDesktop = 0;
    quint32 m_state = 0;
    static const struct org_kde_plasma_window_interface s_interface;
};

const quint32 PlasmaWindowManagementInterface::Private::s_version = 11;

PlasmaWindowManagementInterface::Private::Private(PlasmaWindowManagementInterface *q, Display *d)
    : Global::Private(d, &org_kde_plasma_window_management_interface, s_version)
    , q(q)
{
}

#ifndef K_DOXYGEN
const struct org_kde_plasma_window_management_interface PlasmaWindowManagementInterface::Private::s_interface = {
    showDesktopCallback,
    getWindowCallback
};
#endif

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

void PlasmaWindowManagementInterface::Private::sendStackingOrderChanged()
{
    for (wl_resource *r : resources) {
        sendStackingOrderChanged(r);
    }
}

void PlasmaWindowManagementInterface::Private::sendStackingOrderChanged(wl_resource *r)
{
    if (wl_resource_get_version(r) < ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STACKING_ORDER_CHANGED_SINCE_VERSION) {
        return;
    }

    wl_array wlIds;
    wl_array_init(&wlIds);
    const size_t memLength = sizeof(uint32_t) * stackingOrder.size();
    void *s = wl_array_add(&wlIds, memLength);
    memcpy(s, stackingOrder.data(), memLength);

    org_kde_plasma_window_management_send_stacking_order_changed(r, &wlIds);

    wl_array_release(&wlIds);
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

void PlasmaWindowManagementInterface::Private::getWindowCallback(wl_client *client, wl_resource *resource, uint32_t id, uint32_t internalWindowId)
{
    Q_UNUSED(client)
    auto p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    auto it = std::find_if(p->windows.constBegin(), p->windows.constEnd(),
        [internalWindowId] (PlasmaWindowInterface *window) {
            return window->d->windowId == internalWindowId;
        }
    );
    if (it == p->windows.constEnd()) {
        // create a temp window just for the resource and directly send an unmapped
        PlasmaWindowInterface *window = new PlasmaWindowInterface(p->q, p->q);
        window->d->unmapped = true;
        window->d->createResource(resource, id);
        return;
    }
    (*it)->d->createResource(resource, id);
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
        org_kde_plasma_window_management_send_window(shell, (*it)->d->windowId);
    }
    sendStackingOrderChanged();
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
    // TODO: improve window ids so that it cannot wrap around
    window->d->windowId = ++d->windowIdCounter;
    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        org_kde_plasma_window_management_send_window(*it, window->d->windowId);
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

QList<PlasmaWindowInterface*> PlasmaWindowManagementInterface::windows() const
{
    Q_D();
    return d->windows;
}

void PlasmaWindowManagementInterface::unmapWindow(PlasmaWindowInterface *window)
{
    if (!window) {
        return;
    }
    Q_D();
    d->windows.removeOne(window);
    Q_ASSERT(!d->windows.contains(window));
    window->d->unmap();
}

void PlasmaWindowManagementInterface::setStackingOrder(const QVector<quint32>& stackingOrder)
{
    Q_D();
    if (d->stackingOrder == stackingOrder) {
        return;
    }
    d->stackingOrder = stackingOrder;
    d->sendStackingOrderChanged();
}

void PlasmaWindowManagementInterface::setPlasmaVirtualDesktopManagementInterface(PlasmaVirtualDesktopManagementInterface *manager)
{
    Q_D();
    if (d->plasmaVirtualDesktopManagementInterface == manager) {
        return;
    }
    d->plasmaVirtualDesktopManagementInterface = manager;
}

PlasmaVirtualDesktopManagementInterface *PlasmaWindowManagementInterface::plasmaVirtualDesktopManagementInterface() const
{
    Q_D();
    return d->plasmaVirtualDesktopManagementInterface;
}

#ifndef K_DOXYGEN
const struct org_kde_plasma_window_interface PlasmaWindowInterface::Private::s_interface = {
    setStateCallback,
    setVirtualDesktopCallback,
    setMinimizedGeometryCallback,
    unsetMinimizedGeometryCallback,
    closeCallback,
    requestMoveCallback,
    requestResizeCallback,
    destroyCallback,
    getIconCallback,
    requestEnterVirtualDesktopCallback,
    requestEnterNewVirtualDesktopCallback,
    requestLeaveVirtualDesktopCallback
};
#endif

PlasmaWindowInterface::Private::Private(PlasmaWindowManagementInterface *wm, PlasmaWindowInterface *q)
    : wm(wm)
    , q(q)
{
}

PlasmaWindowInterface::Private::~Private()
{
    // need to copy, as destroy goes through the destroy listener and modifies the list as we iterate
    const auto c = resources;
    for (const auto &r : c) {
        auto client = wl_resource_get_client(r);
        org_kde_plasma_window_send_unmapped(r);
        wl_resource_destroy(r);
        wl_client_flush(client);
    }
}

void PlasmaWindowInterface::Private::destroyCallback(wl_client *, wl_resource *r)
{
    Private *p = cast(r);
    p->resources.removeAll(r);
    wl_resource_destroy(r);
    if (p->unmapped && p->resources.isEmpty()) {
        p->q->deleteLater();
    }
}

void PlasmaWindowInterface::Private::unbind(wl_resource *resource)
{
    Private *p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    p->resources.removeAll(resource);
    if (p->unmapped && p->resources.isEmpty()) {
        p->q->deleteLater();
    }
}

void PlasmaWindowInterface::Private::createResource(wl_resource *parent, uint32_t id)
{
    ClientConnection *c = wm->display()->getConnection(wl_resource_get_client(parent));
    wl_resource *resource = c->createResource(&org_kde_plasma_window_interface, wl_resource_get_version(parent), id);
    if (!resource) {
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    resources << resource;

    org_kde_plasma_window_send_virtual_desktop_changed(resource, m_virtualDesktop);
    for (const auto &desk : plasmaVirtualDesktops) {
        org_kde_plasma_window_send_virtual_desktop_entered(resource, desk.toUtf8().constData());
    }
    if (!m_appId.isEmpty()) {
        org_kde_plasma_window_send_app_id_changed(resource, m_appId.toUtf8().constData());
    }
    if (m_pid != 0) {
        org_kde_plasma_window_send_pid_changed(resource, m_pid);
    }
    if (!m_title.isEmpty()) {
        org_kde_plasma_window_send_title_changed(resource, m_title.toUtf8().constData());
    }
    if (!m_appObjectPath.isEmpty() || !m_appServiceName.isEmpty()) {
        org_kde_plasma_window_send_application_menu(resource, m_appServiceName.toUtf8().constData(), m_appObjectPath.toUtf8().constData());
    }
    org_kde_plasma_window_send_state_changed(resource, m_state);
    if (!m_themedIconName.isEmpty()) {
        org_kde_plasma_window_send_themed_icon_name_changed(resource, m_themedIconName.toUtf8().constData());
    } else {
        if (wl_resource_get_version(resource) >= ORG_KDE_PLASMA_WINDOW_ICON_CHANGED_SINCE_VERSION) {
            org_kde_plasma_window_send_icon_changed(resource);
        }
    }

    org_kde_plasma_window_send_parent_window(resource, resourceForParent(parentWindow, resource));

    if (unmapped) {
        org_kde_plasma_window_send_unmapped(resource);
    }

    if (geometry.isValid() && wl_resource_get_version(resource) >= ORG_KDE_PLASMA_WINDOW_GEOMETRY_SINCE_VERSION) {
        org_kde_plasma_window_send_geometry(resource, geometry.x(), geometry.y(), geometry.width(), geometry.height());
    }

    if (wl_resource_get_version(resource) >= ORG_KDE_PLASMA_WINDOW_INITIAL_STATE_SINCE_VERSION) {
        org_kde_plasma_window_send_initial_state(resource);
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
        org_kde_plasma_window_send_app_id_changed(*it, utf8.constData());
    }
}

void PlasmaWindowInterface::Private::setPid(quint32 pid)
{
    if (m_pid == pid) {
        return;
    }
    m_pid = pid;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_pid_changed(*it, pid);
    }
}

void PlasmaWindowInterface::Private::setThemedIconName(const QString &iconName)
{
    if (m_themedIconName == iconName) {
        return;
    }
    m_themedIconName = iconName;
    const QByteArray utf8 = m_themedIconName.toUtf8();
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_themed_icon_name_changed(*it, utf8.constData());
    }
}

void PlasmaWindowInterface::Private::setIcon(const QIcon &icon)
{
    m_icon = icon;
    setThemedIconName(m_icon.name());
    if (m_icon.name().isEmpty()) {
        for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
            if (wl_resource_get_version(*it) >= ORG_KDE_PLASMA_WINDOW_ICON_CHANGED_SINCE_VERSION) {
                org_kde_plasma_window_send_icon_changed(*it);
            }
        }
    }
}

void PlasmaWindowInterface::Private::getIconCallback(wl_client *client, wl_resource *resource, int32_t fd)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    QtConcurrent::run(
        [fd] (const QIcon &icon) {
            QFile file;
            file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle);
            QDataStream ds(&file);
            ds << icon;
            file.close();
        }, p->m_icon
    );
}

void PlasmaWindowInterface::Private::requestEnterVirtualDesktopCallback(wl_client *client, wl_resource *resource, const char *id)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    emit p->q->enterPlasmaVirtualDesktopRequested(QString::fromUtf8(id));
}

void PlasmaWindowInterface::Private::requestEnterNewVirtualDesktopCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    emit p->q->enterNewPlasmaVirtualDesktopRequested();
}

void PlasmaWindowInterface::Private::requestLeaveVirtualDesktopCallback(wl_client *client, wl_resource *resource, const char *id)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    emit p->q->leavePlasmaVirtualDesktopRequested(QString::fromUtf8(id));
}

void PlasmaWindowInterface::Private::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    const QByteArray utf8 = m_title.toUtf8();
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_title_changed(*it, utf8.constData());
    }
}

void PlasmaWindowInterface::Private::setVirtualDesktop(quint32 desktop)
{
    if (m_virtualDesktop == desktop) {
        return;
    }
    m_virtualDesktop = desktop;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_virtual_desktop_changed(*it, m_virtualDesktop);
    }
}

void PlasmaWindowInterface::Private::unmap()
{
    unmapped = true;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_unmapped(*it);
    }
    if (resources.isEmpty()) {
        q->deleteLater();
    }
}

void PlasmaWindowInterface::Private::setState(org_kde_plasma_window_management_state flag, bool set)
{
    quint32 newState = m_state;
    if (set) {
        newState |= flag;
    } else {
        newState &= ~flag;
    }
    if (newState == m_state) {
        return;
    }
    m_state = newState;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_state_changed(*it, m_state);
    }
}

wl_resource *PlasmaWindowInterface::Private::resourceForParent(PlasmaWindowInterface *parent, wl_resource *child) const
{
    if (!parent) {
        return nullptr;
    }
    auto it = std::find_if(parent->d->resources.begin(), parent->d->resources.end(),
        [child] (wl_resource *parentResource) {
            return wl_resource_get_client(child) == wl_resource_get_client(parentResource);
        }
    );
    if (it != parent->d->resources.end()) {
        return *it;
    }
    return nullptr;
}

void PlasmaWindowInterface::Private::setParentWindow(PlasmaWindowInterface *window)
{
    if (parentWindow == window) {
        return;
    }
    QObject::disconnect(parentWindowDestroyConnection);
    parentWindowDestroyConnection = QMetaObject::Connection();
    parentWindow = window;
    if (parentWindow) {
        parentWindowDestroyConnection = QObject::connect(window, &QObject::destroyed, q,
            [this] {
                parentWindow = nullptr;
                parentWindowDestroyConnection = QMetaObject::Connection();
                for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
                    org_kde_plasma_window_send_parent_window(*it, nullptr);
                }
            }
        );
    }
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        org_kde_plasma_window_send_parent_window(*it, resourceForParent(window, *it));
    }
}

void PlasmaWindowInterface::Private::setGeometry(const QRect &geo)
{
    if (geometry == geo) {
        return;
    }
    geometry = geo;
    if (!geometry.isValid()) {
        return;
    }
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        auto resource = *it;
        if (wl_resource_get_version(resource) < ORG_KDE_PLASMA_WINDOW_GEOMETRY_SINCE_VERSION) {
            continue;
        }
        org_kde_plasma_window_send_geometry(resource, geometry.x(), geometry.y(), geometry.width(), geometry.height());
    }
}

void PlasmaWindowInterface::Private::setApplicationMenuPaths(const QString &service, const QString &object)
{
    if (m_appServiceName == service && m_appObjectPath == object) {
        return;
    }
    m_appServiceName = service;
    m_appObjectPath = object;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        auto resource = *it;
        if (wl_resource_get_version(resource) < ORG_KDE_PLASMA_WINDOW_APPLICATION_MENU_SINCE_VERSION) {
            continue;
        }
        org_kde_plasma_window_send_application_menu(resource, qUtf8Printable(service), qUtf8Printable(object));
    }
}

void PlasmaWindowInterface::Private::closeCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    emit p->q->closeRequested();
}

void PlasmaWindowInterface::Private::requestMoveCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    emit p->q->moveRequested();
}

void PlasmaWindowInterface::Private::requestResizeCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    emit p->q->resizeRequested();
}

void PlasmaWindowInterface::Private::setVirtualDesktopCallback(wl_client *client, wl_resource *resource, uint32_t number)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    emit p->q->virtualDesktopRequested(number);
}

void PlasmaWindowInterface::Private::setStateCallback(wl_client *client, wl_resource *resource, uint32_t flags, uint32_t state)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE) {
        emit p->q->activeRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED) {
        emit p->q->minimizedRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED) {
        emit p->q->maximizedRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN) {
        emit p->q->fullscreenRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE) {
        emit p->q->keepAboveRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW) {
        emit p->q->keepBelowRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION) {
        emit p->q->demandsAttentionRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE) {
        emit p->q->closeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE) {
        emit p->q->minimizeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE) {
        emit p->q->maximizeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE) {
        emit p->q->fullscreenableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR) {
        emit p->q->skipTaskbarRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER) {
        emit p->q->skipSwitcherRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE) {
        emit p->q->shadeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED) {
        emit p->q->shadedRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE) {
        emit p->q->movableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE) {
        emit p->q->resizableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE) {
        emit p->q->virtualDesktopChangeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE);
    }
}

void PlasmaWindowInterface::Private::setMinimizedGeometryCallback(wl_client *client, wl_resource *resource, wl_resource *panel, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    SurfaceInterface *panelSurface = SurfaceInterface::get(panel);

    if (!panelSurface) {
        return;
    }

    if (p->minimizedGeometries.value(panelSurface) == QRect(x, y, width, height)) {
        return;
    }

    p->minimizedGeometries[panelSurface] = QRect(x, y, width, height);
    emit p->q->minimizedGeometriesChanged();
    connect(panelSurface, &QObject::destroyed, p->q, [p, panelSurface] () {
        if (p->minimizedGeometries.remove(panelSurface)) {
            emit p->q->minimizedGeometriesChanged();
        }
    });
}

void PlasmaWindowInterface::Private::unsetMinimizedGeometryCallback(wl_client *client, wl_resource *resource, wl_resource *panel)
{
    Q_UNUSED(client)
    Private *p = cast(resource);
    SurfaceInterface *panelSurface = SurfaceInterface::get(panel);

    if (!panelSurface) {
        return;
    }
    if (!p->minimizedGeometries.contains(panelSurface)) {
        return;
    }
    p->minimizedGeometries.remove(panelSurface);
    emit p->q->minimizedGeometriesChanged();
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

void PlasmaWindowInterface::setPid(quint32 pid)
{
    d->setPid(pid);
}

void PlasmaWindowInterface::setTitle(const QString &title)
{
    d->setTitle(title);
}

void PlasmaWindowInterface::setVirtualDesktop(quint32 desktop)
{
    d->setVirtualDesktop(desktop);
}

void PlasmaWindowInterface::unmap()
{
    d->wm->unmapWindow(this);
}

QHash<SurfaceInterface*, QRect>  PlasmaWindowInterface::minimizedGeometries() const
{
    return d->minimizedGeometries;
}

void PlasmaWindowInterface::setActive(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE, set);
}

void PlasmaWindowInterface::setFullscreen(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN, set);
}

void PlasmaWindowInterface::setKeepAbove(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE, set);
}

void PlasmaWindowInterface::setKeepBelow(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW, set);
}

void PlasmaWindowInterface::setMaximized(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED, set);
}

void PlasmaWindowInterface::setMinimized(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED, set);
}

void PlasmaWindowInterface::setOnAllDesktops(bool set)
{
    //the deprecated vd management
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ON_ALL_DESKTOPS, set);

    if (!d->wm->plasmaVirtualDesktopManagementInterface()) {
        return;
    }

    //the current vd management
    if (set) {
        if (d->plasmaVirtualDesktops.isEmpty()) {
            return;
        }
        //leaving everything means on all desktops
        for (auto desk : plasmaVirtualDesktops()) {
            for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
                org_kde_plasma_window_send_virtual_desktop_left(*it, desk.toUtf8().constData());
            }
        }
        d->plasmaVirtualDesktops.clear();
    } else {
        if (!d->plasmaVirtualDesktops.isEmpty()) {
            return;
        }
        //enters the desktops which are active (usually only one  but not a given)
        for (auto desk : d->wm->plasmaVirtualDesktopManagementInterface()->desktops()) {
            if (desk->isActive() && !d->plasmaVirtualDesktops.contains(desk->id())) {
                d->plasmaVirtualDesktops << desk->id();
                for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
                    org_kde_plasma_window_send_virtual_desktop_entered(*it, desk->id().toUtf8().constData());
                }
            }
        }
    }
}

void PlasmaWindowInterface::setDemandsAttention(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION, set);
}

void PlasmaWindowInterface::setCloseable(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE, set);
}

void PlasmaWindowInterface::setFullscreenable(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE, set);
}

void PlasmaWindowInterface::setMaximizeable(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE, set);
}

void PlasmaWindowInterface::setMinimizeable(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE, set);
}

void PlasmaWindowInterface::setSkipTaskbar(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR, set);
}

void PlasmaWindowInterface::setSkipSwitcher(bool skip)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER, skip);
}

void PlasmaWindowInterface::setThemedIconName(const QString &iconName)
{
    d->setThemedIconName(iconName);
}

void PlasmaWindowInterface::setIcon(const QIcon &icon)
{
    d->setIcon(icon);
}

void PlasmaWindowInterface::addPlasmaVirtualDesktop(const QString &id)
{
    //don't add a desktop we're not sure it exists
    if (!d->wm->plasmaVirtualDesktopManagementInterface() || d->plasmaVirtualDesktops.contains(id)) {
        return;
    }

    PlasmaVirtualDesktopInterface *desktop = d->wm->plasmaVirtualDesktopManagementInterface()->desktop(id);

    if (!desktop) {
        return;
    }

    d->plasmaVirtualDesktops << id;

    //if the desktop dies, remove it from or list
    connect(desktop, &QObject::destroyed,
            this, [this, id](){removePlasmaVirtualDesktop(id);});

    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        org_kde_plasma_window_send_virtual_desktop_entered(*it, id.toUtf8().constData());
    }
}

void PlasmaWindowInterface::removePlasmaVirtualDesktop(const QString &id)
{
    if (!d->plasmaVirtualDesktops.contains(id)) {
        return;
    }

    d->plasmaVirtualDesktops.removeAll(id);
    for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
        org_kde_plasma_window_send_virtual_desktop_left(*it, id.toUtf8().constData());
    }

    //we went on all desktops
    if (d->plasmaVirtualDesktops.isEmpty()) {
        setOnAllDesktops(true);
    }
}

QStringList PlasmaWindowInterface::plasmaVirtualDesktops() const
{
    return d->plasmaVirtualDesktops;
}

void PlasmaWindowInterface::setShadeable(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE, set);
}

void PlasmaWindowInterface::setShaded(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED, set);
}

void PlasmaWindowInterface::setMovable(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE, set);
}

void PlasmaWindowInterface::setResizable(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE, set);
}

void PlasmaWindowInterface::setVirtualDesktopChangeable(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE, set);
}

void PlasmaWindowInterface::setParentWindow(PlasmaWindowInterface *parentWindow)
{
    d->setParentWindow(parentWindow);
}

void PlasmaWindowInterface::setGeometry(const QRect &geometry)
{
    d->setGeometry(geometry);
}

void PlasmaWindowInterface::setApplicationMenuPaths(const QString &serviceName, const QString &objectPath)
{
    d->setApplicationMenuPaths(serviceName, objectPath);
}

quint32 PlasmaWindowInterface::internalId() const
{
    return d->windowId;
}

}
