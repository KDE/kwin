/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xdgdecoration_interface.h"


#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "xdgshell_interface.h"
#include "xdgshell_stable_interface_p.h"

#include "wayland-xdg-decoration-server-protocol.h"

#include <QtDebug>
#include <QPointer>

namespace KWayland
{
namespace Server
{

class XdgDecorationManagerInterface::Private : public Global::Private
{
public:
    Private(XdgDecorationManagerInterface *q, XdgShellInterface *shellInterface, Display *d);

    QHash<XdgShellSurfaceInterface*, XdgDecorationInterface*> m_decorations;
    KWayland::Server::XdgShellStableInterface *m_shellInterface;
private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void getToplevelDecorationCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * toplevel);

    XdgDecorationManagerInterface *q;
    static const struct zxdg_decoration_manager_v1_interface s_interface;
    static const quint32 s_version;
};

const quint32 XdgDecorationManagerInterface::Private::s_version = 1;

XdgDecorationManagerInterface::XdgDecorationManagerInterface(Display *display, XdgShellInterface *shellInterface, QObject *parent):
    Global(new Private(this, shellInterface, display), parent)
{
}

XdgDecorationManagerInterface::~XdgDecorationManagerInterface() {}

#ifndef K_DOXYGEN
const struct zxdg_decoration_manager_v1_interface XdgDecorationManagerInterface::Private::s_interface = {
    destroyCallback,
    getToplevelDecorationCallback
};
#endif

void XdgDecorationManagerInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void XdgDecorationManagerInterface::Private::getToplevelDecorationCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * toplevel)
{
    auto p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    Q_ASSERT(p);

    auto shell = p->m_shellInterface->getSurface(toplevel);
    if (!shell) {
        wl_resource_post_error(resource, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED, "No XDGToplevel found object");
        return;
    }
    if (p->m_decorations.contains(shell)) {
        wl_resource_post_error(resource, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED, "XDGDecoration already exists for this surface");
        return;
    }

    auto xdgDecoration = new XdgDecorationInterface(p->q, shell, resource);
    xdgDecoration->create(p->display->getConnection(client), wl_resource_get_version(resource), id);
    if (!xdgDecoration->resource()) {
        wl_resource_post_no_memory(resource);
        delete xdgDecoration;
        return;
    }
    p->m_decorations[shell] = xdgDecoration;
    QObject::connect(xdgDecoration, &QObject::destroyed, p->q, [=]() {
        p->m_decorations.remove(shell);
    });
    emit p->q->xdgDecorationInterfaceCreated(xdgDecoration);
}

XdgDecorationManagerInterface::Private::Private(XdgDecorationManagerInterface *q, XdgShellInterface *shellInterface, Display *d)
    : Global::Private(d, &zxdg_decoration_manager_v1_interface, s_version)
    , q(q)
{
    m_shellInterface = qobject_cast<XdgShellStableInterface*>(shellInterface);
    Q_ASSERT(m_shellInterface);
}

void XdgDecorationManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&zxdg_decoration_manager_v1_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

void XdgDecorationManagerInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
}

class XdgDecorationInterface::Private : public Resource::Private
{
public:
    Private(XdgDecorationInterface *q, XdgDecorationManagerInterface *c, XdgShellSurfaceInterface *s, wl_resource *parentResource);
    ~Private();
    Mode m_requestedMode = Mode::Undefined;
    XdgShellSurfaceInterface* m_shell;

private:
    static void setModeCallback(wl_client *client, wl_resource *resource, uint32_t mode);
    static void unsetModeCallback(wl_client *client, wl_resource *resource);

    XdgDecorationInterface *q_func() {
        return reinterpret_cast<XdgDecorationInterface *>(q);
    }

    static const struct zxdg_toplevel_decoration_v1_interface s_interface;
};

#ifndef K_DOXYGEN
const struct zxdg_toplevel_decoration_v1_interface XdgDecorationInterface::Private::s_interface = {
    resourceDestroyedCallback,
    setModeCallback,
    unsetModeCallback
};
#endif

void XdgDecorationInterface::Private::setModeCallback(wl_client *client, wl_resource *resource, uint32_t mode_raw)
{
    Q_UNUSED(client);
    auto p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    Q_ASSERT(p);

    Mode mode = Mode::Undefined;
    switch (mode_raw) {
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
        mode = Mode::ClientSide;
        break;
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
        mode = Mode::ServerSide;
        break;
    default:
        break;
    }

    p->m_requestedMode = mode;
    emit p->q_func()->modeRequested(p->m_requestedMode);
}

void XdgDecorationInterface::Private::unsetModeCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client);
    auto p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    Q_ASSERT(p);

    p->m_requestedMode = Mode::Undefined;
    emit p->q_func()->modeRequested(p->m_requestedMode);
}

XdgDecorationInterface::Private::Private(XdgDecorationInterface *q, XdgDecorationManagerInterface *c, XdgShellSurfaceInterface *shell, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &zxdg_toplevel_decoration_v1_interface, &s_interface)
    , m_shell(shell)
{
}

XdgDecorationInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

XdgDecorationInterface::XdgDecorationInterface(XdgDecorationManagerInterface *parent, XdgShellSurfaceInterface *shell, wl_resource *parentResource)
    : Resource(new Private(this, parent, shell, parentResource))
{
}

XdgDecorationInterface::~XdgDecorationInterface() {}

void XdgDecorationInterface::configure(XdgDecorationInterface::Mode mode)
{
    switch(mode) {
    case Mode::ClientSide:
        zxdg_toplevel_decoration_v1_send_configure(resource(), ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
        break;
    case Mode::ServerSide:
        zxdg_toplevel_decoration_v1_send_configure(resource(), ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        break;
    default:
        //configure(Mode::Undefined) should no-op, as it semantically makes no sense
        break;
    }
}

XdgDecorationInterface::Mode XdgDecorationInterface::requestedMode() const
{
    Q_D();
    return d->m_requestedMode;
}

XdgShellSurfaceInterface* XdgDecorationInterface::surface() const
{
    Q_D();
    return d->m_shell;
}

XdgDecorationInterface::Private *XdgDecorationInterface::d_func() const
{
    return reinterpret_cast<XdgDecorationInterface::Private*>(d.data());
}


}
}

