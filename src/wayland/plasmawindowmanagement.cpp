/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "plasmawindowmanagement.h"
#include "display.h"
#include "output.h"
#include "plasmavirtualdesktop.h"
#include "surface.h"
#include "utils/common.h"
#include "wayland/quirks.h"

#include <QFile>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QPointer>
#include <QRect>
#include <QThreadPool>
#include <QUuid>

#include <qwayland-server-plasma-window-management.h>

namespace KWin
{
static const quint32 s_version = 18;
static const quint32 s_activationVersion = 1;

class PlasmaWindowManagementInterfacePrivate : public QtWaylandServer::org_kde_plasma_window_management
{
public:
    PlasmaWindowManagementInterfacePrivate(PlasmaWindowManagementInterface *_q, Display *display);
    void sendShowingDesktopState();
    void sendShowingDesktopState(wl_resource *resource);
    void sendStackingOrderChanged();
    void sendStackingOrderChanged(wl_resource *resource);
    void sendStackingOrderUuidsChanged();
    void sendStackingOrderUuidsChanged(wl_resource *resource);
    void sendStackingOrderChanged2();
    void sendStackingOrderChanged2(Resource *resource);

    PlasmaWindowManagementInterface::ShowingDesktopState state = PlasmaWindowManagementInterface::ShowingDesktopState::Disabled;
    QList<PlasmaWindowInterface *> windows;
    QPointer<PlasmaVirtualDesktopManagementInterface> plasmaVirtualDesktopManagementInterface = nullptr;
    quint32 windowIdCounter = 0;
    QList<quint32> stackingOrder;
    QList<QString> stackingOrderUuids;
    PlasmaWindowManagementInterface *q;

protected:
    void org_kde_plasma_window_management_bind_resource(Resource *resource) override;
    void org_kde_plasma_window_management_show_desktop(Resource *resource, uint32_t state) override;
    void org_kde_plasma_window_management_get_window(Resource *resource, uint32_t id, uint32_t internal_window_id) override;
    void org_kde_plasma_window_management_get_window_by_uuid(Resource *resource, uint32_t id, const QString &internal_window_uuid) override;
    void org_kde_plasma_window_management_get_stacking_order(Resource *resource, uint32_t id) override;
};

class PlasmaWindowInterfacePrivate : public QtWaylandServer::org_kde_plasma_window
{
public:
    PlasmaWindowInterfacePrivate(PlasmaWindowManagementInterface *wm, PlasmaWindowInterface *q);
    ~PlasmaWindowInterfacePrivate();

    class PlasmaWindowResource : public QtWaylandServer::org_kde_plasma_window::Resource
    {
    public:
        QtWaylandServer::org_kde_plasma_window_management::Resource *wmResource = nullptr;
    };

    void setTitle(const QString &title);
    void setAppId(const QString &appId);
    void setPid(quint32 pid);
    void setThemedIconName(const QString &iconName);
    void setIcon(const QIcon &icon);
    void unmap();
    void setState(org_kde_plasma_window_management_state flag, bool set);
    void setParentWindow(PlasmaWindowInterface *parent);
    void setGeometry(const QRect &geometry);
    void setApplicationMenuPaths(const QString &service, const QString &object);
    void setResourceName(const QString &resourceName);
    void sendInitialState(Resource *resource);
    wl_resource *resourceForParent(PlasmaWindowInterface *parent, Resource *child) const;
    void setClientGeometry(const QRect &geometry);

    quint32 windowId = 0;
    QHash<SurfaceInterface *, QRect> minimizedGeometries;
    PlasmaWindowManagementInterface *wm;

