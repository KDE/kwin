/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/cursorshape_v1.h"
#include "wayland/clientconnection.h"
#include "wayland/display.h"
#include "wayland/pointer.h"
#include "wayland/surface.h"
#include "wayland/tablet_v2.h"

#include <QPointer>

#include "qwayland-server-cursor-shape-v1.h"

namespace KWin
{

static constexpr int s_version = 1;

class CursorShapeManagerV1InterfacePrivate : public QtWaylandServer::wp_cursor_shape_manager_v1
{
public:
    CursorShapeManagerV1InterfacePrivate(Display *display);

protected:
    void wp_cursor_shape_manager_v1_destroy(Resource *resource) override;
    void wp_cursor_shape_manager_v1_get_pointer(Resource *resource, uint32_t cursor_shape_device, struct ::wl_resource *pointer) override;
    void wp_cursor_shape_manager_v1_get_tablet_tool_v2(Resource *resource, uint32_t cursor_shape_device, struct ::wl_resource *tablet_tool) override;
};

class CursorShapeDeviceV1Interface : public QtWaylandServer::wp_cursor_shape_device_v1
{
public:
    CursorShapeDeviceV1Interface(PointerInterface *pointer, wl_resource *resource);
    CursorShapeDeviceV1Interface(TabletToolV2Interface *tabletTool, wl_resource *resource);

    QPointer<PointerInterface> pointer;
    QPointer<TabletToolV2Interface> tabletTool;

protected:
    void wp_cursor_shape_device_v1_destroy_resource(Resource *resource) override;
    void wp_cursor_shape_device_v1_destroy(Resource *resource) override;
    void wp_cursor_shape_device_v1_set_shape(Resource *resource, uint32_t serial, uint32_t shape) override;
};

CursorShapeManagerV1InterfacePrivate::CursorShapeManagerV1InterfacePrivate(Display *display)
    : QtWaylandServer::wp_cursor_shape_manager_v1(*display, s_version)
{
}

void CursorShapeManagerV1InterfacePrivate::wp_cursor_shape_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void CursorShapeManagerV1InterfacePrivate::wp_cursor_shape_manager_v1_get_pointer(Resource *resource, uint32_t cursor_shape_device, struct ::wl_resource *pointer)
{
    wl_resource *device = wl_resource_create(resource->client(), &wp_cursor_shape_device_v1_interface, resource->version(), cursor_shape_device);
    if (!device) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    new CursorShapeDeviceV1Interface(PointerInterface::get(pointer), device);
}

void CursorShapeManagerV1InterfacePrivate::wp_cursor_shape_manager_v1_get_tablet_tool_v2(Resource *resource, uint32_t cursor_shape_device, struct ::wl_resource *tablet_tool)
{
    wl_resource *device = wl_resource_create(resource->client(), &wp_cursor_shape_device_v1_interface, resource->version(), cursor_shape_device);
    if (!device) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    new CursorShapeDeviceV1Interface(TabletToolV2Interface::get(tablet_tool), device);
}

CursorShapeManagerV1Interface::CursorShapeManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<CursorShapeManagerV1InterfacePrivate>(display))
{
}

CursorShapeManagerV1Interface::~CursorShapeManagerV1Interface()
{
}

CursorShapeDeviceV1Interface::CursorShapeDeviceV1Interface(PointerInterface *pointer, wl_resource *resource)
    : QtWaylandServer::wp_cursor_shape_device_v1(resource)
    , pointer(pointer)
{
}

CursorShapeDeviceV1Interface::CursorShapeDeviceV1Interface(TabletToolV2Interface *tabletTool, wl_resource *resource)
    : QtWaylandServer::wp_cursor_shape_device_v1(resource)
    , tabletTool(tabletTool)
{
}

void CursorShapeDeviceV1Interface::wp_cursor_shape_device_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void CursorShapeDeviceV1Interface::wp_cursor_shape_device_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

static QByteArray shapeName(uint32_t shape)
{
    switch (shape) {
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_default:
        return QByteArrayLiteral("default");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_context_menu:
        return QByteArrayLiteral("context-menu");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_help:
        return QByteArrayLiteral("help");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_pointer:
        return QByteArrayLiteral("pointer");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_progress:
        return QByteArrayLiteral("progress");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_wait:
        return QByteArrayLiteral("wait");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_cell:
        return QByteArrayLiteral("cell");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_crosshair:
        return QByteArrayLiteral("crosshair");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_text:
        return QByteArrayLiteral("text");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_vertical_text:
        return QByteArrayLiteral("vertical-text");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_alias:
        return QByteArrayLiteral("alias");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_copy:
        return QByteArrayLiteral("copy");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_move:
        return QByteArrayLiteral("move");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_no_drop:
        return QByteArrayLiteral("no-drop");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_not_allowed:
        return QByteArrayLiteral("not-allowed");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_grab:
        return QByteArrayLiteral("grab");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_grabbing:
        return QByteArrayLiteral("grabbing");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_e_resize:
        return QByteArrayLiteral("e-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_n_resize:
        return QByteArrayLiteral("n-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_ne_resize:
        return QByteArrayLiteral("ne-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_nw_resize:
        return QByteArrayLiteral("nw-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_s_resize:
        return QByteArrayLiteral("s-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_se_resize:
        return QByteArrayLiteral("se-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_sw_resize:
        return QByteArrayLiteral("sw-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_w_resize:
        return QByteArrayLiteral("w-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_ew_resize:
        return QByteArrayLiteral("ew-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_ns_resize:
        return QByteArrayLiteral("ns-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_nesw_resize:
        return QByteArrayLiteral("nesw-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_nwse_resize:
        return QByteArrayLiteral("nwse-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_col_resize:
        return QByteArrayLiteral("col-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_row_resize:
        return QByteArrayLiteral("row-resize");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_all_scroll:
        return QByteArrayLiteral("all-scroll");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_zoom_in:
        return QByteArrayLiteral("zoom-in");
    case QtWaylandServer::wp_cursor_shape_device_v1::shape_zoom_out:
        return QByteArrayLiteral("zoom-out");
    default:
        return QByteArrayLiteral("default");
    }
}

void CursorShapeDeviceV1Interface::wp_cursor_shape_device_v1_set_shape(Resource *resource, uint32_t serial, uint32_t shape)
{
    if (shape < shape_default || shape > shape_zoom_out) {
        wl_resource_post_error(resource->handle, error_invalid_shape, "unknown cursor shape");
        return;
    }
    if (pointer) {
        if (!pointer->focusedSurface() || pointer->focusedSurface()->client()->client() != resource->client()) {
            return;
        }
        if (pointer->focusedSerial() == serial) {
            Q_EMIT pointer->cursorChanged(shapeName(shape));
        }
    } else if (tabletTool) {
        if (!tabletTool->currentSurface() || tabletTool->currentSurface()->client()->client() != resource->client()) {
            return;
        }
        if (tabletTool->proximitySerial() == serial) {
            Q_EMIT tabletTool->cursorChanged(shapeName(shape));
        }
    }
}

} // namespace KWin
