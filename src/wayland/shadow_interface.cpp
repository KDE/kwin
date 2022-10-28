/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "shadow_interface.h"
#include "clientbuffer.h"
#include "display.h"
#include "surface_interface_p.h"

#include <qwayland-server-shadow.h>

namespace KWaylandServer
{
static const quint32 s_version = 2;

class ShadowManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_shadow_manager
{
public:
    ShadowManagerInterfacePrivate(ShadowManagerInterface *_q, Display *display);

    ShadowManagerInterface *q;
    Display *display;

protected:
    void org_kde_kwin_shadow_manager_create(Resource *resource, uint32_t id, wl_resource *surface) override;
    void org_kde_kwin_shadow_manager_unset(Resource *resource, wl_resource *surface) override;
    void org_kde_kwin_shadow_manager_destroy(Resource *resource) override;
};

ShadowManagerInterfacePrivate::ShadowManagerInterfacePrivate(ShadowManagerInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_shadow_manager(*display, s_version)
    , q(_q)
    , display(display)
{
}

void ShadowManagerInterfacePrivate::org_kde_kwin_shadow_manager_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ShadowManagerInterfacePrivate::org_kde_kwin_shadow_manager_create(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    wl_resource *shadow_resource = wl_resource_create(resource->client(), &org_kde_kwin_shadow_interface, resource->version(), id);
    if (!shadow_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto shadow = new ShadowInterface(q, shadow_resource);

    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->setShadow(QPointer<ShadowInterface>(shadow));
}

void ShadowManagerInterfacePrivate::org_kde_kwin_shadow_manager_unset(Resource *resource, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->setShadow(QPointer<ShadowInterface>());
}

ShadowManagerInterface::ShadowManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ShadowManagerInterfacePrivate(this, display))
{
}

ShadowManagerInterface::~ShadowManagerInterface() = default;

Display *ShadowManagerInterface::display() const
{
    return d->display;
}

class ShadowInterfacePrivate : public QtWaylandServer::org_kde_kwin_shadow
{
public:
    ShadowInterfacePrivate(ShadowInterface *_q, wl_resource *resource);
    ~ShadowInterfacePrivate();

    struct State
    {
        enum Flags {
            None = 0,
            LeftBuffer = 1 << 0,
            TopLeftBuffer = 1 << 1,
            TopBuffer = 1 << 2,
            TopRightBuffer = 1 << 3,
            RightBuffer = 1 << 4,
            BottomRightBuffer = 1 << 5,
            BottomBuffer = 1 << 6,
            BottomLeftBuffer = 1 << 7,
            Offset = 1 << 8,
        };
        QPointer<ClientBuffer> left;
        QPointer<ClientBuffer> topLeft;
        QPointer<ClientBuffer> top;
        QPointer<ClientBuffer> topRight;
        QPointer<ClientBuffer> right;
        QPointer<ClientBuffer> bottomRight;
        QPointer<ClientBuffer> bottom;
        QPointer<ClientBuffer> bottomLeft;
        QMarginsF offset;
        Flags flags = Flags::None;
    };

    void commit();
    void attach(State::Flags flag, wl_resource *buffer);

    ShadowManagerInterface *manager;
    State current;
    State pending;
    ShadowInterface *q;

protected:
    void org_kde_kwin_shadow_destroy_resource(Resource *resource) override;
    void org_kde_kwin_shadow_commit(Resource *resource) override;
    void org_kde_kwin_shadow_attach_left(Resource *resource, wl_resource *buffer) override;
    void org_kde_kwin_shadow_attach_top_left(Resource *resource, wl_resource *buffer) override;
    void org_kde_kwin_shadow_attach_top(Resource *resource, wl_resource *buffer) override;
    void org_kde_kwin_shadow_attach_top_right(Resource *resource, wl_resource *buffer) override;
    void org_kde_kwin_shadow_attach_right(Resource *resource, wl_resource *buffer) override;
    void org_kde_kwin_shadow_attach_bottom_right(Resource *resource, wl_resource *buffer) override;
    void org_kde_kwin_shadow_attach_bottom(Resource *resource, wl_resource *buffer) override;
    void org_kde_kwin_shadow_attach_bottom_left(Resource *resource, wl_resource *buffer) override;
    void org_kde_kwin_shadow_set_left_offset(Resource *resource, wl_fixed_t offset) override;
    void org_kde_kwin_shadow_set_top_offset(Resource *resource, wl_fixed_t offset) override;
    void org_kde_kwin_shadow_set_right_offset(Resource *resource, wl_fixed_t offset) override;
    void org_kde_kwin_shadow_set_bottom_offset(Resource *resource, wl_fixed_t offset) override;
    void org_kde_kwin_shadow_destroy(Resource *resource) override;
};

void ShadowInterfacePrivate::org_kde_kwin_shadow_commit(Resource *resource)
{
#define BUFFER(__FLAG__, __PART__)                        \
    if (pending.flags & State::Flags::__FLAG__##Buffer) { \
        if (current.__PART__) {                           \
            current.__PART__->unref();                    \
        }                                                 \
        if (pending.__PART__) {                           \
            pending.__PART__->ref();                      \
        }                                                 \
        current.__PART__ = pending.__PART__;              \
    }
    BUFFER(Left, left)
    BUFFER(TopLeft, topLeft)
    BUFFER(Top, top)
    BUFFER(TopRight, topRight)
    BUFFER(Right, right)
    BUFFER(BottomRight, bottomRight)
    BUFFER(Bottom, bottom)
    BUFFER(BottomLeft, bottomLeft)
#undef BUFFER

    if (pending.flags & State::Offset) {
        current.offset = pending.offset;
    }
    pending = State();
}

void ShadowInterfacePrivate::attach(ShadowInterfacePrivate::State::Flags flag, wl_resource *buffer)
{
    ClientBuffer *b = manager->display()->clientBufferForResource(buffer);
    switch (flag) {
    case State::LeftBuffer:
        pending.left = b;
        break;
    case State::TopLeftBuffer:
        pending.topLeft = b;
        break;
    case State::TopBuffer:
        pending.top = b;
        break;
    case State::TopRightBuffer:
        pending.topRight = b;
        break;
    case State::RightBuffer:
        pending.right = b;
        break;
    case State::BottomRightBuffer:
        pending.bottomRight = b;
        break;
    case State::BottomBuffer:
        pending.bottom = b;
        break;
    case State::BottomLeftBuffer:
        pending.bottomLeft = b;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    pending.flags = State::Flags(pending.flags | flag);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_destroy_resource(Resource *resource)
{
    delete q;
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_left(Resource *resource, wl_resource *buffer)
{
    attach(State::LeftBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_top_left(Resource *resource, wl_resource *buffer)
{
    attach(State::TopLeftBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_top(Resource *resource, wl_resource *buffer)
{
    attach(State::TopBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_top_right(Resource *resource, wl_resource *buffer)
{
    attach(State::TopRightBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_right(Resource *resource, wl_resource *buffer)
{
    attach(State::RightBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_bottom_right(Resource *resource, wl_resource *buffer)
{
    attach(State::BottomRightBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_bottom(Resource *resource, wl_resource *buffer)
{
    attach(State::BottomBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_bottom_left(Resource *resource, wl_resource *buffer)
{
    attach(State::BottomLeftBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_set_left_offset(Resource *resource, wl_fixed_t offset)
{
    pending.flags = State::Flags(pending.flags | State::Offset);
    pending.offset.setLeft(wl_fixed_to_double(offset));
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_set_top_offset(Resource *resource, wl_fixed_t offset)
{
    pending.flags = State::Flags(pending.flags | State::Offset);
    pending.offset.setTop(wl_fixed_to_double(offset));
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_set_right_offset(Resource *resource, wl_fixed_t offset)
{
    pending.flags = State::Flags(pending.flags | State::Offset);
    pending.offset.setRight(wl_fixed_to_double(offset));
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_set_bottom_offset(Resource *resource, wl_fixed_t offset)
{
    pending.flags = State::Flags(pending.flags | State::Offset);
    pending.offset.setBottom(wl_fixed_to_double(offset));
}

ShadowInterfacePrivate::ShadowInterfacePrivate(ShadowInterface *_q, wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_shadow(resource)
    , q(_q)
{
}

ShadowInterfacePrivate::~ShadowInterfacePrivate()
{
#define CURRENT(__PART__)          \
    if (current.__PART__) {        \
        current.__PART__->unref(); \
    }
    CURRENT(left)
    CURRENT(topLeft)
    CURRENT(top)
    CURRENT(topRight)
    CURRENT(right)
    CURRENT(bottomRight)
    CURRENT(bottom)
    CURRENT(bottomLeft)
#undef CURRENT
}

ShadowInterface::ShadowInterface(ShadowManagerInterface *manager, wl_resource *resource)
    : QObject()
    , d(new ShadowInterfacePrivate(this, resource))
{
    d->manager = manager;
}

ShadowInterface::~ShadowInterface() = default;

QMarginsF ShadowInterface::offset() const
{
    return d->current.offset;
}

#define BUFFER(__PART__)                            \
    ClientBuffer *ShadowInterface::__PART__() const \
    {                                               \
        return d->current.__PART__;                 \
    }

BUFFER(left)
BUFFER(topLeft)
BUFFER(top)
BUFFER(topRight)
BUFFER(right)
BUFFER(bottomRight)
BUFFER(bottom)
BUFFER(bottomLeft)

}