    bool unmapped = false;
    PlasmaWindowInterface *parentWindow = nullptr;
    QMetaObject::Connection parentWindowDestroyConnection;
    QStringList plasmaVirtualDesktops;
    QStringList plasmaActivities;
    QRect geometry;
    PlasmaWindowInterface *q;
    QString m_title;
    QString m_appId;
    quint32 m_pid = 0;
    QString m_themedIconName;
    QString m_appServiceName;
    QString m_appObjectPath;
    QIcon m_icon;
    quint32 m_state = 0;
    QString uuid;
    QString m_resourceName;
    QRect clientGeometry;

protected:
    Resource *org_kde_plasma_window_allocate() override;
    void org_kde_plasma_window_set_state(Resource *resource, uint32_t flags, uint32_t state) override;
    void org_kde_plasma_window_set_virtual_desktop(Resource *resource, uint32_t number) override;
    void org_kde_plasma_window_set_minimized_geometry(Resource *resource, wl_resource *panel, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void org_kde_plasma_window_unset_minimized_geometry(Resource *resource, wl_resource *panel) override;
    void org_kde_plasma_window_close(Resource *resource) override;
    void org_kde_plasma_window_request_move(Resource *resource) override;
    void org_kde_plasma_window_request_resize(Resource *resource) override;
    void org_kde_plasma_window_destroy(Resource *resource) override;
    void org_kde_plasma_window_get_icon(Resource *resource, int32_t fd) override;
    void org_kde_plasma_window_request_enter_virtual_desktop(Resource *resource, const QString &id) override;
    void org_kde_plasma_window_request_enter_new_virtual_desktop(Resource *resource) override;
    void org_kde_plasma_window_request_leave_virtual_desktop(Resource *resource, const QString &id) override;
    void org_kde_plasma_window_request_enter_activity(Resource *resource, const QString &id) override;
    void org_kde_plasma_window_request_leave_activity(Resource *resource, const QString &id) override;
    void org_kde_plasma_window_send_to_output(Resource *resource, struct wl_resource *output) override;
};

PlasmaWindowManagementInterfacePrivate::PlasmaWindowManagementInterfacePrivate(PlasmaWindowManagementInterface *_q, Display *display)
    : QtWaylandServer::org_kde_plasma_window_management(*display, s_version)
    , q(_q)
{
}

void PlasmaWindowManagementInterfacePrivate::sendShowingDesktopState()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendShowingDesktopState(resource->handle);
    }
}

void PlasmaWindowManagementInterfacePrivate::sendShowingDesktopState(wl_resource *r)
{
    uint32_t s = 0;
    switch (state) {
    case PlasmaWindowManagementInterface::ShowingDesktopState::Enabled:
        s = QtWaylandServer::org_kde_plasma_window_management::show_desktop_enabled;
        break;
    case PlasmaWindowManagementInterface::ShowingDesktopState::Disabled:
        s = QtWaylandServer::org_kde_plasma_window_management::show_desktop_disabled;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    send_show_desktop_changed(r, s);
}

void PlasmaWindowManagementInterfacePrivate::sendStackingOrderChanged()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendStackingOrderChanged(resource->handle);
    }
}

void PlasmaWindowManagementInterfacePrivate::sendStackingOrderChanged(wl_resource *r)
{
    if (wl_resource_get_version(r) < ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STACKING_ORDER_CHANGED_SINCE_VERSION
        || wl_resource_get_version(r) >= ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STACKING_ORDER_CHANGED_2_SINCE_VERSION) {
        return;
    }

    send_stacking_order_changed(r, QByteArray::fromRawData(reinterpret_cast<const char *>(stackingOrder.constData()), sizeof(uint32_t) * stackingOrder.size()));
}

void PlasmaWindowManagementInterfacePrivate::sendStackingOrderUuidsChanged()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendStackingOrderUuidsChanged(resource->handle);
    }
}

void PlasmaWindowManagementInterfacePrivate::sendStackingOrderUuidsChanged(wl_resource *r)
{
    if (wl_resource_get_version(r) < ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STACKING_ORDER_UUID_CHANGED_SINCE_VERSION
        || wl_resource_get_version(r) >= ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STACKING_ORDER_CHANGED_2_SINCE_VERSION) {
        return;
    }

    QString uuids;
    for (const auto &uuid : std::as_const(stackingOrderUuids)) {
        uuids += uuid;
        uuids += QLatin1Char(';');
    }
    // Remove the trailing ';', on the receiving side this is interpreted as an empty uuid.
    if (stackingOrderUuids.size() > 0) {
        uuids.remove(uuids.length() - 1, 1);
    }
    send_stacking_order_uuid_changed(r, uuids);
}

void PlasmaWindowManagementInterfacePrivate::sendStackingOrderChanged2()
{
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        sendStackingOrderChanged2(resource);
    }
}

