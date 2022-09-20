/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "shadow.h"
#include "core/graphicsbuffer.h"
#include "display.h"
#include "surface_p.h"

#include <qwayland-server-shadow.h>

namespace KWin
{
static const quint32 s_version = 2;

class ShadowManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_shadow_manager
{
public:
    ShadowManagerInterfacePrivate(ShadowManagerInterface *_q, Display *display);

    ShadowManagerInterface *q;

protected:
    void org_kde_kwin_shadow_manager_create(Resource *resource, uint32_t id, wl_resource *surface) override;
    void org_kde_kwin_shadow_manager_unset(Resource *resource, wl_resource *surface) override;
    void org_kde_kwin_shadow_manager_destroy(Resource *resource) override;
};

ShadowManagerInterfacePrivate::ShadowManagerInterfacePrivate(ShadowManagerInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_shadow_manager(*display, s_version)
    , q(_q)
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

    auto shadow = new ShadowInterface(q, shadow_resource, s);

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

class ShadowInterfacePrivate : public QtWaylandServer::org_kde_kwin_shadow
{
public:
    ShadowInterfacePrivate(ShadowInterface *_q, wl_resource *resource, SurfaceInterface *surface);
    ~ShadowInterfacePrivate();

    struct Commit
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
        QPointer<GraphicsBuffer> left;
        QPointer<GraphicsBuffer> topLeft;
        QPointer<GraphicsBuffer> top;
        QPointer<GraphicsBuffer> topRight;
        QPointer<GraphicsBuffer> right;
        QPointer<GraphicsBuffer> bottomRight;
        QPointer<GraphicsBuffer> bottom;
        QPointer<GraphicsBuffer> bottomLeft;
        QMarginsF offset;
        Flags flags = Flags::None;
    };

    void commit();
    void attach(Commit::Flags flag, wl_resource *buffer);

    ShadowInterface *q;
    ShadowManagerInterface *manager;
    Commit pending;

    GraphicsBufferRef left;
    GraphicsBufferRef topLeft;
    GraphicsBufferRef top;
    GraphicsBufferRef topRight;
    GraphicsBufferRef right;
    GraphicsBufferRef bottomRight;
    GraphicsBufferRef bottom;
    GraphicsBufferRef bottomLeft;
    QMarginsF offset;

    QPointer<SurfaceInterface> surface;

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
    if (pending.flags & Commit::Flags::LeftBuffer) {
        left = pending.left;
    }
    if (pending.flags & Commit::Flags::TopLeftBuffer) {
        topLeft = pending.topLeft;
    }
    if (pending.flags & Commit::Flags::TopBuffer) {
        top = pending.top;
    }
    if (pending.flags & Commit::Flags::TopRightBuffer) {
        topRight = pending.topRight;
    }
    if (pending.flags & Commit::Flags::RightBuffer) {
        right = pending.right;
    }
    if (pending.flags & Commit::Flags::BottomRightBuffer) {
        bottomRight = pending.bottomRight;
    }
    if (pending.flags & Commit::Flags::BottomBuffer) {
        bottom = pending.bottom;
    }
    if (pending.flags & Commit::Flags::BottomLeftBuffer) {
        bottomLeft = pending.bottomLeft;
    }

    if (pending.flags & Commit::Flags::Offset) {
        offset = pending.offset;
    }
    pending = Commit();
}

void ShadowInterfacePrivate::attach(Commit::Flags flag, wl_resource *buffer)
{
    GraphicsBuffer *b = Display::bufferForResource(buffer);
    switch (flag) {
    case Commit::LeftBuffer:
        pending.left = b;
        break;
    case Commit::TopLeftBuffer:
        pending.topLeft = b;
        break;
    case Commit::TopBuffer:
        pending.top = b;
        break;
    case Commit::TopRightBuffer:
        pending.topRight = b;
        break;
    case Commit::RightBuffer:
        pending.right = b;
        break;
    case Commit::BottomRightBuffer:
        pending.bottomRight = b;
        break;
    case Commit::BottomBuffer:
        pending.bottom = b;
        break;
    case Commit::BottomLeftBuffer:
        pending.bottomLeft = b;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    pending.flags = Commit::Flags(pending.flags | flag);
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
    attach(Commit::LeftBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_top_left(Resource *resource, wl_resource *buffer)
{
    attach(Commit::TopLeftBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_top(Resource *resource, wl_resource *buffer)
{
    attach(Commit::TopBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_top_right(Resource *resource, wl_resource *buffer)
{
    attach(Commit::TopRightBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_right(Resource *resource, wl_resource *buffer)
{
    attach(Commit::RightBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_bottom_right(Resource *resource, wl_resource *buffer)
{
    attach(Commit::BottomRightBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_bottom(Resource *resource, wl_resource *buffer)
{
    attach(Commit::BottomBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_attach_bottom_left(Resource *resource, wl_resource *buffer)
{
    attach(Commit::BottomLeftBuffer, buffer);
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_set_left_offset(Resource *resource, wl_fixed_t offset)
{
    pending.flags = Commit::Flags(pending.flags | Commit::Offset);
    pending.offset.setLeft(wl_fixed_to_double(offset) * (surface ? surface->compositorToClientScale() : 1));
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_set_top_offset(Resource *resource, wl_fixed_t offset)
{
    pending.flags = Commit::Flags(pending.flags | Commit::Offset);
    pending.offset.setTop(wl_fixed_to_double(offset) * (surface ? surface->compositorToClientScale() : 1));
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_set_right_offset(Resource *resource, wl_fixed_t offset)
{
    pending.flags = Commit::Flags(pending.flags | Commit::Offset);
    pending.offset.setRight(wl_fixed_to_double(offset) * (surface ? surface->compositorToClientScale() : 1));
}

void ShadowInterfacePrivate::org_kde_kwin_shadow_set_bottom_offset(Resource *resource, wl_fixed_t offset)
{
    pending.flags = Commit::Flags(pending.flags | Commit::Offset);
    pending.offset.setBottom(wl_fixed_to_double(offset) * (surface ? surface->compositorToClientScale() : 1));
}

ShadowInterfacePrivate::ShadowInterfacePrivate(ShadowInterface *_q, wl_resource *resource, SurfaceInterface *surface)
    : QtWaylandServer::org_kde_kwin_shadow(resource)
    , q(_q)
    , surface(surface)
{
}

ShadowInterfacePrivate::~ShadowInterfacePrivate()
{
}

ShadowInterface::ShadowInterface(ShadowManagerInterface *manager, wl_resource *resource, SurfaceInterface *surface)
    : QObject()
    , d(new ShadowInterfacePrivate(this, resource, surface))
{
    d->manager = manager;
}

ShadowInterface::~ShadowInterface() = default;

QMarginsF ShadowInterface::offset() const
{
    return d->offset;
}

GraphicsBuffer *ShadowInterface::left() const
{
    return d->left.buffer();
}

GraphicsBuffer *ShadowInterface::topLeft() const
{
    return d->topLeft.buffer();
}

GraphicsBuffer *ShadowInterface::top() const
{
    return d->top.buffer();
}

GraphicsBuffer *ShadowInterface::topRight() const
{
    return d->topRight.buffer();
}

GraphicsBuffer *ShadowInterface::right() const
{
    return d->right.buffer();
}

GraphicsBuffer *ShadowInterface::bottomRight() const
{
    return d->bottomRight.buffer();
}

GraphicsBuffer *ShadowInterface::bottom() const
{
    return d->bottom.buffer();
}

GraphicsBuffer *ShadowInterface::bottomLeft() const
{
    return d->bottomLeft.buffer();
}

}

#include "wayland/moc_shadow.cpp"
