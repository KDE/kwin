/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "idlenotify_v1.h"
#include "display.h"

#include "idledetector.h"

#include "qwayland-server-ext-idle-notify-v1.h"

namespace KWin
{

static const quint32 s_version = 1;

class IdleNotifyV1InterfacePrivate : public QtWaylandServer::ext_idle_notifier_v1
{
public:
    IdleNotifyV1InterfacePrivate(Display *display);

protected:
    void ext_idle_notifier_v1_destroy(Resource *resource) override;
    void ext_idle_notifier_v1_get_idle_notification(Resource *resource, uint32_t id, uint32_t timeout, struct ::wl_resource *seat) override;
};

class IdleNotificationV1Interface : public QObject, public QtWaylandServer::ext_idle_notification_v1
{
    Q_OBJECT

public:
    IdleNotificationV1Interface(wl_client *client, int version, uint32_t id, std::chrono::milliseconds timeout);

protected:
    void ext_idle_notification_v1_destroy_resource(Resource *resource) override;
    void ext_idle_notification_v1_destroy(Resource *resource) override;
};

IdleNotifyV1InterfacePrivate::IdleNotifyV1InterfacePrivate(Display *display)
    : QtWaylandServer::ext_idle_notifier_v1(*display, s_version)
{
}

void IdleNotifyV1InterfacePrivate::ext_idle_notifier_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void IdleNotifyV1InterfacePrivate::ext_idle_notifier_v1_get_idle_notification(Resource *resource, uint32_t id, uint32_t timeout, struct ::wl_resource *seat)
{
    new IdleNotificationV1Interface(resource->client(), resource->version(), id, std::chrono::milliseconds(timeout));
}

IdleNotificationV1Interface::IdleNotificationV1Interface(wl_client *client, int version, uint32_t id, std::chrono::milliseconds timeout)
    : QtWaylandServer::ext_idle_notification_v1(client, id, version)
{
    auto detector = new IdleDetector(timeout, this);
    connect(detector, &IdleDetector::idle, this, [this]() {
        send_idled();
    });
    connect(detector, &IdleDetector::resumed, this, [this]() {
        send_resumed();
    });
}

void IdleNotificationV1Interface::ext_idle_notification_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void IdleNotificationV1Interface::ext_idle_notification_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

IdleNotifyV1Interface::IdleNotifyV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new IdleNotifyV1InterfacePrivate(display))
{
}

IdleNotifyV1Interface::~IdleNotifyV1Interface()
{
}

}

#include "idlenotify_v1.moc"

#include "moc_idlenotify_v1.cpp"