void PlasmaWindowManagementInterfacePrivate::sendStackingOrderChanged2(Resource *resource)
{
    if (resource->version() < ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STACKING_ORDER_CHANGED_2_SINCE_VERSION) {
        return;
    }
    org_kde_plasma_window_management_send_stacking_order_changed_2(resource->handle);
}

void PlasmaWindowManagementInterfacePrivate::org_kde_plasma_window_management_bind_resource(Resource *resource)
{
    for (const auto window : std::as_const(windows)) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_MANAGEMENT_WINDOW_WITH_UUID_SINCE_VERSION) {
            send_window_with_uuid(resource->handle, window->d->windowId, window->d->uuid);
        } else {
            send_window(resource->handle, window->d->windowId);
        }
    }
    sendStackingOrderChanged(resource->handle);
    sendStackingOrderUuidsChanged(resource->handle);
}

void PlasmaWindowManagementInterfacePrivate::org_kde_plasma_window_management_show_desktop(Resource *resource, uint32_t state)
{
    PlasmaWindowManagementInterface::ShowingDesktopState s = PlasmaWindowManagementInterface::ShowingDesktopState::Disabled;
    switch (state) {
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_ENABLED:
        s = PlasmaWindowManagementInterface::ShowingDesktopState::Enabled;
        break;
    case ORG_KDE_PLASMA_WINDOW_MANAGEMENT_SHOW_DESKTOP_DISABLED:
    default:
        s = PlasmaWindowManagementInterface::ShowingDesktopState::Disabled;
        break;
    }
    Q_EMIT q->requestChangeShowingDesktop(s);
}

void PlasmaWindowManagementInterfacePrivate::org_kde_plasma_window_management_get_window(Resource *resource, uint32_t id, uint32_t internal_window_id)
{
    for (const auto window : std::as_const(windows)) {
        if (window->d->windowId == internal_window_id) {
            auto windowResource = window->d->add(resource->client(), id, resource->version());
            static_cast<PlasmaWindowInterfacePrivate::PlasmaWindowResource *>(windowResource)->wmResource = resource;
            window->d->sendInitialState(windowResource);
        }
    }
    // create a temp window just for the resource, bind then immediately delete it, sending an unmap event
    PlasmaWindowInterface window(q, q);
    auto windowResource = window.d->add(resource->client(), id, resource->version());
    window.d->sendInitialState(windowResource);
}

void PlasmaWindowManagementInterfacePrivate::org_kde_plasma_window_management_get_window_by_uuid(Resource *resource,
                                                                                                 uint32_t id,
                                                                                                 const QString &internal_window_uuid)
{
    auto it = std::find_if(windows.constBegin(), windows.constEnd(), [internal_window_uuid](PlasmaWindowInterface *window) {
        return window->d->uuid == internal_window_uuid;
    });
    if (it == windows.constEnd()) {
        qCDebug(KWIN_CORE) << "Could not find window with uuid" << internal_window_uuid;
        // create a temp window just for the resource, bind then immediately delete it, sending an unmap event
        PlasmaWindowInterface window(q, q);
        auto windowResource = window.d->add(resource->client(), id, resource->version());
        window.d->sendInitialState(windowResource);
        return;
    }
    auto windowResource = (*it)->d->add(resource->client(), id, resource->version());
    static_cast<PlasmaWindowInterfacePrivate::PlasmaWindowResource *>(windowResource)->wmResource = resource;
    (*it)->d->sendInitialState(windowResource);
}

void PlasmaWindowManagementInterfacePrivate::org_kde_plasma_window_management_get_stacking_order(Resource *resource, uint32_t id)
{
    wl_resource *stackingOrder = wl_resource_create(resource->client(), &org_kde_plasma_stacking_order_interface, resource->version(), id);
    for (const auto &uuid : std::as_const(stackingOrderUuids)) {
        org_kde_plasma_stacking_order_send_window(stackingOrder, uuid.toUtf8().constData());
    }
    org_kde_plasma_stacking_order_send_done(stackingOrder);
    wl_resource_destroy(stackingOrder);
}

PlasmaWindowManagementInterface::PlasmaWindowManagementInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PlasmaWindowManagementInterfacePrivate(this, display))
{
}

PlasmaWindowManagementInterface::~PlasmaWindowManagementInterface() = default;

void PlasmaWindowManagementInterface::setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState state)
{
    if (d->state == state) {
        return;
    }
    d->state = state;
    d->sendShowingDesktopState();
}

