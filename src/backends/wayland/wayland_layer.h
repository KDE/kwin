/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/graphicsbuffer.h"
#include "core/outputlayer.h"
#include "utils/damagejournal.h"

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

namespace KWin
{

class SwapchainSlot;
class Swapchain;
class RenderBackend;

namespace WaylandClient
{

class Viewport;

}

namespace Wayland
{

class WaylandOutput;
class WaylandDisplay;

class WaylandLayer : public OutputLayer
{
public:
    explicit WaylandLayer(WaylandOutput *output, RenderBackend *backend, WaylandDisplay *display, OutputLayerType type, int zpos);
    ~WaylandLayer() override;

    std::optional<OutputLayerBeginFrameInfo> beginFrame(OutputFrame *frame) override;
    bool endFrame(const Region &damagedDeviceRegion, FileDescriptor &&sync, OutputFrame *frame) override;
    bool earlyScanoutChecks() override;
    bool importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame) override;
    FormatModifierMap supportedDrmFormats() const override;
    void releaseBuffers() override;

    bool test() const;
    void setBuffer(GraphicsBuffer *buffer, const Region &deviceDamagedRegion);
    void commit(PresentationMode presentationMode);

    KWayland::Client::Surface *surface() const;
    KWayland::Client::SubSurface *subSurface() const;

protected:
    RenderBackend *const m_backend;
    WaylandDisplay *const m_display;
    DamageJournal m_damageJournal;
    std::shared_ptr<Swapchain> m_swapchain;
    std::shared_ptr<SwapchainSlot> m_currentSlot;

    std::unique_ptr<KWayland::Client::Surface> m_surface;
    std::unique_ptr<KWayland::Client::SubSurface> m_subSurface;
    wp_presentation_feedback *m_presentationFeedback = nullptr;
    wp_tearing_control_v1 *m_tearingControl = nullptr;
    wp_color_management_surface_v1 *m_colorSurface = nullptr;
    wp_fractional_scale_v1 *m_fractionalScale = nullptr;
    std::unique_ptr<WaylandClient::Viewport> m_viewport;
    std::shared_ptr<ColorDescription> m_previousColor;
    GraphicsBufferRef m_pendingBuffer;
    Region m_pendingDamage;
};

}

}
