/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/rect.h"

struct wl_registry;
struct wl_surface;
struct wp_viewport;
struct wp_viewporter;

namespace KWin::WaylandClient
{

class KWIN_EXPORT Viewport
{
public:
    explicit Viewport(wp_viewport *obj);
    ~Viewport();

    void setSource(const QSize &size);
    void setSource(const RectF &rect);
    void setDestination(const QSize &size);

private:
    wp_viewport *const m_obj;
};

class KWIN_EXPORT Viewporter
{
public:
    explicit Viewporter(wl_registry *registry, uint32_t name, uint32_t version);
    ~Viewporter();

    std::unique_ptr<Viewport> createViewport(wl_surface *surface);

private:
    wp_viewporter *const m_obj;
};

}