PlasmaWindowInterface *PlasmaWindowManagementInterface::createWindow(QObject *parent, const QUuid &uuid)
{
    PlasmaWindowInterface *window = new PlasmaWindowInterface(this, parent);

    window->d->uuid = uuid.toString();
    window->d->windowId = ++d->windowIdCounter; // NOTE the window id is deprecated

    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_MANAGEMENT_WINDOW_WITH_UUID_SINCE_VERSION) {
            d->send_window_with_uuid(resource->handle, window->d->windowId, window->d->uuid);
        } else {
            d->send_window(resource->handle, window->d->windowId);
        }
    }
    d->windows << window;
    connect(window, &QObject::destroyed, this, [this, window] {
        d->windows.removeAll(window);
    });
    return window;
}

QList<PlasmaWindowInterface *> PlasmaWindowManagementInterface::windows() const
{
    return d->windows;
}

void PlasmaWindowManagementInterface::setStackingOrder(const QList<quint32> &stackingOrder)
{
    if (d->stackingOrder == stackingOrder) {
        return;
    }
    d->stackingOrder = stackingOrder;
    d->sendStackingOrderChanged();
}

void PlasmaWindowManagementInterface::setStackingOrderUuids(const QList<QString> &stackingOrderUuids)
{
    if (d->stackingOrderUuids == stackingOrderUuids) {
        return;
    }
    d->stackingOrderUuids = stackingOrderUuids;
    d->sendStackingOrderUuidsChanged();
    d->sendStackingOrderChanged2();
}

void PlasmaWindowManagementInterface::setPlasmaVirtualDesktopManagementInterface(PlasmaVirtualDesktopManagementInterface *manager)
{
    if (d->plasmaVirtualDesktopManagementInterface == manager) {
        return;
    }
    d->plasmaVirtualDesktopManagementInterface = manager;
}

PlasmaVirtualDesktopManagementInterface *PlasmaWindowManagementInterface::plasmaVirtualDesktopManagementInterface() const
{
    return d->plasmaVirtualDesktopManagementInterface;
}

//////PlasmaWindow
PlasmaWindowInterfacePrivate::PlasmaWindowInterfacePrivate(PlasmaWindowManagementInterface *wm, PlasmaWindowInterface *q)
    : QtWaylandServer::org_kde_plasma_window()
    , wm(wm)
    , q(q)
{
}

PlasmaWindowInterfacePrivate::~PlasmaWindowInterfacePrivate()
{
    unmap();
}

QtWaylandServer::org_kde_plasma_window::Resource *PlasmaWindowInterfacePrivate::org_kde_plasma_window_allocate()
{
    return new PlasmaWindowResource;
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PlasmaWindowInterfacePrivate::sendInitialState(Resource *resource)
{
    for (const auto &desk : std::as_const(plasmaVirtualDesktops)) {
        send_virtual_desktop_entered(resource->handle, desk);
    }
    for (const auto &activity : std::as_const(plasmaActivities)) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_ACTIVITY_ENTERED_SINCE_VERSION) {
            send_activity_entered(resource->handle, activity);
        }
    }
    if (!m_appId.isEmpty()) {
        send_app_id_changed(resource->handle, truncate(m_appId));
    }
    if (m_pid != 0) {
        send_pid_changed(resource->handle, m_pid);
    }
    if (!m_title.isEmpty()) {
        send_title_changed(resource->handle, truncate(m_title));
    }
    if (!m_appObjectPath.isEmpty() || !m_appServiceName.isEmpty()) {
        send_application_menu(resource->handle, m_appServiceName, m_appObjectPath);
    }
    send_state_changed(resource->handle, m_state);
    if (!m_themedIconName.isEmpty()) {
        send_themed_icon_name_changed(resource->handle, m_themedIconName);
    } else if (!m_icon.isNull()) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_ICON_CHANGED_SINCE_VERSION) {
            send_icon_changed(resource->handle);
        }
    }

    send_parent_window(resource->handle, resourceForParent(parentWindow, resource));

    if (geometry.isValid() && resource->version() >= ORG_KDE_PLASMA_WINDOW_GEOMETRY_SINCE_VERSION) {
        send_geometry(resource->handle, geometry.x(), geometry.y(), geometry.width(), geometry.height());
    }

    if (resource->version() >= ORG_KDE_PLASMA_WINDOW_INITIAL_STATE_SINCE_VERSION) {
        send_initial_state(resource->handle);
    }
    if (!m_resourceName.isEmpty()) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_RESOURCE_NAME_CHANGED_SINCE_VERSION) {
            send_resource_name_changed(resource->handle, m_resourceName);
        }
    }

    if (clientGeometry.isValid() && resource->version() >= ORG_KDE_PLASMA_WINDOW_CLIENT_GEOMETRY_SINCE_VERSION) {
        send_client_geometry(resource->handle, clientGeometry.x(), clientGeometry.y(), clientGeometry.width(), clientGeometry.height());
    }
}

