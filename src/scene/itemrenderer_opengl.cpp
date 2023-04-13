/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer_opengl.h"
#include "libkwineffects/rendertarget.h"
#include "libkwineffects/renderviewport.h"
#include "platformsupport/scenes/opengl/openglsurfacetexture.h"
#include "scene/decorationitem.h"
#include "scene/imageitem.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/workspacescene_opengl.h"

namespace KWin
{

ItemRendererOpenGL::ItemRendererOpenGL()
{
    const QString visualizeOptionsString = qEnvironmentVariable("KWIN_SCENE_VISUALIZE");
    if (!visualizeOptionsString.isEmpty()) {
        const QStringList visualtizeOptions = visualizeOptionsString.split(';');
        m_debug.fractionalEnabled = visualtizeOptions.contains(QLatin1StringView("fractional"));
    }
}

ImageItem *ItemRendererOpenGL::createImageItem(Scene *scene, Item *parent)
{
    return new ImageItemOpenGL(scene, parent);
}

void ItemRendererOpenGL::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
    GLFramebuffer *fbo = renderTarget.framebuffer();
    GLFramebuffer::pushFramebuffer(fbo);

    GLVertexBuffer::streamingBuffer()->beginFrame();
}

void ItemRendererOpenGL::endFrame()
{
    GLVertexBuffer::streamingBuffer()->endOfFrame();
    GLFramebuffer::popFramebuffer();
}

QVector4D ItemRendererOpenGL::modulate(float opacity, float brightness) const
{
    const float a = opacity;
    const float rgb = opacity * brightness;

    return QVector4D(rgb, rgb, rgb, a);
}

void ItemRendererOpenGL::setBlendEnabled(bool enabled)
{
    if (enabled && !m_blendingEnabled) {
        glEnable(GL_BLEND);
    } else if (!enabled && m_blendingEnabled) {
        glDisable(GL_BLEND);
    }

    m_blendingEnabled = enabled;
}

static GLTexture *bindSurfaceTexture(SurfaceItem *surfaceItem)
{
    SurfacePixmap *surfacePixmap = surfaceItem->pixmap();
    auto platformSurfaceTexture =
        static_cast<OpenGLSurfaceTexture *>(surfacePixmap->texture());
    if (surfacePixmap->isDiscarded()) {
        return platformSurfaceTexture->texture();
    }

    if (platformSurfaceTexture->texture()) {
        const QRegion region = surfaceItem->damage();
        if (!region.isEmpty()) {
            platformSurfaceTexture->update(region);
            surfaceItem->resetDamage();
        }
    } else {
        if (!surfacePixmap->isValid()) {
            return nullptr;
        }
        if (!platformSurfaceTexture->create()) {
            qCDebug(KWIN_OPENGL) << "Failed to bind window";
            return nullptr;
        }
        surfaceItem->resetDamage();
    }

    return platformSurfaceTexture->texture();
}

static QRectF logicalRectToDeviceRect(const QRectF &logical, qreal deviceScale)
{
    return QRectF(QPointF(std::round(logical.left() * deviceScale), std::round(logical.top() * deviceScale)),
                  QPointF(std::round(logical.right() * deviceScale), std::round(logical.bottom() * deviceScale)));
}

static RenderGeometry clipQuads(const Item *item, const ItemRendererOpenGL::RenderContext *context)
{
    const WindowQuadList quads = item->quads();

    // Item to world translation.
    const QPointF worldTranslation = context->transformStack.top().map(QPointF(0., 0.));
    const qreal scale = context->renderTargetScale;

    RenderGeometry geometry;
    geometry.reserve(quads.count() * 6);

    // split all quads in bounding rect with the actual rects in the region
    for (const WindowQuad &quad : std::as_const(quads)) {
        if (context->clip != infiniteRegion() && !context->hardwareClipping) {
            // Scale to device coordinates, rounding as needed.
            QRectF deviceBounds = logicalRectToDeviceRect(quad.bounds(), scale);

            for (const QRect &clipRect : std::as_const(context->clip)) {
                QRectF deviceClipRect = logicalRectToDeviceRect(clipRect, scale).translated(-worldTranslation);

                const QRectF &intersected = deviceClipRect.intersected(deviceBounds);
                if (intersected.isValid()) {
                    if (deviceBounds == intersected) {
                        // case 1: completely contains, include and do not check other rects
                        geometry.appendWindowQuad(quad, scale);
                        break;
                    }
                    // case 2: intersection
                    geometry.appendSubQuad(quad, intersected, scale);
                }
            }
        } else {
            geometry.appendWindowQuad(quad, scale);
        }
    }

    return geometry;
}

