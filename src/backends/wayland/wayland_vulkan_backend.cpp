/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_vulkan_backend.h"
#include "core/drmdevice.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "utils/drm_format_helper.h"
#include "vulkan/vulkan_swapchain.h"
#include "wayland_display.h"
#include "wayland_logging.h"

#include "KWayland/Client/surface.h"

#include <xf86drm.h>

namespace KWin
{
namespace Wayland
{

WaylandVulkanBackend::WaylandVulkanBackend(WaylandBackend *backend)
    : m_backend(backend)
{
}

WaylandVulkanBackend::~WaylandVulkanBackend() = default;

VulkanDevice *WaylandVulkanBackend::findDevice() const
{
    const dev_t id = m_backend->drmDevice()->deviceId();
    drmDevicePtr mainDevice = nullptr;
    if (drmGetDeviceFromDevId(id, 0, &mainDevice) != 0) {
        return nullptr;
    }
    const auto compare = [&mainDevice](dev_t id) {
        drmDevicePtr thisDevice = nullptr;
        if (drmGetDeviceFromDevId(id, 0, &thisDevice) != 0) {
            return false;
        }
        const bool ret = drmDevicesEqual(mainDevice, thisDevice);
        drmFreeDevice(&thisDevice);
        return ret;
    };
    for (const auto &device : m_devices) {
        if (device->primaryNode() && compare(*device->primaryNode())) {
            return device.get();
        } else if (device->renderNode() && compare(*device->renderNode())) {
            return device.get();
        }
    }
    free(mainDevice);
    return nullptr;
}

bool WaylandVulkanBackend::init()
{
    if (!m_backend->display()->linuxDmabuf()) {
        qCWarning(KWIN_WAYLAND_BACKEND) << "no linux dmabuf available";
        return false;
    }
    if (!VulkanBackend::init()) {
        return false;
    }
    m_mainDevice = findDevice();
    if (!m_mainDevice) {
        qCWarning(KWIN_WAYLAND_BACKEND) << "couldn't find a main device!";
        return false;
    }

    const auto createOutput = [this](Output *output) {
        m_outputs[output] = std::make_unique<WaylandVulkanLayer>(static_cast<WaylandOutput *>(output), this, m_mainDevice, m_backend->drmDevice()->allocator(), m_backend->display());
    };
    const auto outputs = m_backend->outputs();
    std::for_each(outputs.begin(), outputs.end(), createOutput);
    connect(m_backend, &WaylandBackend::outputAdded, this, createOutput);
    connect(m_backend, &WaylandBackend::outputRemoved, this, [this](Output *output) {
        m_outputs.erase(output);
    });
    return true;
}

bool WaylandVulkanBackend::testImportBuffer(GraphicsBuffer *buffer)
{
    return m_mainDevice->importClientBuffer(buffer) != nullptr;
}

QHash<uint32_t, QVector<uint64_t>> WaylandVulkanBackend::supportedFormats() const
{
    return m_mainDevice->supportedFormats();
}

OutputLayer *WaylandVulkanBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

bool WaylandVulkanBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    const auto wlOutput = static_cast<WaylandOutput *>(output);
    wlOutput->setPendingFrame(frame);
    KWayland::Client::Surface *surface = wlOutput->surface();
    surface->attachBuffer(static_cast<WaylandVulkanLayer *>(primaryLayer(output))->currentBuffer());
    surface->damage(infiniteRegion());
    surface->setScale(std::ceil(wlOutput->scale()));
    surface->commit();
    Q_EMIT wlOutput->outputChange(infiniteRegion());
    return true;
}

WaylandBackend *WaylandVulkanBackend::backend() const
{
    return m_backend;
}

WaylandVulkanLayer::WaylandVulkanLayer(WaylandOutput *output, WaylandVulkanBackend *backend, VulkanDevice *device, GraphicsBufferAllocator *allocator, WaylandDisplay *display)
    : OutputLayer(output)
    , m_backend(backend)
    , m_output(output)
    , m_device(device)
    , m_allocator(allocator)
    , m_display(display)
{
}

WaylandVulkanLayer::~WaylandVulkanLayer() = default;

std::optional<OutputLayerBeginFrameInfo> WaylandVulkanLayer::doBeginFrame()
{
    if (!m_swapchain || m_swapchain->size() != m_output->modeSize()) {
        const auto hostFormats = m_display->linuxDmabuf()->formats();
        const auto formats = m_device->supportedFormats();
        for (auto it = formats.begin(); it != formats.end(); it++) {
            const uint32_t format = it.key();
            if (hostFormats.contains(format)) {
                const auto info = FormatInfo::get(format);
                Q_ASSERT(info);
                // just pick the first 10 or 8 bpc format. More can be supported later
                if (info->bitsPerColor == 8 || info->bitsPerColor == 10) {
                    m_swapchain = VulkanSwapchain::create(m_device, m_allocator, m_output->modeSize(), format, it.value());
                    if (m_swapchain) {
                        break;
                    }
                }
            }
        }
        if (!m_swapchain) {
            return std::nullopt;
        }
    }
    m_currentSlot = m_swapchain->acquire();
    if (!m_currentSlot) {
        return std::nullopt;
    }
    return OutputLayerBeginFrameInfo{
        .renderTarget = RenderTarget(m_currentSlot->commandBuffer(), m_currentSlot->texture()->view(), m_currentSlot->buffer()->size()),
        .repaint = infiniteRegion(),
    };
}

bool WaylandVulkanLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    const bool ret = m_device->submitCommandBufferBlocking(m_currentSlot->commandBuffer());
    m_presentationBuffer = m_backend->backend()->importBuffer(m_currentSlot->buffer());
    Q_ASSERT(m_presentationBuffer);
    m_swapchain->release(m_currentSlot.get());
    return ret;
}

DrmDevice *WaylandVulkanLayer::scanoutDevice() const
{
    return m_backend->drmDevice();
}

QHash<uint32_t, QList<uint64_t>> WaylandVulkanLayer::supportedDrmFormats() const
{
    return m_backend->supportedFormats();
}

wl_buffer *WaylandVulkanLayer::currentBuffer() const
{
    return m_presentationBuffer;
}
}
}