void PlasmaWindowInterfacePrivate::setAppId(const QString &appId)
{
    if (m_appId == appId) {
        return;
    }

    m_appId = appId;
    const auto clientResources = resourceMap();

    for (auto resource : clientResources) {
        send_app_id_changed(resource->handle, truncate(m_appId));
    }
}

void PlasmaWindowInterfacePrivate::setPid(quint32 pid)
{
    if (m_pid == pid) {
        return;
    }
    m_pid = pid;
    const auto clientResources = resourceMap();

    for (auto resource : clientResources) {
        send_pid_changed(resource->handle, pid);
    }
}

void PlasmaWindowInterfacePrivate::setThemedIconName(const QString &iconName)
{
    if (m_themedIconName == iconName) {
        return;
    }
    m_themedIconName = iconName;
    const auto clientResources = resourceMap();

    for (auto resource : clientResources) {
        send_themed_icon_name_changed(resource->handle, m_themedIconName);
    }
}

void PlasmaWindowInterfacePrivate::setIcon(const QIcon &icon)
{
    m_icon = icon;
    setThemedIconName(m_icon.name());

    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_ICON_CHANGED_SINCE_VERSION) {
            send_icon_changed(resource->handle);
        }
    }
}

void PlasmaWindowInterfacePrivate::setResourceName(const QString &resourceName)
{
    if (m_resourceName == resourceName) {
        return;
    }
    m_resourceName = resourceName;

    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_RESOURCE_NAME_CHANGED_SINCE_VERSION) {
            send_resource_name_changed(resource->handle, resourceName);
        }
    }
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_get_icon(Resource *resource, int32_t fd)
{
    QThreadPool::globalInstance()->start([fd, icon = m_icon]() {
        QFile file;
        if (!file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
            close(fd);
            qCWarning(KWIN_CORE) << Q_FUNC_INFO << "failed to open file:" << file.errorString();
            return;
        }
        QDataStream ds(&file);
        ds << icon;
        file.close();
    });
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_request_enter_virtual_desktop(Resource *resource, const QString &id)
{
    Q_EMIT q->enterPlasmaVirtualDesktopRequested(id);
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_request_enter_new_virtual_desktop(Resource *resource)
{
    Q_EMIT q->enterNewPlasmaVirtualDesktopRequested();
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_request_leave_virtual_desktop(Resource *resource, const QString &id)
{
    Q_EMIT q->leavePlasmaVirtualDesktopRequested(id);
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_request_enter_activity(Resource *resource, const QString &id)
{
    Q_EMIT q->enterPlasmaActivityRequested(id);
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_request_leave_activity(Resource *resource, const QString &id)
{
    Q_EMIT q->leavePlasmaActivityRequested(id);
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_send_to_output(Resource *resource, struct wl_resource *output)
{
    Q_EMIT q->sendToOutput(OutputInterface::get(output));
}

void PlasmaWindowInterfacePrivate::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    const auto clientResources = resourceMap();

    for (auto resource : clientResources) {
        send_title_changed(resource->handle, truncate(m_title));
    }
}

void PlasmaWindowInterfacePrivate::unmap()
{
    if (unmapped) {
        return;
    }
    unmapped = true;
    const auto clientResources = resourceMap();

    for (auto resource : clientResources) {
        send_unmapped(resource->handle);
    }
}

void PlasmaWindowInterfacePrivate::setState(org_kde_plasma_window_management_state flag, bool set)
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
    const auto clientResources = resourceMap();

    for (auto resource : clientResources) {
        send_state_changed(resource->handle, m_state);
    }
}

wl_resource *PlasmaWindowInterfacePrivate::resourceForParent(PlasmaWindowInterface *parent, Resource *child) const
{
    if (!parent) {
        return nullptr;
    }

    const auto parentResource = parent->d->resourceMap();

    for (auto resource : parentResource) {
        if (static_cast<PlasmaWindowResource *>(child)->wmResource == static_cast<PlasmaWindowResource *>(resource)->wmResource) {
            return resource->handle;
        }
    }
    return nullptr;
}

void PlasmaWindowInterfacePrivate::setParentWindow(PlasmaWindowInterface *window)
{
    if (parentWindow == window) {
        return;
    }
    QObject::disconnect(parentWindowDestroyConnection);
    parentWindowDestroyConnection = QMetaObject::Connection();
    parentWindow = window;
    if (parentWindow) {
        parentWindowDestroyConnection = QObject::connect(window, &QObject::destroyed, q, [this] {
            parentWindow = nullptr;
            parentWindowDestroyConnection = QMetaObject::Connection();
            const auto clientResources = resourceMap();
            for (auto resource : clientResources) {
                send_parent_window(resource->handle, nullptr);
            }
        });
    }
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        send_parent_window(resource->handle, resourceForParent(window, resource));
    }
}

void PlasmaWindowInterfacePrivate::setGeometry(const QRect &geo)
{
    if (geometry == geo) {
        return;
    }
    geometry = geo;
    if (!geometry.isValid()) {
        return;
    }

    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() < ORG_KDE_PLASMA_WINDOW_GEOMETRY_SINCE_VERSION) {
            continue;
        }
        send_geometry(resource->handle, geometry.x(), geometry.y(), geometry.width(), geometry.height());
    }
}

void PlasmaWindowInterfacePrivate::setApplicationMenuPaths(const QString &service, const QString &object)
{
    if (m_appServiceName == service && m_appObjectPath == object) {
        return;
    }
    m_appServiceName = service;
    m_appObjectPath = object;
    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() < ORG_KDE_PLASMA_WINDOW_APPLICATION_MENU_SINCE_VERSION) {
            continue;
        }
        send_application_menu(resource->handle, service, object);
    }
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_close(Resource *resource)
{
    Q_EMIT q->closeRequested();
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_request_move(Resource *resource)
{
    Q_EMIT q->moveRequested();
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_request_resize(Resource *resource)
{
    Q_EMIT q->resizeRequested();
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_set_virtual_desktop(Resource *resource, uint32_t number)
{
    // This method is intentionally left blank.
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_set_state(Resource *resource, uint32_t flags, uint32_t state)
{
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE) {
        Q_EMIT q->activeRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ACTIVE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED) {
        Q_EMIT q->minimizedRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZED);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED) {
        Q_EMIT q->maximizedRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZED);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN) {
        Q_EMIT q->fullscreenRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREEN);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE) {
        Q_EMIT q->keepAboveRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_ABOVE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW) {
        Q_EMIT q->keepBelowRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_KEEP_BELOW);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION) {
        Q_EMIT q->demandsAttentionRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_DEMANDS_ATTENTION);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE) {
        Q_EMIT q->closeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CLOSEABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE) {
        Q_EMIT q->minimizeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MINIMIZABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE) {
        Q_EMIT q->maximizeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MAXIMIZABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE) {
        Q_EMIT q->fullscreenableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_FULLSCREENABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR) {
        Q_EMIT q->skipTaskbarRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPTASKBAR);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER) {
        Q_EMIT q->skipSwitcherRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SKIPSWITCHER);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE) {
        Q_EMIT q->shadeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADEABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED) {
        Q_EMIT q->shadedRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_SHADED);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE) {
        Q_EMIT q->movableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_MOVABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE) {
        Q_EMIT q->resizableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_RESIZABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE) {
        Q_EMIT q->virtualDesktopChangeableRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_VIRTUAL_DESKTOP_CHANGEABLE);
    }
    if (flags & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_NO_BORDER) {
        Q_EMIT q->noBorderRequested(state & ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_NO_BORDER);
    }
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_set_minimized_geometry(Resource *resource,
                                                                                wl_resource *panel,
                                                                                uint32_t x,
                                                                                uint32_t y,
                                                                                uint32_t width,
                                                                                uint32_t height)
{
    SurfaceInterface *panelSurface = SurfaceInterface::get(panel);

    if (!panelSurface) {
        return;
    }

    if (minimizedGeometries.value(panelSurface) == QRect(x, y, width, height)) {
        return;
    }

    minimizedGeometries[panelSurface] = QRect(x, y, width, height);
    Q_EMIT q->minimizedGeometriesChanged();
    QObject::connect(panelSurface, &QObject::destroyed, q, [this, panelSurface]() {
        if (minimizedGeometries.remove(panelSurface)) {
            Q_EMIT q->minimizedGeometriesChanged();
        }
    });
}