void ItemRendererOpenGL::createRenderNode(Item *item, RenderContext *context)
{
    const QList<Item *> sortedChildItems = item->sortedChildItems();

    QMatrix4x4 matrix;
    const auto logicalPosition = QVector2D(item->position().x(), item->position().y());
    const auto scale = context->renderTargetScale;
    matrix.translate(roundVector(logicalPosition * scale).toVector3D());
    matrix *= item->transform();
    context->transformStack.push(context->transformStack.top() * matrix);

    context->opacityStack.push(context->opacityStack.top() * item->opacity());

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() >= 0) {
            break;
        }
        if (childItem->explicitVisible()) {
            createRenderNode(childItem, context);
        }
    }

    item->preprocess();

    RenderGeometry geometry = clipQuads(item, context);

    if (auto shadowItem = qobject_cast<ShadowItem *>(item)) {
        if (!geometry.isEmpty()) {
            OpenGLShadowTextureProvider *textureProvider = static_cast<OpenGLShadowTextureProvider *>(shadowItem->textureProvider());
            context->renderNodes.append(RenderNode{
                .texture = textureProvider->shadowTexture(),
                .geometry = geometry,
                .transformMatrix = context->transformStack.top(),
                .opacity = context->opacityStack.top(),
                .hasAlpha = true,
                .coordinateType = UnnormalizedCoordinates,
                .scale = scale,
            });
        }
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(item)) {
        if (!geometry.isEmpty()) {
            auto renderer = static_cast<const SceneOpenGLDecorationRenderer *>(decorationItem->renderer());
            context->renderNodes.append(RenderNode{
                .texture = renderer->texture(),
                .geometry = geometry,
                .transformMatrix = context->transformStack.top(),
                .opacity = context->opacityStack.top(),
                .hasAlpha = true,
                .coordinateType = UnnormalizedCoordinates,
                .scale = scale,
            });
        }
    } else if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        SurfacePixmap *pixmap = surfaceItem->pixmap();
        if (pixmap) {
            if (!geometry.isEmpty()) {
                context->renderNodes.append(RenderNode{
                    .texture = bindSurfaceTexture(surfaceItem),
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = pixmap->hasAlphaChannel(),
                    .coordinateType = NormalizedCoordinates,
                    .scale = scale,
                });
            }
        }
    } else if (auto imageItem = qobject_cast<ImageItemOpenGL *>(item)) {
        if (!geometry.isEmpty()) {
            context->renderNodes.append(RenderNode{
                .texture = imageItem->texture(),
                .geometry = geometry,
                .transformMatrix = context->transformStack.top(),
                .opacity = context->opacityStack.top(),
                .hasAlpha = imageItem->image().hasAlphaChannel(),
                .coordinateType = NormalizedCoordinates,
                .scale = scale,
            });
        }
    }

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() < 0) {
            continue;
        }
        if (childItem->explicitVisible()) {
            createRenderNode(childItem, context);
        }
    }

    context->transformStack.pop();
    context->opacityStack.pop();
}

void ItemRendererOpenGL::renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region)
{
    if (region == infiniteRegion() || (region.rectCount() == 1 && (*region.begin()) == viewport.renderRect())) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    } else if (!region.isEmpty()) {
        glClearColor(0, 0, 0, 0);
        glEnable(GL_SCISSOR_TEST);

        const auto targetSize = viewport.mapToRenderTarget(viewport.renderRect());

        for (const QRect &r : region) {
            const auto deviceRect = viewport.mapToRenderTarget(r);
            glScissor(deviceRect.x(), targetSize.height() - (deviceRect.y() + deviceRect.height()), deviceRect.width(), deviceRect.height());
            glClear(GL_COLOR_BUFFER_BIT);
        }

        glDisable(GL_SCISSOR_TEST);
    }
}

