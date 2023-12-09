/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer_opengl.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "platformsupport/scenes/opengl/openglsurfacetexture.h"
#include "scene/decorationitem.h"
#include "scene/imageitem.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/workspacescene_opengl.h"
#include "utils/common.h"

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

std::unique_ptr<ImageItem> ItemRendererOpenGL::createImageItem(Scene *scene, Item *parent)
{
    return std::make_unique<ImageItemOpenGL>(scene, parent);
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

static OpenGLSurfaceContents bindSurfaceTexture(SurfaceItem *surfaceItem)
{
    SurfacePixmap *surfacePixmap = surfaceItem->pixmap();
    auto platformSurfaceTexture =
        static_cast<OpenGLSurfaceTexture *>(surfacePixmap->texture());
    if (surfacePixmap->isDiscarded()) {
        return platformSurfaceTexture->texture();
    }

    if (platformSurfaceTexture->texture().isValid()) {
        const QRegion region = surfaceItem->damage();
        if (!region.isEmpty()) {
            platformSurfaceTexture->update(region);
            surfaceItem->resetDamage();
        }
    } else {
        if (!surfacePixmap->isValid()) {
            return {};
        }
        if (!platformSurfaceTexture->create()) {
            qCDebug(KWIN_OPENGL) << "Failed to bind window";
            return {};
        }
        surfaceItem->resetDamage();
    }

    return platformSurfaceTexture->texture();
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
            QRectF deviceBounds = snapToPixelGridF(scaledRect(quad.bounds(), scale));

            for (const QRect &clipRect : std::as_const(context->clip)) {
                QRectF deviceClipRect = snapToPixelGridF(scaledRect(clipRect, scale)).translated(-worldTranslation);

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

    const auto logicalPosition = QVector2D(item->position().x(), item->position().y());
    const auto scale = context->renderTargetScale;

    QMatrix4x4 matrix;
    matrix.translate(roundVector(logicalPosition * scale).toVector3D());
    if (context->transformStack.size() == 1) {
        matrix *= context->rootTransform;
    }
    if (!item->transform().isIdentity()) {
        matrix.scale(scale, scale);
        matrix *= item->transform();
        matrix.scale(1 / scale, 1 / scale);
    }
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
                .colorDescription = item->colorDescription(),
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
                .colorDescription = item->colorDescription(),
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
                    .colorDescription = item->colorDescription(),
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
                .colorDescription = item->colorDescription(),
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

        const auto targetSize = renderTarget.size();
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
        .rootTransform = data.toMatrix(viewport.scale()), // TODO: unify transforms
        .clip = region,
        .hardwareClipping = region != infiniteRegion() && ((mask & Scene::PAINT_WINDOW_TRANSFORMED) || (mask & Scene::PAINT_SCREEN_TRANSFORMED)),
        .renderTargetScale = viewport.scale(),
    };

    renderContext.transformStack.push(QMatrix4x4());
    renderContext.opacityStack.push(data.opacity());

    createRenderNode(item, &renderContext);

    int totalVertexCount = 0;
    for (const RenderNode &node : std::as_const(renderContext.renderNodes)) {
        totalVertexCount += node.geometry.count();
    }
    if (totalVertexCount == 0) {
        return;
    }

    ShaderTraits baseShaderTraits = ShaderTrait::MapTexture;
    if (data.brightness() != 1.0) {
        baseShaderTraits |= ShaderTrait::Modulate;
    }
    if (data.saturation() != 1.0) {
        baseShaderTraits |= ShaderTrait::AdjustSaturation;
    }

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    const auto map = vbo->map<GLVertex2D>(totalVertexCount);
    if (!map) {
        return;
    }

    for (int i = 0, v = 0; i < renderContext.renderNodes.count(); i++) {
        RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.geometry.isEmpty()
            || (std::holds_alternative<GLTexture *>(renderNode.texture) && !std::get<GLTexture *>(renderNode.texture))
            || (std::holds_alternative<OpenGLSurfaceContents>(renderNode.texture) && !std::get<OpenGLSurfaceContents>(renderNode.texture).isValid())) {
            continue;
        }

        renderNode.firstVertex = v;
        renderNode.vertexCount = renderNode.geometry.count();

        GLTexture *texture = nullptr;
        if (std::holds_alternative<GLTexture *>(renderNode.texture)) {
            texture = std::get<GLTexture *>(renderNode.texture);
        } else {
            texture = std::get<OpenGLSurfaceContents>(renderNode.texture).planes.constFirst().get();
        }
        renderNode.geometry.postProcessTextureCoordinates(texture->matrix(renderNode.coordinateType));

        renderNode.geometry.copy(map->subspan(v));
        v += renderNode.geometry.count();
    }

    vbo->unmap();
    vbo->bindArrays();

    if (renderContext.hardwareClipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    // Make sure the blend function is set up correctly in case we will be doing blending
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // The scissor region must be in the render target local coordinate system.
    QRegion scissorRegion = infiniteRegion();
    if (renderContext.hardwareClipping) {
        scissorRegion = viewport.mapToRenderTarget(region);
    }

    ShaderTraits lastTraits;
    GLShader *shader = nullptr;
    for (int i = 0; i < renderContext.renderNodes.count(); i++) {
        const RenderNode &renderNode = renderContext.renderNodes[i];
        if (renderNode.vertexCount == 0) {
            continue;
        }

        setBlendEnabled(renderNode.hasAlpha || renderNode.opacity < 1.0);

        ShaderTraits traits = baseShaderTraits;
        if (renderNode.opacity != 1.0) {
            traits |= ShaderTrait::Modulate;
        }
        if (renderNode.colorDescription != renderTarget.colorDescription()) {
            traits |= ShaderTrait::TransformColorspace;
        }
        if (!shader || traits != lastTraits) {
            lastTraits = traits;
            if (shader) {
                ShaderManager::instance()->popShader();
            }
            shader = ShaderManager::instance()->pushShader(traits);
            if (traits & ShaderTrait::AdjustSaturation) {
                const auto toXYZ = renderTarget.colorDescription().colorimetry().toXYZ();
                shader->setUniform(GLShader::FloatUniform::Saturation, data.saturation());
                shader->setUniform(GLShader::Vec3Uniform::PrimaryBrightness, QVector3D(toXYZ(1, 0), toXYZ(1, 1), toXYZ(1, 2)));
            }

            if (traits & ShaderTrait::MapTexture) {
                shader->setUniform(GLShader::IntUniform::Sampler, 0);
                shader->setUniform(GLShader::IntUniform::Sampler1, 1);
            }
        }
        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, renderContext.projectionMatrix * renderNode.transformMatrix);
        if (traits & ShaderTrait::Modulate) {
            shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, modulate(renderNode.opacity, data.brightness()));
        }
        if (traits & ShaderTrait::TransformColorspace) {
            shader->setColorspaceUniforms(renderNode.colorDescription, renderTarget.colorDescription());
        }

        if (std::holds_alternative<GLTexture *>(renderNode.texture)) {
            const auto texture = std::get<GLTexture *>(renderNode.texture);
            glActiveTexture(GL_TEXTURE0);
            shader->setUniform("converter", 0);
            texture->bind();
        } else {
            const auto contents = std::get<OpenGLSurfaceContents>(renderNode.texture);
            shader->setUniform("converter", contents.planes.count() > 1);
            for (int plane = 0; plane < contents.planes.count(); ++plane) {
                glActiveTexture(GL_TEXTURE0 + plane);
                contents.planes[plane]->bind();
            }
        }

        vbo->draw(scissorRegion, GL_TRIANGLES, renderNode.firstVertex,
                  renderNode.vertexCount, renderContext.hardwareClipping);

        if (std::holds_alternative<GLTexture *>(renderNode.texture)) {
            auto texture = std::get<GLTexture *>(renderNode.texture);
            glActiveTexture(GL_TEXTURE0);
            texture->unbind();
        } else {
            const auto contents = std::get<OpenGLSurfaceContents>(renderNode.texture);
            for (int plane = 0; plane < contents.planes.count(); ++plane) {
                glActiveTexture(GL_TEXTURE0 + plane);
                contents.planes[plane]->unbind();
            }
        }
    }
    if (shader) {
        ShaderManager::instance()->popShader();
    }

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

        QVector2D size;
        if (std::holds_alternative<GLTexture *>(renderNode.texture)) {
            auto texture = std::get<GLTexture *>(renderNode.texture);
            size = QVector2D(texture->width(), texture->height());
        } else {
            auto texture = std::get<OpenGLSurfaceContents>(renderNode.texture).planes.constFirst().get();
            size = QVector2D(texture->width(), texture->height());
        }

        m_debug.fractionalShader->setUniform("geometrySize", size);
        m_debug.fractionalShader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, renderContext.projectionMatrix * renderNode.transformMatrix);

        vbo->draw(region, GL_TRIANGLES, renderNode.firstVertex,
                  renderNode.vertexCount, renderContext.hardwareClipping);
    }
}

} // namespace KWin