void PlasmaWindowInterfacePrivate::org_kde_plasma_window_unset_minimized_geometry(Resource *resource, wl_resource *panel)
{
    SurfaceInterface *panelSurface = SurfaceInterface::get(panel);

    if (!panelSurface) {
        return;
    }
    if (!minimizedGeometries.contains(panelSurface)) {
        return;
    }
    minimizedGeometries.remove(panelSurface);
    Q_EMIT q->minimizedGeometriesChanged();
}

PlasmaWindowInterface::PlasmaWindowInterface(PlasmaWindowManagementInterface *wm, QObject *parent)
    : QObject(parent)
    , d(new PlasmaWindowInterfacePrivate(wm, this))
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

void PlasmaWindowInterface::unmap()
{
    d->unmap();
}

QHash<SurfaceInterface *, QRect> PlasmaWindowInterface::minimizedGeometries() const
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
    // the deprecated vd management
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_ON_ALL_DESKTOPS, set);

    if (!d->wm->plasmaVirtualDesktopManagementInterface()) {
        return;
    }
    const auto clientResources = d->resourceMap();
    // the current vd management
    if (set) {
        if (d->plasmaVirtualDesktops.isEmpty()) {
            return;
        }
        // leaving everything means on all desktops
        for (auto desk : plasmaVirtualDesktops()) {
            for (auto resource : clientResources) {
                d->send_virtual_desktop_left(resource->handle, desk);
            }
        }
        d->plasmaVirtualDesktops.clear();
    } else {
        if (!d->plasmaVirtualDesktops.isEmpty()) {
            return;
        }
        // enters the desktops which are active (usually only one  but not a given)
        const auto desktops = d->wm->plasmaVirtualDesktopManagementInterface()->desktops();
        for (const auto desktop : desktops) {
            if (desktop->isActive() && !d->plasmaVirtualDesktops.contains(desktop->id())) {
                d->plasmaVirtualDesktops << desktop->id();
                for (auto resource : clientResources) {
                    d->send_virtual_desktop_entered(resource->handle, desktop->id());
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

void PlasmaWindowInterface::setIcon(const QIcon &icon)
{
    d->setIcon(icon);
}

void PlasmaWindowInterface::setResourceName(const QString &resourceName)
{
    d->setResourceName(resourceName);
}

void PlasmaWindowInterface::addPlasmaVirtualDesktop(const QString &id)
{
    // don't add a desktop we're not sure it exists
    if (!d->wm->plasmaVirtualDesktopManagementInterface() || d->plasmaVirtualDesktops.contains(id)) {
        return;
    }

    PlasmaVirtualDesktopInterface *desktop = d->wm->plasmaVirtualDesktopManagementInterface()->desktop(id);

    if (!desktop) {
        return;
    }

    d->plasmaVirtualDesktops << id;

    // if the desktop dies, remove it from or list
    connect(desktop, &QObject::destroyed, this, [this, id]() {
        removePlasmaVirtualDesktop(id);
    });

    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_virtual_desktop_entered(resource->handle, id);
    }
}

void PlasmaWindowInterface::removePlasmaVirtualDesktop(const QString &id)
{
    if (!d->plasmaVirtualDesktops.contains(id)) {
        return;
    }

    d->plasmaVirtualDesktops.removeAll(id);
    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_virtual_desktop_left(resource->handle, id);
    }

    // we went on all desktops
    if (d->plasmaVirtualDesktops.isEmpty()) {
        setOnAllDesktops(true);
    }
}

QStringList PlasmaWindowInterface::plasmaVirtualDesktops() const
{
    return d->plasmaVirtualDesktops;
}

void PlasmaWindowInterface::addPlasmaActivity(const QString &id)
{
    if (d->plasmaActivities.contains(id)) {
        return;
    }

    d->plasmaActivities << id;

    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_ACTIVITY_ENTERED_SINCE_VERSION) {
            d->send_activity_entered(resource->handle, id);
        }
    }
}

