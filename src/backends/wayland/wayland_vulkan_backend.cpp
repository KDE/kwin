/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_vulkan_backend.h"
#include "core/renderdevice.h"
#include "vulkan/vulkan_device.h"
#include "wayland-client/linuxdmabuf.h"
#include "wayland_backend.h"
#include "wayland_display.h"
#include "wayland_output.h"

#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>

namespace KWin::Wayland
{

WaylandVulkanBackend::WaylandVulkanBackend(WaylandBackend *b, RenderDevice *device)
    : VulkanBackend(device)
    , m_backend(b)
{
    m_formats = device->vulkanDevice()->storageFormats().intersected(m_backend->display()->linuxDmabuf()->formats());
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
    auto primary = std::make_unique<WaylandLayer>(waylandOutput, this, m_backend->display(), OutputLayerType::Primary, 0);
    primary->subSurface()->placeAbove(waylandOutput->surface());
    layers.push_back(std::move(primary));
    for (int z = 1; z < 5; z++) {
        auto layer = std::make_unique<WaylandLayer>(waylandOutput, this, m_backend->display(), OutputLayerType::GenericLayer, z);
        layer->subSurface()->placeAbove(static_cast<WaylandLayer *>(layers.back().get())->surface());
        layers.push_back(std::move(layer));
    }
    waylandOutput->setOutputLayers(std::move(layers));
}

}
