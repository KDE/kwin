#include "itemrenderer_vulkan.h"
#include "imageitem.h"

namespace KWin
{

ItemRendererVulkan::ItemRendererVulkan(VulkanBackend *backend)
    : m_backend(backend)
{
}

void ItemRendererVulkan::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
}

void ItemRendererVulkan::endFrame()
{
}

void ItemRendererVulkan::renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region)
{
}

void ItemRendererVulkan::renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data)
{
}

std::unique_ptr<ImageItem> ItemRendererVulkan::createImageItem(Scene *scene, Item *parent)
{
    return std::make_unique<ImageItemVulkan>(scene, parent);
}

}
