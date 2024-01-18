#include "wayland_vulkan_backend.h"
#include "core/gbmgraphicsbufferallocator.h"
#include "platformsupport/scenes/vulkan/vulkan_swapchain.h"
#include "utils/drm_format_helper.h"
#include "wayland_display.h"

#include "KWayland/Client/surface.h"

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
    if (!VulkanBackend::init()) {
        return false;
    }
    m_mainDevice = findDevice(m_backend->display());
    if (!m_mainDevice) {
        qWarning() << "couldn't find a main device!";
        return false;
    }
    m_allocator = std::make_unique<GbmGraphicsBufferAllocator>(m_backend->gbmDevice());

    const auto createOutput = [this](Output *output) {
        m_outputs[output] = std::make_unique<WaylandVulkanLayer>(static_cast<WaylandOutput *>(output), this, m_mainDevice, m_allocator.get(), m_backend->display());
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

OutputLayer *WaylandVulkanBackend::primaryLayer(Output *output)
{
    return m_outputs[output].get();
}

void WaylandVulkanBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    static_cast<WaylandVulkanLayer *>(primaryLayer(output))->present(frame);
}

WaylandBackend *WaylandVulkanBackend::backend() const
{
    return m_backend;
}

WaylandVulkanLayer::WaylandVulkanLayer(WaylandOutput *output, WaylandVulkanBackend *backend, VulkanDevice *device, GraphicsBufferAllocator *allocator, WaylandDisplay *display)
    : m_backend(backend)
    , m_output(output)
    , m_device(device)
    , m_allocator(allocator)
    , m_display(display)
{
    auto [result, cmdPool] = m_device->logicalDevice().createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        m_device->queueFamilyIndex(),
    });
    if (result != vk::Result::eSuccess) {
        qWarning() << "creating a command pool failed:" << vk::to_string(result);
        return;
    }
    m_cmdPool = std::move(cmdPool);
}

WaylandVulkanLayer::~WaylandVulkanLayer() = default;

std::optional<OutputLayerBeginFrameInfo> WaylandVulkanLayer::beginFrame()
{
    if (!m_cmdPool) {
        return std::nullopt;
    }
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

bool WaylandVulkanLayer::endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    return m_device->submitCommandBufferBlocking(m_currentSlot->commandBuffer());
}

quint32 WaylandVulkanLayer::format() const
{
    return m_swapchain ? m_swapchain->format() : DRM_FORMAT_ARGB8888;
}

std::chrono::nanoseconds WaylandVulkanLayer::queryRenderTime() const
{
    // TODO
    return std::chrono::nanoseconds::zero();
}

void WaylandVulkanLayer::present(const std::shared_ptr<OutputFrame> &frame)
{
    m_output->setPendingFrame(frame);
    wl_buffer *buffer = m_backend->backend()->importBuffer(m_currentSlot->buffer());
    Q_ASSERT(buffer);

    KWayland::Client::Surface *surface = m_output->surface();
    surface->attachBuffer(buffer);
    surface->damage(QRect(QPoint(), surface->size()));
    surface->setScale(std::ceil(m_output->scale()));
    surface->commit();
    Q_EMIT m_output->outputChange(infiniteRegion());

    m_swapchain->release(m_currentSlot.get());
}
}
}