void PlasmaWindowInterface::removePlasmaActivity(const QString &id)
{
    if (!d->plasmaActivities.removeOne(id)) {
        return;
    }

    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() >= ORG_KDE_PLASMA_WINDOW_ACTIVITY_LEFT_SINCE_VERSION) {
            d->send_activity_left(resource->handle, id);
        }
    }
}

QStringList PlasmaWindowInterface::plasmaActivities() const
{
    return d->plasmaActivities;
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

void PlasmaWindowInterface::setNoBorder(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_NO_BORDER, set);
}

void PlasmaWindowInterface::setCanSetNoBorder(bool set)
{
    d->setState(ORG_KDE_PLASMA_WINDOW_MANAGEMENT_STATE_CAN_SET_NO_BORDER, set);
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

QString PlasmaWindowInterface::uuid() const
{
    return d->uuid;
}

class PlasmaWindowActivationFeedbackInterfacePrivate : public QtWaylandServer::org_kde_plasma_activation_feedback
{
public:
    explicit PlasmaWindowActivationFeedbackInterfacePrivate(Display *display);

protected:
    void org_kde_plasma_activation_feedback_destroy(Resource *resource) override;
};

class PlasmaWindowActivationInterfacePrivate : public QtWaylandServer::org_kde_plasma_activation
{
public:
    explicit PlasmaWindowActivationInterfacePrivate(PlasmaWindowActivationInterface *q)
        : QtWaylandServer::org_kde_plasma_activation()
        , q(q)
    {
    }

    PlasmaWindowActivationInterface *const q;
};

PlasmaWindowActivationFeedbackInterfacePrivate::PlasmaWindowActivationFeedbackInterfacePrivate(Display *display)
    : QtWaylandServer::org_kde_plasma_activation_feedback(*display, s_activationVersion)
{
}

void PlasmaWindowActivationFeedbackInterfacePrivate::org_kde_plasma_activation_feedback_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PlasmaWindowActivationFeedbackInterface::PlasmaWindowActivationFeedbackInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PlasmaWindowActivationFeedbackInterfacePrivate(display))
{
}