void ItemRendererOpenGL::renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data)
{
    if (region.isEmpty()) {
        return;
    }

    RenderContext renderContext{
        .projectionMatrix = data.projectionMatrix(),
        .clip = region,
        .hardwareClipping = region != infiniteRegion() && ((mask & Scene::PAINT_WINDOW_TRANSFORMED) || (mask & Scene::PAINT_SCREEN_TRANSFORMED)),
        .renderTargetScale = viewport.scale(),
    };

    renderContext.transformStack.push(QMatrix4x4());
    renderContext.opacityStack.push(data.opacity());

    item->setTransform(data.toMatrix(renderContext.renderTargetScale));

    createRenderNode(item, &renderContext);

    int totalVertexCount = 0;
    for (const RenderNode &node : std::as_const(renderContext.renderNodes)) {
        totalVertexCount += node.geometry.count();
    }
    if (totalVertexCount == 0) {
        return;
    }

    const size_t size = totalVertexCount * sizeof(GLVertex2D);

    ShaderTraits shaderTraits = ShaderTrait::MapTexture;

    if (data.brightness() != 1.0) {
        shaderTraits |= ShaderTrait::Modulate;
    }
    if (data.saturation() != 1.0) {
        shaderTraits |= ShaderTrait::AdjustSaturation;
    }

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(GLVertexBuffer::GLVertex2DLayout, 2, sizeof(GLVertex2D));

    GLVertex2D *map = (GLVertex2D *)vbo->map(size);

    for (int i = 0, v = 0; i < renderContext.renderNodes.count(); i++) {
        RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.geometry.isEmpty() || !renderNode.texture) {
            continue;
        }

        if (renderNode.opacity != 1.0) {
            shaderTraits |= ShaderTrait::Modulate;
        }

        renderNode.firstVertex = v;
        renderNode.vertexCount = renderNode.geometry.count();

        renderNode.geometry.postProcessTextureCoordinates(renderNode.texture->matrix(renderNode.coordinateType));

        renderNode.geometry.copy(std::span(&map[v], renderNode.geometry.count()));
        v += renderNode.geometry.count();
    }

    vbo->unmap();
    vbo->bindArrays();

    GLShader *shader = ShaderManager::instance()->pushShader(shaderTraits);
    shader->setUniform(GLShader::Saturation, data.saturation());

    if (renderContext.hardwareClipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    // Make sure the blend function is set up correctly in case we will be doing blending
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    float opacity = -1.0;

    // The scissor region must be in the render target local coordinate system.
    QRegion scissorRegion = infiniteRegion();
    if (renderContext.hardwareClipping) {
        scissorRegion = viewport.mapToRenderTarget(region);
    }

    for (int i = 0; i < renderContext.renderNodes.count(); i++) {
        const RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.vertexCount == 0) {
            continue;
        }

        setBlendEnabled(renderNode.hasAlpha || renderNode.opacity < 1.0);

        shader->setUniform(GLShader::ModelViewProjectionMatrix, renderContext.projectionMatrix * renderNode.transformMatrix);
        if (opacity != renderNode.opacity) {
            shader->setUniform(GLShader::ModulationConstant,
                               modulate(renderNode.opacity, data.brightness()));
            opacity = renderNode.opacity;
        }

        renderNode.texture->setFilter(GL_LINEAR);
        renderNode.texture->setWrapMode(GL_CLAMP_TO_EDGE);
        renderNode.texture->bind();

        vbo->draw(scissorRegion, GL_TRIANGLES, renderNode.firstVertex,
                  renderNode.vertexCount, renderContext.hardwareClipping);
    }

    ShaderManager::instance()->popShader();

    if (m_debug.fractionalEnabled) {
        visualizeFractional(viewport, scissorRegion, renderContext);
    }

    vbo->unbindArrays();

    setBlendEnabled(false);

    if (renderContext.hardwareClipping) {
        glDisable(GL_SCISSOR_TEST);
    }
}

void ItemRendererOpenGL::visualizeFractional(const RenderViewport &viewport, const QRegion &region, const RenderContext &renderContext)
{
    if (!m_debug.fractionalShader) {
        m_debug.fractionalShader = ShaderManager::instance()->generateShaderFromFile(
            ShaderTrait::MapTexture,
            QStringLiteral(":/scene/shaders/debug_fractional.vert"),
            QStringLiteral(":/scene/shaders/debug_fractional.frag"));
    }

    if (!m_debug.fractionalShader) {
        return;
    }

    ShaderBinder debugShaderBinder(m_debug.fractionalShader.get());
    m_debug.fractionalShader->setUniform("fractionalPrecision", 0.01f);

    auto screenSize = viewport.renderRect().size() * viewport.scale();
    m_debug.fractionalShader->setUniform("screenSize", QVector2D(float(screenSize.width()), float(screenSize.height())));

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();

    for (int i = 0; i < renderContext.renderNodes.count(); i++) {
        const RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.vertexCount == 0) {
            continue;
        }

        setBlendEnabled(true);

        m_debug.fractionalShader->setUniform("geometrySize", QVector2D(renderNode.texture->width(), renderNode.texture->height()));
        m_debug.fractionalShader->setUniform(GLShader::ModelViewProjectionMatrix, renderContext.projectionMatrix * renderNode.transformMatrix);

        vbo->draw(region, GL_TRIANGLES, renderNode.firstVertex,
                  renderNode.vertexCount, renderContext.hardwareClipping);
    }
}

} // namespace KWin
