/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_vulkan_backend.h"
#include "core/drmdevice.h"
#include "core/renderdevice.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_swapchain.h"
#include "wayland-client/linuxdmabuf.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_output.h"

#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

namespace KWin::Wayland
{

static const bool bufferAgeEnabled = qEnvironmentVariable("KWIN_USE_BUFFER_AGE") != QStringLiteral("0");

WaylandVulkanLayer::WaylandVulkanLayer(WaylandOutput *output, WaylandVulkanBackend *backend, OutputLayerType type, int zpos)
    : WaylandLayer(output, type, zpos)
    , m_backend(backend)
{
}

WaylandVulkanLayer::~WaylandVulkanLayer()
{
}

std::optional<OutputLayerBeginFrameInfo> WaylandVulkanLayer::doBeginFrame()
{
    const QSize nativeSize = targetRect().size();
    if (!m_swapchain || m_swapchain->size() != nativeSize) {
        const auto formats = supportedDrmFormats();
        const auto suitableFormats = filterAndSortFormats(formats, m_requiredAlphaBits, m_output->colorPowerTradeoff());
        m_swapchain.reset();
        for (const auto &format : suitableFormats) {
            m_swapchain = VulkanSwapchain::create(m_backend->renderDevice()->vulkanDevice(), scanoutDevice()->allocator(),
                                                  nativeSize, format.drmFormat, formats[format.drmFormat]);
            if (m_swapchain) {
                break;
            }
        }
        if (!m_swapchain) {
            return std::nullopt;
        }
    }
    if (m_color != m_previousColor) {
        // need to force a full repaint
        m_damageJournal.clear();
    }
    m_buffer = m_swapchain->acquire();
    if (!m_buffer) {
        return std::nullopt;
    }

    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_buffer->texture(), m_color),
        .repaint = bufferAgeEnabled ? m_damageJournal.accumulate(m_buffer->age(), Region::infinite()) : Region::infinite(),
    };
}

bool WaylandVulkanLayer::doEndFrame(const Region &renderedDeviceRegion, const Region &damagedDeviceRegion, OutputFrame *frame)
{
    return true;
}

bool WaylandVulkanLayer::importScanoutBuffer(GraphicsBuffer *buffer, const std::shared_ptr<OutputFrame> &frame)
{
    // TODO share code with the egl layer through WaylandLayer?
    return false;
}

DrmDevice *WaylandVulkanLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

FormatModifierMap WaylandVulkanLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

void WaylandVulkanLayer::releaseBuffers()
{
    m_buffer.reset();
    m_swapchain.reset();
}

WaylandVulkanBackend::WaylandVulkanBackend(WaylandBackend *b)
    : VulkanBackend(b->renderDevice())
    , m_backend(b)
{
    m_formats = m_device->vulkanDevice()->supportedFormats().intersected(m_backend->display()->linuxDmabuf()->formats());
    initWayland();

    for (const auto &output : m_backend->outputs()) {
        createOutputLayers(output);
    }
    connect(m_backend, &WaylandBackend::outputAdded, this, &WaylandVulkanBackend::createOutputLayers);
}

WaylandVulkanBackend::~WaylandVulkanBackend()
{
}

WaylandBackend *WaylandVulkanBackend::backend() const
{
    return m_backend;
}

DrmDevice *WaylandVulkanBackend::drmDevice() const
{
    return m_device->drmDevice();
}

FormatModifierMap WaylandVulkanBackend::supportedFormats() const
{
    return m_formats;
}

QList<OutputLayer *> WaylandVulkanBackend::compatibleOutputLayers(BackendOutput *output)
{
    return static_cast<WaylandOutput *>(output)->outputLayers();
}

void WaylandVulkanBackend::createOutputLayers(BackendOutput *output)
{
    const auto waylandOutput = static_cast<WaylandOutput *>(output);
    std::vector<std::unique_ptr<OutputLayer>> layers;
    auto primary = std::make_unique<WaylandVulkanLayer>(waylandOutput, this, OutputLayerType::Primary, 0);
    primary->subSurface()->placeAbove(waylandOutput->surface());
    layers.push_back(std::move(primary));
    for (int z = 1; z < 5; z++) {
        auto layer = std::make_unique<WaylandVulkanLayer>(waylandOutput, this, OutputLayerType::GenericLayer, z);
        layer->subSurface()->placeAbove(static_cast<WaylandVulkanLayer *>(layers.back().get())->surface());
        layers.push_back(std::move(layer));
    }
    waylandOutput->setOutputLayers(std::move(layers));
}

}