PlasmaWindowActivationFeedbackInterface::~PlasmaWindowActivationFeedbackInterface()
{
}

std::unique_ptr<PlasmaWindowActivationInterface> PlasmaWindowActivationFeedbackInterface::createActivation(const QString &appid)
{
    std::unique_ptr<PlasmaWindowActivationInterface> activation(new PlasmaWindowActivationInterface());
    const auto resources = d->resourceMap();
    for (auto resource : resources) {
        auto activationResource = activation->d->add(resource->client(), resource->version());
        d->send_activation(resource->handle, activationResource->handle);
    }
    activation->sendAppId(appid);
    return activation;
}

PlasmaWindowActivationInterface::PlasmaWindowActivationInterface()
    : d(new PlasmaWindowActivationInterfacePrivate(this))
{
}

PlasmaWindowActivationInterface::~PlasmaWindowActivationInterface()
{
    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_finished(resource->handle);
    }
}

void PlasmaWindowActivationInterface::sendAppId(const QString &appid)
{
    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->send_app_id(resource->handle, appid);
    }
}

void PlasmaWindowInterface::setClientGeometry(const QRect &geometry)
{
    d->setClientGeometry(geometry);
}

void PlasmaWindowInterfacePrivate::setClientGeometry(const QRect &geometry)
{
    if (clientGeometry == geometry) {
        return;
    }
    clientGeometry = geometry;
    if (!clientGeometry.isValid()) {
        return;
    }

    const auto clientResources = resourceMap();
    for (auto resource : clientResources) {
        if (resource->version() < ORG_KDE_PLASMA_WINDOW_CLIENT_GEOMETRY_SINCE_VERSION) {
            continue;
        }
        send_client_geometry(resource->handle, clientGeometry.x(), clientGeometry.y(), clientGeometry.width(), clientGeometry.height());
    }
}
}

#include "moc_plasmawindowmanagement.cpp"
