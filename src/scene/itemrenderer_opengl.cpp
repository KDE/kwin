/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer_opengl.h"
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
}

ImageItem *ItemRendererOpenGL::createImageItem(Scene *scene, Item *parent)
{
    return new ImageItemOpenGL(scene, parent);
}

void ItemRendererOpenGL::beginFrame(RenderTarget *renderTarget)
{
    GLFramebuffer *fbo = std::get<GLFramebuffer *>(renderTarget->nativeHandle());
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
            SceneOpenGLShadow *shadow = static_cast<SceneOpenGLShadow *>(shadowItem->shadow());
            context->renderNodes.append(RenderNode{
                .texture = shadow->shadowTexture(),
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

void ItemRendererOpenGL::renderBackground(const QRegion &region)
{
    if (region == infiniteRegion() || (region.rectCount() == 1 && (*region.begin()) == renderTargetRect())) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    } else if (!region.isEmpty()) {
        glClearColor(0, 0, 0, 0);
        glEnable(GL_SCISSOR_TEST);

        const auto scale = renderTargetScale();
        const auto targetRect = scaledRect(renderTargetRect(), scale).toRect();

        for (const QRect &r : region) {
            auto deviceRect = scaledRect(r, scale).toAlignedRect();
            glScissor(deviceRect.x(), targetRect.height() - (deviceRect.y() + deviceRect.height()), deviceRect.width(), deviceRect.height());
            glClear(GL_COLOR_BUFFER_BIT);
        }

        glDisable(GL_SCISSOR_TEST);
    }
}

void ItemRendererOpenGL::renderItem(Item *item, int mask, const QRegion &region, const WindowPaintData &data)
{
    if (region.isEmpty()) {
        return;
    }

    RenderContext renderContext{
        .clip = region,
        .hardwareClipping = region != infiniteRegion() && ((mask & Scene::PAINT_WINDOW_TRANSFORMED) || (mask & Scene::PAINT_SCREEN_TRANSFORMED)),
        .renderTargetScale = data.renderTargetScale().value_or(renderTargetScale()),
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
        scissorRegion = mapToRenderTarget(region);
    }

    const QMatrix4x4 projectionMatrix = data.projectionMatrix();
    for (int i = 0; i < renderContext.renderNodes.count(); i++) {
        const RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.vertexCount == 0) {
            continue;
        }

        setBlendEnabled(renderNode.hasAlpha || renderNode.opacity < 1.0);

        shader->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix * renderNode.transformMatrix);
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

    vbo->unbindArrays();

    setBlendEnabled(false);

    ShaderManager::instance()->popShader();

    if (renderContext.hardwareClipping) {
        glDisable(GL_SCISSOR_TEST);
    }
}

} // namespace KWin
