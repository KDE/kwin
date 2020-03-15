/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "shadow_interface.h"
#include "buffer_interface.h"
#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "surface_interface_p.h"

#include <wayland-server.h>
#include <wayland-shadow-server-protocol.h>

namespace KWayland
{
namespace Server
{

class ShadowManagerInterface::Private : public Global::Private
{
public:
    Private(ShadowManagerInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void createShadow(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);

    static void createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);
    static void unsetCallback(wl_client *client, wl_resource *resource, wl_resource *surface);
    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    ShadowManagerInterface *q;
    static const struct org_kde_kwin_shadow_manager_interface s_interface;
    static const quint32 s_version;
};

const quint32 ShadowManagerInterface::Private::s_version = 2;

#ifndef K_DOXYGEN
const struct org_kde_kwin_shadow_manager_interface ShadowManagerInterface::Private::s_interface = {
    createCallback,
    unsetCallback,
    destroyCallback
};
#endif

ShadowManagerInterface::Private::Private(ShadowManagerInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_shadow_manager_interface, s_version)
    , q(q)
{
}

void ShadowManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_shadow_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void ShadowManagerInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

void ShadowManagerInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void ShadowManagerInterface::Private::createCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    cast(resource)->createShadow(client, resource, id, surface);
}

void ShadowManagerInterface::Private::createShadow(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        return;
    }

    ShadowInterface *shadow = new ShadowInterface(q, resource);
    shadow->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!shadow->resource()) {
        wl_resource_post_no_memory(resource);
        delete shadow;
        return;
    }
    s->d_func()->setShadow(QPointer<ShadowInterface>(shadow));
}

void ShadowManagerInterface::Private::unsetCallback(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        return;
    }
    s->d_func()->setShadow(QPointer<ShadowInterface>());
}

ShadowManagerInterface::ShadowManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

ShadowManagerInterface::~ShadowManagerInterface() = default;

class ShadowInterface::Private : public Resource::Private
{
public:
    Private(ShadowInterface *q, ShadowManagerInterface *c, wl_resource *parentResource);
    ~Private();

    struct State {
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
            Offset = 1 << 8
        };
        BufferInterface *left = nullptr;
        BufferInterface *topLeft = nullptr;
        BufferInterface *top = nullptr;
        BufferInterface *topRight = nullptr;
        BufferInterface *right = nullptr;
        BufferInterface *bottomRight = nullptr;
        BufferInterface *bottom = nullptr;
        BufferInterface *bottomLeft = nullptr;
        QMarginsF offset;
        Flags flags  = Flags::None;
    };
    State current;
    State pending;

private:
    void commit();
    void attach(State::Flags flag, wl_resource *buffer);
    ShadowInterface *q_func() {
        return reinterpret_cast<ShadowInterface *>(q);
    }

    static void commitCallback(wl_client *client, wl_resource *resource);
    static void attachLeftCallback(wl_client *client, wl_resource *resource, wl_resource *buffer);
    static void attachTopLeftCallback(wl_client *client, wl_resource *resource, wl_resource *buffer);
    static void attachTopCallback(wl_client *client, wl_resource *resource, wl_resource *buffer);
    static void attachTopRightCallback(wl_client *client, wl_resource *resource, wl_resource *buffer);
    static void attachRightCallback(wl_client *client, wl_resource *resource, wl_resource *buffer);
    static void attachBottomRightCallback(wl_client *client, wl_resource *resource, wl_resource *buffer);
    static void attachBottomCallback(wl_client *client, wl_resource *resource, wl_resource *buffer);
    static void attachBottomLeftCallback(wl_client *client, wl_resource *resource, wl_resource *buffer);
    static void offsetLeftCallback(wl_client *client, wl_resource *resource, wl_fixed_t offset);
    static void offsetTopCallback(wl_client *client, wl_resource *resource, wl_fixed_t offset);
    static void offsetRightCallback(wl_client *client, wl_resource *resource, wl_fixed_t offset);
    static void offsetBottomCallback(wl_client *client, wl_resource *resource, wl_fixed_t offset);

    static const struct org_kde_kwin_shadow_interface s_interface;
};

