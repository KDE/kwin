/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opengl/glutils.h"
#include "scene/itemrenderer.h"
#include "scene/surfaceitem.h"

#include <unordered_set>

namespace KWin
{

class EglDisplay;

class KWIN_EXPORT ItemRendererOpenGL : public ItemRenderer
{
public:
    struct RenderNode
    {
        ShaderTraits traits;
        QVarLengthArray<GLTexture *, 4> textures;
        RenderGeometry geometry;
        QMatrix4x4 transformMatrix;
        int firstVertex = 0;
        int vertexCount = 0;
        qreal opacity = 1;
        bool hasAlpha = false;
        ColorDescription colorDescription;
        RenderingIntent renderingIntent;
        std::shared_ptr<SyncReleasePoint> bufferReleasePoint;
        QVector4D box;
        QVector4D borderRadius;
        int borderThickness = 0;
        QColor borderColor;
    };

    struct RenderContext
    {
        QList<RenderNode> renderNodes;
        QStack<QMatrix4x4> transformStack;
        QStack<qreal> opacityStack;
        const QMatrix4x4 projectionMatrix;
        const QMatrix4x4 rootTransform;
        const QRegion clip;
        const bool hardwareClipping;
        const qreal renderTargetScale;
    };

    ItemRendererOpenGL(EglDisplay *eglDisplay);

    void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport) override;
    void endFrame() override;

    void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region) override;
    void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data, const std::function<bool(Item *)> &filter) override;

    std::unique_ptr<ImageItem> createImageItem(Item *parent = nullptr) override;

private:
    QVector4D modulate(float opacity, float brightness) const;
    void setBlendEnabled(bool enabled);
    void createRenderNode(Item *item, RenderContext *context, const std::function<bool(Item *)> &filter);
    void visualizeFractional(const RenderViewport &viewport, const QRegion &region, const RenderContext &renderContext);

    bool m_blendingEnabled = false;
    EglDisplay *const m_eglDisplay;
    std::unordered_set<std::shared_ptr<SyncReleasePoint>> m_releasePoints;

    struct
    {
        bool fractionalEnabled = false;
        std::unique_ptr<GLShader> fractionalShader;
    } m_debug;
};

} // namespace KWin
