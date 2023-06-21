#include "wayland_vulkan_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "utils/drm_format_helper.h"
#include "wayland_display.h"

#include <drm_fourcc.h>
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

VulkanDevice *WaylandVulkanBackend::findDevice(WaylandDisplay *display) const
{
    const dev_t id = display->linuxDmabuf()->mainDeviceId();
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
        qWarning() << "no linux dmabuf available";
        return false;
    }
    if (!AbstractVulkanBackend::init()) {
        return false;
    }
    m_mainDevice = findDevice(m_backend->display());
    if (!m_mainDevice) {
        qWarning() << "couldn't find a main device!";
        return false;
    }
    m_allocator = std::make_unique<GbmGraphicsBufferAllocator>(m_backend->gbmDevice());

    const auto createOutput = [this](Output *output) {
        m_outputs[output] = std::make_unique<WaylandVulkanLayer>(static_cast<WaylandOutput *>(output), m_mainDevice, m_allocator.get(), m_backend->display());
    };
    const auto outputs = m_backend->outputs();
    std::for_each(outputs.begin(), outputs.end(), createOutput);
    connect(m_backend, &WaylandBackend::outputAdded, this, createOutput);
    connect(m_backend, &WaylandBackend::outputRemoved, this, [this](Output *output) {
        m_outputs.erase(output);
    });
    qWarning() << "success";
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

WaylandVulkanLayer::WaylandVulkanLayer(WaylandOutput *output, VulkanDevice *device, GraphicsBufferAllocator *allocator, WaylandDisplay *display)
    : m_output(output)
    , m_device(device)
    , m_allocator(allocator)
    , m_display(display)
{
    auto [result, cmdPool] = m_device->logicalDevice().createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlags(),
        m_device->queueFamilyIndex(),
    });
    if (result != vk::Result::eSuccess) {
        qWarning() << "creating a command pool failed:" << vk::to_string(result);
        return;
    }
    m_cmdPool = std::move(cmdPool);
}

std::optional<OutputLayerBeginFrameInfo> WaylandVulkanLayer::beginFrame()
{
    if (!m_cmdPool) {
        return std::nullopt;
    }
    m_currentResources = nullptr;
    for (auto &res : m_resources) {
        if (m_device->logicalDevice().getFenceStatus(res.fence.get()) == vk::Result::eSuccess) {
            m_currentResources = &res;
            break;
        }
    }
    if (!m_currentResources) {
        if (m_resources.empty()) {
            // don't have a buffer format yet
            const auto hostFormats = m_display->linuxDmabuf()->formats();
            const auto formats = m_device->supportedFormats();
            for (auto it = formats.begin(); it != formats.end(); it++) {
                const uint32_t format = it.key();
                if (hostFormats.contains(format)) {
                    const auto info = formatInfo(format);
                    Q_ASSERT(info);
                    // just pick the first 10 or 8 bpc format. More can be supported later
                    if (info->bitsPerColor == 8 || info->bitsPerColor == 10) {
                        auto resources = allocateResources(format, it.value());
                        if (resources.has_value()) {
                            m_resources.push_back(std::move(*resources));
                            break;
                        }
                    }
                }
            }
        } else {
            const auto firstAttributes = m_resources.front().buffer->dmabufAttributes();
            auto resources = allocateResources(firstAttributes->format, {firstAttributes->modifier});
            if (!resources.has_value()) {
                return std::nullopt;
            }
            m_resources.push_back(std::move(*resources));
        }
    }

    return std::nullopt;
}

std::optional<WaylandVulkanLayer::Resources> WaylandVulkanLayer::allocateResources(uint32_t format, const QVector<uint64_t> &modifiers) const
{
    GraphicsBufferRef buffer = m_allocator->allocate(GraphicsBufferOptions{
        .size = m_output->modeSize(),
        .format = format,
        .modifiers = modifiers,
        .software = false,
    });
    if (!buffer) {
        qWarning() << "buffer allocation failed";
        return std::nullopt;
    }
    auto [result, cmdBuffers] = m_device->logicalDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(m_cmdPool.get(), vk::CommandBufferLevel::ePrimary, 1));
    if (result != vk::Result::eSuccess) {
        qWarning() << "allocating a command buffer failed:" << vk::to_string(result);
        return std::nullopt;
    }
    auto [fenceResult, fence] = m_device->logicalDevice().createFenceUnique(vk::FenceCreateInfo{});
    if (fenceResult != vk::Result::eSuccess) {
        qWarning() << "creating a fence failed:" << vk::to_string(result);
        return std::nullopt;
    }
    auto texture = m_device->importDmabuf(*buffer);
    if (!texture) {
        return std::nullopt;
    }
    return Resources{
        .buffer = buffer,
        .texture = std::make_shared<VulkanTexture>(std::move(*texture)),
        .cmd = std::move(cmdBuffers.front()),
        .fence = std::move(fence),
    };
}

bool WaylandVulkanLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return false;
}

quint32 WaylandVulkanLayer::format() const
{
    return m_resources.empty() ? DRM_FORMAT_ARGB8888 : m_resources.front().buffer->dmabufAttributes()->format;
}

}
}
