#pragma once

#include "itemrenderer.h"
#include "platformsupport/scenes/vulkan/vulkan_backend.h"

namespace KWin
{

class ItemRendererVulkan : public ItemRenderer
{
public:
    ItemRendererVulkan(VulkanBackend *backend);

    void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport) override;
    void endFrame(const RenderTarget &renderTarget) override;

    void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region) override;
    void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data) override;

    std::unique_ptr<ImageItem> createImageItem(Scene *scene, Item *parent = nullptr) override;

private:
    VulkanBackend *const m_backend;
};

}
