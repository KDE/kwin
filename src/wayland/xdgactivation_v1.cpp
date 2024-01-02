/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgactivation_v1.h"
#include "clientconnection.h"
#include "display.h"
#include "seat.h"
#include "surface.h"

#include "qwayland-server-xdg-activation-v1.h"

#include <QPointer>

namespace KWin
{
static const int s_version = 1;

class XdgActivationTokenV1Interface : public QtWaylandServer::xdg_activation_token_v1
{
public:
    XdgActivationTokenV1Interface(XdgActivationV1Interface::CreatorFunction creator, ClientConnection *client, uint32_t newId)
        : QtWaylandServer::xdg_activation_token_v1(*client, newId, s_version)
        , m_creator(creator)
        , m_client(client)
    {
    }

protected:
    void xdg_activation_token_v1_set_serial(Resource *resource, uint32_t serial, struct ::wl_resource *seat) override;
    void xdg_activation_token_v1_set_app_id(Resource *resource, const QString &app_id) override;
    void xdg_activation_token_v1_set_surface(QtWaylandServer::xdg_activation_token_v1::Resource *resource, struct ::wl_resource *surface) override;
    void xdg_activation_token_v1_commit(Resource *resource) override;
    void xdg_activation_token_v1_destroy(Resource *resource) override;
    void xdg_activation_token_v1_destroy_resource(Resource *resource) override;

    const XdgActivationV1Interface::CreatorFunction m_creator;
    QPointer<SurfaceInterface> m_surface;
    uint m_serial = 0;
    struct ::wl_resource *m_seat = nullptr;
    QString m_appId;
    bool m_committed = false;
    ClientConnection *const m_client;
};

void XdgActivationTokenV1Interface::xdg_activation_token_v1_set_serial(Resource *resource, uint32_t serial, struct ::wl_resource *seat)
{
    m_serial = serial;
    m_seat = seat;
}

void XdgActivationTokenV1Interface::xdg_activation_token_v1_set_app_id(Resource *resource, const QString &app_id)
{
    m_appId = app_id;
}

void XdgActivationTokenV1Interface::xdg_activation_token_v1_set_surface(Resource *resource, struct ::wl_resource *surface)
{
    m_surface = SurfaceInterface::get(surface);
}

void XdgActivationTokenV1Interface::xdg_activation_token_v1_commit(Resource *resource)
{
    QString token;
    if (m_creator) {
        token = m_creator(m_client, m_surface, m_serial, SeatInterface::get(m_seat), m_appId);
    }

    m_committed = true;
    send_done(resource->handle, token);
}

void XdgActivationTokenV1Interface::xdg_activation_token_v1_destroy(Resource *resource)
{
    if (!m_committed) {
        send_done(resource->handle, {});
    }
    wl_resource_destroy(resource->handle);
}

void XdgActivationTokenV1Interface::xdg_activation_token_v1_destroy_resource(Resource *resource)
{
    delete this;
}

class XdgActivationV1InterfacePrivate : public QtWaylandServer::xdg_activation_v1
{
public:
    XdgActivationV1InterfacePrivate(Display *display, XdgActivationV1Interface *q)
        : QtWaylandServer::xdg_activation_v1(*display, s_version)
        , q(q)
        , m_display(display)
    {
    }

protected:
    void xdg_activation_v1_get_activation_token(Resource *resource, uint32_t id) override;
    void xdg_activation_v1_activate(Resource *resource, const QString &token, struct ::wl_resource *surface) override;
    void xdg_activation_v1_destroy(Resource *resource) override;

public:
    XdgActivationV1Interface::CreatorFunction m_creator;
    XdgActivationV1Interface *const q;
    Display *const m_display;
};

void XdgActivationV1InterfacePrivate::xdg_activation_v1_get_activation_token(Resource *resource, uint32_t id)
{
    new XdgActivationTokenV1Interface(m_creator, m_display->getConnection(resource->client()), id);
}

void XdgActivationV1InterfacePrivate::xdg_activation_v1_activate(Resource *resource, const QString &token, struct ::wl_resource *surface)
{
    Q_EMIT q->activateRequested(SurfaceInterface::get(surface), token);
}

void XdgActivationV1InterfacePrivate::xdg_activation_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

XdgActivationV1Interface::XdgActivationV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new XdgActivationV1InterfacePrivate(display, this))
{
}

XdgActivationV1Interface::~XdgActivationV1Interface()
{
}

void XdgActivationV1Interface::setActivationTokenCreator(const CreatorFunction &creator)
{
    d->m_creator = creator;
}

}

#include "moc_xdgactivation_v1.cpp"
