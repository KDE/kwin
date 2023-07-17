/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgactivation_v1_interface.h"
#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"
#include "xdgforeign_v2_interface.h"

#include "qwayland-server-xdg-activation-v1.h"

namespace KWaylandServer
{
static const int s_version = 2;

class XdgActivationTokenV1Interface : public QtWaylandServer::xdg_activation_token_v1
{
public:
    XdgActivationTokenV1Interface(XdgActivationV1Interface::CreatorFunction creator, ClientConnection *client, uint32_t newId, uint32_t version)
        : QtWaylandServer::xdg_activation_token_v1(*client, newId, version)
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

class XdgActivationWindowHintV1Interface : public QtWaylandServer::xdg_activation_window_hint
{
public:
    XdgActivationWindowHintV1Interface(wl_client *client, uint32_t id, uint32_t version, XdgForeignV2Interface *xdgForeign, const XdgActivationV1Interface::ChooserFunction &func);

protected:
    void xdg_activation_window_hint_set_activation_token(Resource *resource, const QString &token) override;
    void xdg_activation_window_hint_add_window_candidate_handle(Resource *resource, const QString &window_handle) override;
    void xdg_activation_window_hint_commit(Resource *resource) override;
    void xdg_activation_window_hint_destroy_resource(Resource *resource) override;

private:
    XdgForeignV2Interface *const m_xdgForeign;
    const XdgActivationV1Interface::ChooserFunction m_chooser;
    QString m_activationToken;
    QVector<QString> m_windowHandles;
};

class XdgActivationV1InterfacePrivate : public QtWaylandServer::xdg_activation_v1
{
public:
    XdgActivationV1InterfacePrivate(Display *display, XdgActivationV1Interface *q, XdgForeignV2Interface *xdgForeign)
        : QtWaylandServer::xdg_activation_v1(*display, s_version)
        , q(q)
        , m_display(display)
        , m_xdgForeign(xdgForeign)
    {
    }

protected:
    void xdg_activation_v1_get_activation_token(Resource *resource, uint32_t id) override;
    void xdg_activation_v1_activate(Resource *resource, const QString &token, struct ::wl_resource *surface) override;
    void xdg_activation_v1_destroy(Resource *resource) override;
    void xdg_activation_v1_get_window_hint(Resource *resource, uint32_t id) override;

public:
    XdgActivationV1Interface::CreatorFunction m_creator;
    XdgActivationV1Interface::ChooserFunction m_chooser;
    XdgActivationV1Interface *const q;
    Display *const m_display;
    XdgForeignV2Interface *const m_xdgForeign;
};

void XdgActivationV1InterfacePrivate::xdg_activation_v1_get_activation_token(Resource *resource, uint32_t id)
{
    new XdgActivationTokenV1Interface(m_creator, m_display->getConnection(resource->client()), id, resource->version());
}

void XdgActivationV1InterfacePrivate::xdg_activation_v1_activate(Resource *resource, const QString &token, struct ::wl_resource *surface)
{
    Q_EMIT q->activateRequested(SurfaceInterface::get(surface), token);
}

void XdgActivationV1InterfacePrivate::xdg_activation_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgActivationV1InterfacePrivate::xdg_activation_v1_get_window_hint(Resource *resource, uint32_t id)
{
    new XdgActivationWindowHintV1Interface(resource->client(), id, resource->version(), m_xdgForeign, m_chooser);
}

XdgActivationV1Interface::XdgActivationV1Interface(Display *display, XdgForeignV2Interface *xdgForeign, QObject *parent)
    : QObject(parent)
    , d(new XdgActivationV1InterfacePrivate(display, this, xdgForeign))
{
}

XdgActivationV1Interface::~XdgActivationV1Interface()
{
}

void XdgActivationV1Interface::setActivationTokenCreator(const CreatorFunction &creator)
{
    d->m_creator = creator;
}

void XdgActivationV1Interface::setWindowActivationChooser(const ChooserFunction &chooser)
{
    d->m_chooser = chooser;
}

XdgActivationWindowHintV1Interface::XdgActivationWindowHintV1Interface(wl_client *client, uint32_t id, uint32_t version, XdgForeignV2Interface *xdgForeign, const XdgActivationV1Interface::ChooserFunction &func)
    : QtWaylandServer::xdg_activation_window_hint(client, id, version)
    , m_xdgForeign(xdgForeign)
    , m_chooser(func)
{
}

void XdgActivationWindowHintV1Interface::xdg_activation_window_hint_set_activation_token(Resource *resource, const QString &token)
{
    m_activationToken = token;
}

void XdgActivationWindowHintV1Interface::xdg_activation_window_hint_add_window_candidate_handle(Resource *resource, const QString &window_handle)
{
    m_windowHandles.push_back(window_handle);
}

void XdgActivationWindowHintV1Interface::xdg_activation_window_hint_commit(Resource *resource)
{
    QVector<SurfaceInterface *> surfaces;
    QHash<SurfaceInterface *, QString> handles;
    for (const auto &handle : std::as_const(m_windowHandles)) {
        if (auto surf = m_xdgForeign->exportedSurface(handle)) {
            surfaces.push_back(surf);
            handles[surf] = handle;
        }
    }
    const auto [chosen, fallback] = m_chooser(surfaces, m_activationToken);
    if (chosen) {
        xdg_activation_window_hint_send_chosen_candidate(resource->handle, handles[chosen].toStdString().data());
    } else {
        xdg_activation_window_hint_send_new_candidate_preferred(resource->handle, handles[fallback].toStdString().data());
    }
    wl_resource_destroy(resource->handle);
}

void XdgActivationWindowHintV1Interface::xdg_activation_window_hint_destroy_resource(Resource *resource)
{
    delete this;
}
}

#include "moc_xdgactivation_v1_interface.cpp"
