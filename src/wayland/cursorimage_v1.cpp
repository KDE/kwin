/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/cursorimage_v1.h"
#include "utils/resource.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/pointer.h"
#include "wayland/surface.h"
#include "wayland/tablet_v2.h"

#include "wayland/qwayland-server-cursor-image-v1.h"

namespace KWin
{

struct CursorImageV1Commit
{
    std::optional<QPointF> hotspot;
};

struct CursorImageV1State
{
    QPointF hotspot;
};

class CursorImageV1Private : public SurfaceExtension<CursorImageV1Commit>, public QtWaylandServer::wp_cursor_image_v1
{
public:
    CursorImageV1Private(CursorImageV1 *q, SurfaceInterface *surface, wl_resource *resource);

    void apply(CursorImageV1Commit *commit) override;

    CursorImageV1 *q;
    QPointer<SurfaceInterface> surface;
    CursorImageV1State state;

protected:
    void wp_cursor_image_v1_destroy_resource(Resource *resource) override;
    void wp_cursor_image_v1_destroy(Resource *resource) override;
    void wp_cursor_image_v1_set_hotspot(Resource *resource, int32_t x, int32_t y) override;
};

CursorImageV1Private::CursorImageV1Private(CursorImageV1 *q, SurfaceInterface *surface, wl_resource *resource)
    : SurfaceExtension(surface)
    , QtWaylandServer::wp_cursor_image_v1(resource)
    , q(q)
    , surface(surface)
{
}

void CursorImageV1Private::apply(CursorImageV1Commit *commit)
{
    const CursorImageV1State previous = state;
    if (commit->hotspot.has_value()) {
        state.hotspot = commit->hotspot.value();
    }

    if (previous.hotspot != state.hotspot) {
        Q_EMIT q->hotspotChanged();
    }
}

void CursorImageV1Private::wp_cursor_image_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void CursorImageV1Private::wp_cursor_image_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void CursorImageV1Private::wp_cursor_image_v1_set_hotspot(Resource *resource, int32_t x, int32_t y)
{
    pending.hotspot = QPointF(x, y);
}

SurfaceRole *CursorImageV1::role()
{
    static SurfaceRole role(QByteArrayLiteral("cursor_image_v1"));
    return &role;
}

CursorImageV1 *CursorImageV1::get(wl_resource *resource)
{
    if (auto imagePrivate = resource_cast<CursorImageV1Private *>(resource)) {
        return imagePrivate->q;
    }
    return nullptr;
}

class CursorImageManagerV1InterfacePrivate : public QtWaylandServer::wp_cursor_image_manager_v1
{
public:
    explicit CursorImageManagerV1InterfacePrivate(Display *display);

protected:
    void wp_cursor_image_manager_v1_destroy(Resource *resource) override;
    void wp_cursor_image_manager_v1_get_cursor_image(Resource *resource, uint32_t cursor_image, struct ::wl_resource *surface) override;
    void wp_cursor_image_manager_v1_set_pointer_cursor(Resource *resource, struct ::wl_resource *pointer, uint32_t serial, struct ::wl_resource *cursor) override;
    void wp_cursor_image_manager_v1_set_tablet_tool_v2_cursor(Resource *resource, struct ::wl_resource *tablet_tool, uint32_t serial, struct ::wl_resource *cursor) override;
};

CursorImageManagerV1InterfacePrivate::CursorImageManagerV1InterfacePrivate(Display *display)
    : QtWaylandServer::wp_cursor_image_manager_v1(*display, 1)
{
}

void CursorImageManagerV1InterfacePrivate::wp_cursor_image_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void CursorImageManagerV1InterfacePrivate::wp_cursor_image_manager_v1_get_cursor_image(Resource *resource, uint32_t cursor_image, struct ::wl_resource *surface_resource)
{
    auto surface = SurfaceInterface::get(surface_resource);
    if (const SurfaceRole *role = surface->role()) {
        if (role != CursorImageV1::role()) {
            wl_resource_post_error(resource->handle, error_role,
                                   "the wl_surface already has a role assigned %s", role->name().constData());
            return;
        }
    } else {
        surface->setRole(CursorImageV1::role());
    }

    wl_resource *image = wl_resource_create(resource->client(), &wp_cursor_image_v1_interface, resource->version(), cursor_image);
    if (!image) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    new CursorImageV1(surface, image);
}

void CursorImageManagerV1InterfacePrivate::wp_cursor_image_manager_v1_set_pointer_cursor(Resource *resource, struct ::wl_resource *pointer_resource, uint32_t serial, struct ::wl_resource *cursor_resource)
{
    auto pointer = PointerInterface::get(pointer_resource);
    auto cursor = CursorImageV1::get(cursor_resource);

    if (!pointer->focusedSurface() || pointer->focusedSurface()->client()->client() != resource->client()) {
        return;
    }
    if (pointer->focusedSerial() == serial) {
        Q_EMIT pointer->cursorChanged(cursor);
    }
}

void CursorImageManagerV1InterfacePrivate::wp_cursor_image_manager_v1_set_tablet_tool_v2_cursor(Resource *resource, struct ::wl_resource *tablet_tool_resource, uint32_t serial, struct ::wl_resource *cursor_resource)
{
    auto tabletTool = TabletToolV2Interface::get(tablet_tool_resource);
    auto cursor = CursorImageV1::get(cursor_resource);

    if (!tabletTool->currentSurface() || tabletTool->currentSurface()->client()->client() != resource->client()) {
        return;
    }
    if (tabletTool->proximitySerial() == serial) {
        Q_EMIT tabletTool->cursorChanged(cursor);
    }
}

CursorImageManagerV1Interface::CursorImageManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<CursorImageManagerV1InterfacePrivate>(display))
{
}

CursorImageManagerV1Interface::~CursorImageManagerV1Interface()
{
}

} // namespace KWin

#include "moc_cursorimage_v1.cpp"
