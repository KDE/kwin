/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"

namespace KWayland
{
namespace Client
{
class Surface;
class SubSurface;
}
}

struct wl_buffer;
struct wp_presentation_feedback;
struct wp_tearing_control_v1;
struct wp_color_management_surface_v1;
struct wp_fractional_scale_v1;
struct wp_fractional_scale_v1_listener;
struct wp_viewport;

namespace KWin
{

namespace Wayland
{

class WaylandOutput;

class WaylandLayer : public OutputLayer
{
public:
    explicit WaylandLayer(WaylandOutput *output);
    ~WaylandLayer() override;

    bool test() const;
    void setBuffer(wl_buffer *buffer, const QRegion &logicalDamagedRegion);
    void commit(PresentationMode presentationMode);

    KWayland::Client::Surface *surface() const;

private:
    std::unique_ptr<KWayland::Client::Surface> m_surface;
    std::unique_ptr<KWayland::Client::SubSurface> m_subSurface;
    wp_presentation_feedback *m_presentationFeedback = nullptr;
    wp_tearing_control_v1 *m_tearingControl = nullptr;
    wp_color_management_surface_v1 *m_colorSurface = nullptr;
    wp_fractional_scale_v1 *m_fractionalScale = nullptr;
    wp_viewport *m_viewport = nullptr;
    std::optional<ColorDescription> m_previousColor;
};

}

}
