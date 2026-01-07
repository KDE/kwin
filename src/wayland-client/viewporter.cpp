/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "viewporter.h"

#include "wayland-viewporter-client-protocol.h"

namespace KWin::WaylandClient
{

Viewport::Viewport(wp_viewport *obj)
    : m_obj(obj)
{
}

Viewport::~Viewport()
{
    wp_viewport_destroy(m_obj);
}

void Viewport::setSource(const QSize &size)
{
    setSource(Rect(QPoint(), size));
}

void Viewport::setSource(const RectF &rect)
{
    wp_viewport_set_source(m_obj,
                           wl_fixed_from_double(rect.x()),
                           wl_fixed_from_double(rect.y()),
                           wl_fixed_from_double(rect.width()),
                           wl_fixed_from_double(rect.height()));
}

void Viewport::setDestination(const QSize &size)
{
    wp_viewport_set_destination(m_obj, size.width(), size.height());
}

Viewporter::Viewporter(wl_registry *registry, uint32_t name, uint32_t version)
    : m_obj(reinterpret_cast<wp_viewporter *>(wl_registry_bind(registry, name, &wp_viewporter_interface, version)))
{
}

Viewporter::~Viewporter()
{
    wp_viewporter_destroy(m_obj);
}

std::unique_ptr<Viewport> Viewporter::createViewport(wl_surface *surface)
{
    return std::make_unique<Viewport>(wp_viewporter_get_viewport(m_obj, surface));
}

}
