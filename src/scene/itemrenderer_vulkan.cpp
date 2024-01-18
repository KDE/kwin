#include "itemrenderer_vulkan.h"
#include "decorationitem.h"
#include "imageitem.h"
#include "platformsupport/scenes/vulkan/vulkan_texture.h"
#include "shadowitem.h"
#include "surfaceitem.h"

namespace KWin
{

ItemRendererVulkan::ItemRendererVulkan(VulkanBackend *backend)
    : m_backend(backend)
{
}

void ItemRendererVulkan::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
    renderTarget.commandBuffer().reset(vk::CommandBufferResetFlags());
    if (renderTarget.commandBuffer().begin(vk::CommandBufferBeginInfo{}) != vk::Result::eSuccess) {
        qWarning() << "command buffer begin failed";
        return;
    }
    std::vector<vk::RenderingAttachmentInfo> colorAttachments{
        vk::RenderingAttachmentInfo(
            renderTarget.imageView(),
            vk::ImageLayout::eAttachmentOptimal,
            vk::ResolveModeFlagBits::eNone,
            {}, // resolve image view
            {}, // resolve image layout
            vk::AttachmentLoadOp::eLoad,
            vk::AttachmentStoreOp::eStore,
            vk::ClearColorValue() // would be used with vk::AttachmentLoadOp::eClear
            )};

    renderTarget.commandBuffer().beginRendering(vk::RenderingInfo(
        vk::RenderingFlags(),
        vk::Rect2D(
            vk::Offset2D(0, 0),
            vk::Extent2D(renderTarget.size().width(), renderTarget.size().height())),
        1, // layer count
        0, // view mask
        colorAttachments,
        {}, // depth attachment
        {} // stencil attachment
        ));
    renderTarget.commandBuffer().setViewport(0, vk::Viewport(0, 0, // x, y
                                                             renderTarget.size().width(), renderTarget.size().height(), // width, height
                                                             0, 1 // min depth, max depth
                                                             ));
}

void ItemRendererVulkan::endFrame(const RenderTarget &renderTarget)
{
    renderTarget.commandBuffer().endRendering();
    if (renderTarget.commandBuffer().end() != vk::Result::eSuccess) {
        qWarning() << "command buffer end failed";
        return;
    }
}

void ItemRendererVulkan::renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region)
{
    if (region.isEmpty()) {
        return;
    }
    std::vector<vk::ClearRect> rects;
    rects.reserve(region.rectCount());
    for (const auto &rect : region) {
        rects.push_back(vk::ClearRect(vk::Rect2D(vk::Offset2D(rect.x(), rect.y()), vk::Extent2D(rect.width(), rect.height())), 0, 1));
    }
    renderTarget.commandBuffer().clearAttachments(vk::ClearAttachment(
                                                      vk::ImageAspectFlagBits::eColor,
                                                      0,
                                                      vk::ClearColorValue(0.2f, 0.2f, 0.8f, 1.0f)),
                                                  rects);
}

struct RenderNode
{
    VulkanTexture *texture = nullptr;
    // RenderGeometry geometry;
    // QMatrix4x4 transformationMatrix;
    QRectF rect;
    QColor color;
};

static void createRenderNodes(Item *item, std::vector<RenderNode> &list)
{
    const QList<Item *> sortedChildItems = item->sortedChildItems();
    for (const auto child : sortedChildItems) {
        if (child->explicitVisible()) {
            createRenderNodes(item, list);
        }
    }

    item->preprocess();

    if (auto shadowItem = qobject_cast<ShadowItem *>(item)) {
        list.push_back(RenderNode{
            .rect = QRect(),
        });
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(item)) {
        list.push_back(RenderNode{
            .rect = QRect(),
        });
    } else if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        SurfacePixmap *pixmap = surfaceItem->pixmap();
        if (pixmap) {
            list.push_back(RenderNode{
                .rect = surfaceItem->boundingRect(),
                .color = QColor(0, 255, 0),
            });
        }
    } else if (auto imageItem = qobject_cast<ImageItemOpenGL *>(item)) {
        list.push_back(RenderNode{
            .rect = imageItem->boundingRect(),
            .color = QColor(255, 0, 0),
        });
    }
}

void ItemRendererVulkan::renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data)
{
    if (region.isEmpty()) {
        return;
    }
    std::vector<RenderNode> renderNodes;
    createRenderNodes(item, renderNodes);

    for (const auto &node : renderNodes) {
        std::vector<vk::ClearRect> rects;
        rects.push_back(vk::ClearRect(vk::Rect2D(
                                          vk::Offset2D(node.rect.x(), node.rect.y()),
                                          vk::Extent2D(node.rect.width(), node.rect.height())),
                                      0, 1));
        renderTarget.commandBuffer().clearAttachments(vk::ClearAttachment(
                                                          vk::ImageAspectFlagBits::eColor,
                                                          0,
                                                          vk::ClearColorValue(node.color.red(), node.color.green(), node.color.blue(), 255)),
                                                      rects);
    }
}

std::unique_ptr<ImageItem> ItemRendererVulkan::createImageItem(Scene *scene, Item *parent)
{
    return std::make_unique<ImageItemVulkan>(scene, parent);
}

}