#ifndef K_DOXYGEN
const struct org_kde_kwin_shadow_interface ShadowInterface::Private::s_interface = {
    commitCallback,
    attachLeftCallback,
    attachTopLeftCallback,
    attachTopCallback,
    attachTopRightCallback,
    attachRightCallback,
    attachBottomRightCallback,
    attachBottomCallback,
    attachBottomLeftCallback,
    offsetLeftCallback,
    offsetTopCallback,
    offsetRightCallback,
    offsetBottomCallback,
    resourceDestroyedCallback
};
#endif

void ShadowInterface::Private::commitCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast<Private>(resource)->commit();
}

void ShadowInterface::Private::commit()
{
#define BUFFER( __FLAG__, __PART__ ) \
    if (pending.flags & State::Flags::__FLAG__##Buffer) { \
        if (current.__PART__) { \
            current.__PART__->unref(); \
        } \
        if (pending.__PART__) { \
            pending.__PART__->ref(); \
        } \
        current.__PART__ = pending.__PART__; \
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

void ShadowInterface::Private::attach(ShadowInterface::Private::State::Flags flag, wl_resource *buffer)
{
    BufferInterface *b = BufferInterface::get(buffer);
    if (b) {
        QObject::connect(b, &BufferInterface::aboutToBeDestroyed, q,
            [this](BufferInterface *buffer) {
    #define PENDING( __PART__ ) \
                if (pending.__PART__ == buffer) { \
                    pending.__PART__ = nullptr; \
                }
                PENDING(left)
                PENDING(topLeft)
                PENDING(top)
                PENDING(topRight)
                PENDING(right)
                PENDING(bottomRight)
                PENDING(bottom)
                PENDING(bottomLeft)
    #undef PENDING

    #define CURRENT( __PART__ ) \
                if (current.__PART__ == buffer) { \
                    current.__PART__->unref(); \
                    current.__PART__ = nullptr; \
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
        );
    }
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

#define ATTACH( __PART__ ) \
void ShadowInterface::Private::attach##__PART__##Callback(wl_client *client, wl_resource *resource, wl_resource *buffer) \
{ \
    Q_UNUSED(client) \
    Private *p = cast<Private>(resource); \
    p->attach(State::__PART__##Buffer, buffer); \
}

ATTACH(Left)
ATTACH(TopLeft)
ATTACH(Top)
ATTACH(TopRight)
ATTACH(Right)
ATTACH(BottomRight)
ATTACH(Bottom)
ATTACH(BottomLeft)

#undef ATTACH

#define OFFSET( __PART__ ) \
void ShadowInterface::Private::offset##__PART__##Callback(wl_client *client, wl_resource *resource, wl_fixed_t offset) \
{ \
    Q_UNUSED(client) \
    Q_UNUSED(resource) \
    Private *p = cast<Private>(resource); \
    p->pending.flags = State::Flags(p->pending.flags | State::Offset); \
    p->pending.offset.set##__PART__(wl_fixed_to_double(offset)); \
}

OFFSET(Left)
OFFSET(Top)
OFFSET(Right)
OFFSET(Bottom)

#undef OFFSET

ShadowInterface::Private::Private(ShadowInterface *q, ShadowManagerInterface *c, wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &org_kde_kwin_shadow_interface, &s_interface)
{
}

ShadowInterface::Private::~Private()
{
#define CURRENT( __PART__ ) \
    if (current.__PART__) { \
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

ShadowInterface::ShadowInterface(ShadowManagerInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

ShadowInterface::~ShadowInterface() = default;

QMarginsF ShadowInterface::offset() const
{
    Q_D();
    return d->current.offset;
}

#define BUFFER( __PART__ ) \
BufferInterface *ShadowInterface::__PART__() const \
{ \
    Q_D(); \
    return d->current.__PART__; \
}

BUFFER(left)
BUFFER(topLeft)
BUFFER(top)
BUFFER(topRight)
BUFFER(right)
BUFFER(bottomRight)
BUFFER(bottom)
BUFFER(bottomLeft)

ShadowInterface::Private *ShadowInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
