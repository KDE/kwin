/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/colorspace.h"
#include "scene/borderradius.h"
#include "scene/itemgeometry.h"
#include "scene/itemrenderer.h"

#include <QList>
#include <QStack>
#include <vulkan/vulkan_raii.hpp>

namespace KWin
{

class VulkanDevice;
class VulkanTexture;
class VulkanShader;
class SyncReleasePoint;

class ItemRendererVulkan : public ItemRenderer
{
public:
    struct RenderNode
    {
        VulkanTexture *texture;
        QMatrix4x4 transformMatrix;
        int firstVertex = 0;
        int vertexCount = 0;
        qreal opacity = 1;
        bool hasAlpha = false;
        std::shared_ptr<ColorDescription> colorDescription;
        RenderingIntent renderingIntent;
        std::shared_ptr<SyncReleasePoint> bufferReleasePoint;
        QVector4D box;
        QVector4D borderRadius;
        int borderThickness = 0;
        QColor borderColor;
        bool paintHole = false;
        bool hasFloatingPointColor = false;
    };

    struct ItemData
    {
        QVector4D rect;
        QVector4D color;
        int textureIndex;
        float outlineWidth;
        int padding;
        int padding2;
        QVector4D box;
        QVector4D cornerRadius;
    };

    struct RenderCorner
    {
        RectF box;
        BorderRadius radius;
    };

    struct RenderContext
    {
        QList<RenderNode> renderNodes;
        QStack<QMatrix4x4> transformStack;
        QStack<qreal> opacityStack;
        QStack<RenderCorner> cornerStack;
        const QMatrix4x4 projectionMatrix;
        const QMatrix4x4 rootTransform;
        const Region deviceClip;
        const bool hardwareClipping;
        const qreal renderTargetScale;
        const QPointF viewportOrigin;
        const QPoint renderOffset;
        const QSize viewportSize;
        std::vector<ItemData> uniform;
        std::vector<VulkanTexture *> textures;
    };

    explicit ItemRendererVulkan(VulkanDevice *device);
    ~ItemRendererVulkan() override;

    std::unique_ptr<Texture> createTexture(GraphicsBuffer *buffer) override;
    std::unique_ptr<Texture> createTexture(const QImage &image) override;

    std::unique_ptr<NinePatch> createNinePatch(const QImage &image) override;
    std::unique_ptr<NinePatch> createNinePatch(const QImage &topLeftPatch,
                                               const QImage &topPatch,
                                               const QImage &topRightPatch,
                                               const QImage &rightPatch,
                                               const QImage &bottomRightPatch,
                                               const QImage &bottomPatch,
                                               const QImage &bottomLeftPatch,
                                               const QImage &leftPatch) override;

    std::unique_ptr<Atlas> createAtlas(const QList<QImage> &sprites) override;

    void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport) override;
    void endFrame() override;

    void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const Region &deviceRegion) override;
    void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const Region &deviceRegion, const WindowPaintData &data, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter) override;

private:
    void createRenderNode(Item *item, RenderContext *context, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter);

    VulkanDevice *const m_device;
    std::unordered_set<std::shared_ptr<SyncReleasePoint>> m_releasePoints;
    std::unique_ptr<VulkanShader> m_shader;
};

}
