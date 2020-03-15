/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "relativepointer_interface_p.h"
#include "display.h"
#include "pointer_interface_p.h"
#include <wayland-relativepointer-unstable-v1-server-protocol.h>
#include <QSizeF>

namespace KWayland
{
namespace Server
{

class RelativePointerManagerUnstableV1Interface::Private : public RelativePointerManagerInterface::Private
{
public:
    Private(RelativePointerManagerUnstableV1Interface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void getRelativePointerCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * pointer);

    RelativePointerManagerUnstableV1Interface *q;

    static const struct zwp_relative_pointer_manager_v1_interface s_interface;
    static const quint32 s_version;
};

const quint32 RelativePointerManagerUnstableV1Interface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct zwp_relative_pointer_manager_v1_interface RelativePointerManagerUnstableV1Interface::Private::s_interface = {
    destroyCallback,
    getRelativePointerCallback
};
#endif

void RelativePointerManagerUnstableV1Interface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void RelativePointerManagerUnstableV1Interface::Private::getRelativePointerCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *pointer)
{
    PointerInterface *p = PointerInterface::get(pointer);
    if (!p) {
        // TODO: raise error?
        return;
    }
    auto m = cast(resource);
    auto *r = new RelativePointerUnstableV1Interface(m->q, resource);
    r->d->create(m->display->getConnection(client), version, id);
    p->d_func()->registerRelativePointer(r);
}

RelativePointerManagerUnstableV1Interface::Private::Private(RelativePointerManagerUnstableV1Interface *q, Display *d)
    : RelativePointerManagerInterface::Private(RelativePointerInterfaceVersion::UnstableV1, q, d, &zwp_relative_pointer_manager_v1_interface, s_version)
    , q(q)
{
}

void RelativePointerManagerUnstableV1Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&zwp_relative_pointer_manager_v1_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void RelativePointerManagerUnstableV1Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

RelativePointerManagerUnstableV1Interface::RelativePointerManagerUnstableV1Interface(Display *display, QObject *parent)
    : RelativePointerManagerInterface(new Private(this, display), parent)
{
}

RelativePointerManagerUnstableV1Interface::~RelativePointerManagerUnstableV1Interface() = default;

class RelativePointerUnstableV1Interface::Private : public RelativePointerInterface::Private
{
public:
    Private(RelativePointerUnstableV1Interface *q, RelativePointerManagerUnstableV1Interface *c, wl_resource *parentResource);
    ~Private();
    void relativeMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds) override;

private:
    RelativePointerUnstableV1Interface *q_func() {
        return reinterpret_cast<RelativePointerUnstableV1Interface *>(q);
    }

    static const struct zwp_relative_pointer_v1_interface s_interface;
};

#ifndef K_DOXYGEN
const struct zwp_relative_pointer_v1_interface RelativePointerUnstableV1Interface::Private::s_interface = {
    resourceDestroyedCallback
};
#endif

RelativePointerUnstableV1Interface::Private::Private(RelativePointerUnstableV1Interface *q, RelativePointerManagerUnstableV1Interface *c, wl_resource *parentResource)
    : RelativePointerInterface::Private(q, c, parentResource, &zwp_relative_pointer_v1_interface, &s_interface)
{
}

RelativePointerUnstableV1Interface::Private::~Private() = default;

void RelativePointerUnstableV1Interface::Private::relativeMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds)
{
    if (!resource) {
        return;
    }
    zwp_relative_pointer_v1_send_relative_motion(resource, (microseconds >> 32), microseconds,
                                                 wl_fixed_from_double(delta.width()),
                                                 wl_fixed_from_double(delta.height()),
                                                 wl_fixed_from_double(deltaNonAccelerated.width()),
                                                 wl_fixed_from_double(deltaNonAccelerated.height()));
}

RelativePointerUnstableV1Interface::RelativePointerUnstableV1Interface(RelativePointerManagerUnstableV1Interface *parent, wl_resource *parentResource)
    : RelativePointerInterface(new Private(this, parent, parentResource))
{
}

RelativePointerUnstableV1Interface::~RelativePointerUnstableV1Interface() = default;

}
}
