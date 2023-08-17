#pragma once
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferallocator.h"
#include "core/outputlayer.h"
#include "platformsupport/scenes/vulkan/vulkan_backend.h"
#include "platformsupport/scenes/vulkan/vulkan_device.h"
#include "platformsupport/scenes/vulkan/vulkan_texture.h"
#include "wayland_backend.h"
#include "wayland_output.h"

namespace KWin
{
class VulkanSwapchain;
class VulkanSwapchainSlot;

namespace Wayland
{

class WaylandDisplay;
class WaylandVulkanLayer;

class WaylandVulkanBackend : public VulkanBackend
{
public:
    WaylandVulkanBackend(WaylandBackend *backend);
    ~WaylandVulkanBackend() override;

    bool init() override;
    bool testImportBuffer(GraphicsBuffer *buffer) override;
    QHash<uint32_t, QVector<uint64_t>> supportedFormats() const override;
    OutputLayer *primaryLayer(Output *output) override;
    void present(Output *output) override;

private:
    VulkanDevice *findDevice(WaylandDisplay *display) const;

    WaylandBackend *const m_backend;

    VulkanDevice *m_mainDevice = nullptr;
    std::unique_ptr<GraphicsBufferAllocator> m_allocator;
    std::map<Output *, std::unique_ptr<WaylandVulkanLayer>> m_outputs;
};

class WaylandVulkanLayer : public OutputLayer
{
public:
    explicit WaylandVulkanLayer(WaylandOutput *output, VulkanDevice *device, GraphicsBufferAllocator *allocator, WaylandDisplay *display);
    ~WaylandVulkanLayer();

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    quint32 format() const override;

private:
    WaylandOutput *const m_output;
    VulkanDevice *const m_device;
    GraphicsBufferAllocator *const m_allocator;
    WaylandDisplay *const m_display;

    vk::UniqueCommandPool m_cmdPool;
    std::shared_ptr<VulkanSwapchain> m_swapchain;
    std::shared_ptr<VulkanSwapchainSlot> m_currentSlot;
};

}
}
