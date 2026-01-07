/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_layer.h"
#include "color_manager.h"
#include "wayland-client/viewporter.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_output.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/subcompositor.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

#include "wayland-presentation-time-client-protocol.h"
#include "wayland-tearing-control-v1-client-protocol.h"

namespace KWin
{
namespace Wayland
{

WaylandLayer::WaylandLayer(WaylandOutput *output, OutputLayerType type, int zpos)
    : OutputLayer(output, type, zpos, zpos, zpos)
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
    m_viewport = display->viewporter()->createViewport(*m_surface);
}

WaylandLayer::~WaylandLayer()
{
    if (m_tearingControl) {
        wp_tearing_control_v1_destroy(m_tearingControl);
    }
    if (m_colorSurface) {
        wp_color_management_surface_v1_destroy(m_colorSurface);
    }
}

bool WaylandLayer::test() const
{
    if (!isEnabled()) {
        return true;
    }
    if (!m_colorSurface && m_color != ColorDescription::sRGB) {
        return false;
    }
    if (offloadTransform() != OutputTransform::Kind::Normal) {
        // TODO allow this?
        return false;
    }
    if (sourceRect().isEmpty()) {
        return false;
    }
    return true;
}

void WaylandLayer::setBuffer(wl_buffer *buffer, const Region &deviceDamagedRegion)
{
    m_surface->attachBuffer(buffer);
    m_surface->damageBuffer(QRegion(deviceDamagedRegion));
}

void WaylandLayer::commit(PresentationMode presentationMode)
{
    if (!isEnabled()) {
        setBuffer(nullptr, Region{});
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
        return;
    }
    // this is a bit annoying, we need a new Wayland protocol
    // to do this properly with fractional scaling. Until we
    // have that, it may cause blurriness in some cases!
    const Rect logicalTarget = targetRect().scaled(1.0 / m_output->scale()).rounded();
    if (m_viewport) {
        m_viewport->setSource(sourceRect());
        m_viewport->setDestination(logicalTarget.size());
    }
    m_subSurface->setPosition(logicalTarget.topLeft());
    if (m_colorSurface && m_color != m_previousColor) {
        const auto imageDescription = static_cast<WaylandOutput *>(m_output.get())->backend()->display()->colorManager()->createImageDescription(*m_color);
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

KWayland::Client::SubSurface *WaylandLayer::subSurface() const
{
    return m_subSurface.get();
}
}
}
