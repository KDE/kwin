/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_layer.h"
#include "color_manager.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_output.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/subcompositor.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

#include "wayland-presentation-time-client-protocol.h"
#include "wayland-tearing-control-v1-client-protocol.h"
#include "wayland-viewporter-client-protocol.h"

namespace KWin
{
namespace Wayland
{

WaylandLayer::WaylandLayer(WaylandOutput *output)
    : OutputLayer(output)
    , m_surface(output->backend()->display()->compositor()->createSurface())
    , m_subSurface(output->backend()->display()->subCompositor()->createSubSurface(m_surface.get(), output->surface()))
{
    // make presentation atomic with WaylandOutput::present
    m_subSurface->setMode(KWayland::Client::SubSurface::Mode::Synchronized);
    const auto display = output->backend()->display();
    if (auto manager = display->tearingControl()) {
        m_tearingControl = wp_tearing_control_manager_v1_get_tearing_control(manager, *m_surface);
    }
    if (auto manager = display->colorManager()) {
        const bool supportsMinFeatures = manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC)
            && manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES)
            && manager->supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES)
            && manager->supportsTransferFunction(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
        if (supportsMinFeatures) {
            m_colorSurface = wp_color_manager_v1_get_surface(manager->object(), *m_surface);
        }
    }
    m_viewport = wp_viewporter_get_viewport(display->viewporter(), *m_surface);
}

WaylandLayer::~WaylandLayer()
{
    if (m_tearingControl) {
        wp_tearing_control_v1_destroy(m_tearingControl);
    }
    if (m_colorSurface) {
        wp_color_management_surface_v1_destroy(m_colorSurface);
    }
    if (m_viewport) {
        wp_viewport_destroy(m_viewport);
    }
}

bool WaylandLayer::test() const
{
    if (!m_colorSurface && m_color != ColorDescription::sRGB) {
        return false;
    }
    if (offloadTransform() != OutputTransform::Kind::Normal) {
        // TODO allow this?
        return false;
    }
    return true;
}

void WaylandLayer::setBuffer(wl_buffer *buffer, const QRegion &logicalDamagedRegion)
{
    m_surface->attachBuffer(buffer);
    m_surface->damage(logicalDamagedRegion);
}

void WaylandLayer::commit(PresentationMode presentationMode)
{
    // this is a bit annoying, we need a new Wayland protocol
    // to do this properly with fractional scaling. Until we
    // have that, it may cause blurriness in some cases!
    const QRect logicalTarget = scaledRect(targetRect(), 1.0 / m_output->scale()).toRect();
    if (m_viewport) {
        wp_viewport_set_source(m_viewport,
                               wl_fixed_from_double(sourceRect().x()),
                               wl_fixed_from_double(sourceRect().y()),
                               wl_fixed_from_double(sourceRect().width()),
                               wl_fixed_from_double(sourceRect().height()));
        wp_viewport_set_destination(m_viewport, logicalTarget.width(), logicalTarget.height());
    }
    m_subSurface->setPosition(logicalTarget.topLeft());
    if (m_colorSurface && m_color != m_previousColor) {
        const auto imageDescription = static_cast<WaylandOutput *>(m_output)->backend()->display()->colorManager()->createImageDescription(m_color);
        wp_color_management_surface_v1_set_image_description(m_colorSurface, imageDescription, WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
        wp_image_description_v1_destroy(imageDescription);
        m_previousColor = m_color;
    }
    if (m_tearingControl) {
        if (presentationMode == PresentationMode::Async) {
            wp_tearing_control_v1_set_presentation_hint(m_tearingControl, WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC);
        } else {
            wp_tearing_control_v1_set_presentation_hint(m_tearingControl, WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC);
        }
    }
    m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
}

KWayland::Client::Surface *WaylandLayer::surface() const
{
    return m_surface.get();
}

}
}
