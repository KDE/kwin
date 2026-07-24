/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer_opengl.h"
#include "core/colorpipeline.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "core/syncobjtimeline.h"
#include "effect/effect.h"
#include "opengl/eglnativefence.h"
#include "scene/decorationitem.h"
#include "scene/imageitem.h"
#include "scene/mirroritem.h"
#include "scene/opengl/atlas.h"
#include "scene/opengl/ninepatch.h"
#include "scene/opengl/texture.h"
#include "scene/outlinedborderitem.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/workspacescene.h"
#include "utils/common.h"

namespace KWin
{

ItemRendererOpenGL::ItemRendererOpenGL(EglDisplay *eglDisplay)
    : m_eglDisplay(eglDisplay)
{
    const QString visualizeOptionsString = qEnvironmentVariable("KWIN_SCENE_VISUALIZE");
    if (!visualizeOptionsString.isEmpty()) {
        const QStringList visualtizeOptions = visualizeOptionsString.split(';');
        m_debug.fractionalEnabled = visualtizeOptions.contains(QLatin1StringView("fractional"));
    }
}

std::unique_ptr<Texture> ItemRendererOpenGL::createTexture(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint)
{
    return BufferTextureOpenGL::create(buffer, releasePoint);
}

std::unique_ptr<Texture> ItemRendererOpenGL::createTexture(const QImage &image)
{
    return ImageTextureOpenGL::create(image);
}

std::unique_ptr<NinePatch> ItemRendererOpenGL::createNinePatch(const QImage &image)
{
    return NinePatchOpenGL::create(image);
}

std::unique_ptr<NinePatch> ItemRendererOpenGL::createNinePatch(const QImage &topLeftPatch,
                                                               const QImage &topPatch,
                                                               const QImage &topRightPatch,
                                                               const QImage &rightPatch,
                                                               const QImage &bottomRightPatch,
                                                               const QImage &bottomPatch,
                                                               const QImage &bottomLeftPatch,
                                                               const QImage &leftPatch)
{
    return NinePatchOpenGL::create(topLeftPatch, topPatch, topRightPatch, rightPatch, bottomRightPatch, bottomPatch, bottomLeftPatch, leftPatch);
}

std::unique_ptr<Atlas> ItemRendererOpenGL::createAtlas(const QList<QImage> &sprites)
{
    return AtlasOpenGL::create(sprites);
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

    if (m_eglDisplay) {
        EGLNativeFence fence(m_eglDisplay);
        if (fence.isValid()) {
            for (const auto &releasePoint : m_releasePoints) {
                releasePoint->addReleaseFence(fence.fileDescriptor());
            }
        }
    }
    m_releasePoints.clear();
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

static RenderGeometry getQuads(const Item *item, const ItemRendererOpenGL::RenderContext *context)
{
    const WindowQuadList quads = item->quads();

    RenderGeometry geometry;
    geometry.reserve(quads.count());

    for (const WindowQuad &quad : std::as_const(quads)) {
        geometry.appendWindowQuad(quad, context->renderTargetScale);
    }

    return geometry;
}

bool ItemRendererOpenGL::createRenderNode(Item *item, RenderContext *context, Rect deviceClip, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter)
{
    bool hole = false;
    if (filter && filter(item)) {
        if (!holeFilter || !holeFilter(item)) {
            return true;
        }
        hole = true;
    }
    const auto scale = context->renderTargetScale;
    if (auto clip = item->globalClipRect()) {
        deviceClip &= clip->scaled(scale).translated(-context->viewportOrigin).rounded();
        if (deviceClip.isEmpty()) {
            return true;
        }
    }

    const QList<Item *> sortedChildItems = item->sortedChildItems();

    const auto logicalPosition = QVector2D(item->position().x(), item->position().y());

    MirrorItem *mirror = qobject_cast<MirrorItem *>(item);
    Item *toRender = mirror ? mirror->source() : item;

    QMatrix4x4 matrix;
    matrix.translate(roundVector(logicalPosition * scale).toVector3D());
    if (context->transformStack.size() == 1) {
        matrix *= context->rootTransform;
    }
    if (mirror && !mirror->transform().isIdentity()) {
        matrix.scale(scale, scale);
        matrix *= mirror->transform();
        matrix.scale(1 / scale, 1 / scale);
    }
    if (!toRender->transform().isIdentity()) {
        matrix.scale(scale, scale);
        matrix *= toRender->transform();
        matrix.scale(1 / scale, 1 / scale);
    }
    context->transformStack.push(context->transformStack.top() * matrix);

    double opacity = toRender->opacity();
    if (mirror) {
        opacity *= mirror->opacity();
    }
    context->opacityStack.push(context->opacityStack.top() * opacity);

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() >= 0) {
            break;
        }
        if (childItem->explicitVisible()) {
            if (!createRenderNode(childItem, context, deviceClip, filter, holeFilter)) {
                return false;
            }
        }
    }

    if (const BorderRadius radius = toRender->borderRadius(); !radius.isNull()) {
        const RectF nativeRect = toRender->rect().scaled(context->renderTargetScale).rounded();
        const BorderRadius nativeRadius = radius.scaled(context->renderTargetScale).rounded();
        context->cornerStack.push({
            .box = nativeRect,
            .radius = nativeRadius,
        });
    } else if (!context->cornerStack.isEmpty()) {
        const auto &top = std::as_const(context->cornerStack).top();
        context->cornerStack.push({
            .box = matrix.inverted().mapRect(top.box),
            .radius = top.radius,
        });
    }

    // For multi-gpu copies, preprocess may change the active EGL context,
    // and switching back can fail in the case of a GPU reset.
    toRender->preprocess();
    if (!EglContext::currentContext()) {
        return false;
    }

    RenderGeometry geometry = getQuads(toRender, context);

    if (auto shadowItem = qobject_cast<ShadowItem *>(toRender)) {
        if (!geometry.isEmpty()) {
            const auto ninePatch = static_cast<NinePatchOpenGL *>(shadowItem->ninePatch());
            if (ninePatch->texture()) {
                RenderNode &renderNode = context->renderNodes.emplace_back(RenderNode{
                    .traits = ShaderTrait::MapTexture,
                    .textures = {ninePatch->texture()},
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = true,
                    .colorDescription = toRender->colorDescription(),
                    .renderingIntent = toRender->renderingIntent(),
                    .bufferReleasePoint = nullptr,
                    .paintHole = hole,
                    .clipRect = deviceClip,
                });
                renderNode.geometry.postProcessTextureCoordinates(ninePatch->texture()->matrix(UnnormalizedCoordinates));
            }
        }
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(toRender)) {
        if (!geometry.isEmpty()) {
            auto atlas = static_cast<const AtlasOpenGL *>(decorationItem->atlas());
            if (atlas && atlas->texture()) {
                RenderNode &renderNode = context->renderNodes.emplace_back(RenderNode{
                    .traits = ShaderTrait::MapTexture,
                    .textures = {atlas->texture()},
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = true,
                    .colorDescription = toRender->colorDescription(),
                    .renderingIntent = toRender->renderingIntent(),
                    .bufferReleasePoint = nullptr,
                    .paintHole = hole,
                    .clipRect = deviceClip,
                });
                renderNode.geometry.postProcessTextureCoordinates(atlas->texture()->matrix(UnnormalizedCoordinates));
            }
        }
    } else if (auto surfaceItem = qobject_cast<SurfaceItem *>(toRender)) {
        auto texture = static_cast<TextureOpenGL *>(surfaceItem->texture());
        if (texture && !texture->planes().isEmpty()) {
            if (!geometry.isEmpty()) {
                RenderNode &renderNode = context->renderNodes.emplace_back(RenderNode{
                    .traits = texture->planes().count() == 1 ? ShaderTrait::MapTexture : ShaderTrait::MapMultiPlaneTexture,
                    .textures = texture->planes(),
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = surfaceItem->hasAlphaChannel(),
                    .colorDescription = toRender->colorDescription(),
                    .renderingIntent = toRender->renderingIntent(),
                    .bufferReleasePoint = texture->releasePoint(),
                    .paintHole = hole,
                    .hasFloatingPointColor = texture->isFloatingPoint(),
                    .layerDebugBox = m_debug.layerEnabled ? std::optional(toRender->rect()) : std::nullopt,
                    .clipRect = deviceClip,
                });
                renderNode.geometry.postProcessTextureCoordinates(texture->planes().at(0)->matrix(UnnormalizedCoordinates));
                if (surfaceItem->colorDescription()->yuvCoefficients() != YUVMatrixCoefficients::Identity) {
                    renderNode.traits |= ShaderTrait::YuvConversion;
                }

                if (!context->cornerStack.isEmpty()) {
                    const auto &top = context->cornerStack.top();

                    renderNode.traits |= ShaderTrait::RoundedCorners;
                    renderNode.hasAlpha = true;
                    renderNode.box = QVector4D(top.box.x() + top.box.width() * 0.5,
                                               top.box.y() + top.box.height() * 0.5,
                                               top.box.width() * 0.5,
                                               top.box.height() * 0.5),
                    renderNode.borderRadius = top.radius.toVector();
                }
            }
        }
    } else if (auto imageItem = qobject_cast<ImageItem *>(toRender)) {
        if (!geometry.isEmpty()) {
            auto texture = static_cast<TextureOpenGL *>(imageItem->texture());
            if (texture && !texture->planes().isEmpty()) {
                RenderNode &renderNode = context->renderNodes.emplace_back(RenderNode{
                    .traits = ShaderTrait::MapTexture,
                    .textures = texture->planes(),
                    .geometry = geometry,
                    .transformMatrix = context->transformStack.top(),
                    .opacity = context->opacityStack.top(),
                    .hasAlpha = imageItem->image().hasAlphaChannel(),
                    .colorDescription = toRender->colorDescription(),
                    .renderingIntent = toRender->renderingIntent(),
                    .bufferReleasePoint = texture->releasePoint(),
                    .paintHole = hole,
                    .clipRect = deviceClip,
                });
                renderNode.geometry.postProcessTextureCoordinates(texture->planes()[0]->matrix(UnnormalizedCoordinates));
            }
        }
    } else if (auto borderItem = qobject_cast<OutlinedBorderItem *>(toRender)) {
        if (!geometry.isEmpty()) {
            const BorderOutline outline = borderItem->outline();
            const int thickness = std::round(outline.thickness() * context->renderTargetScale);
            const RectF outerRect = borderItem->rect().scaled(context->renderTargetScale).rounded();
            const RectF innerRect = outerRect.adjusted(thickness, thickness, -thickness, -thickness);
            context->renderNodes.append(RenderNode{
                .traits = ShaderTrait::Border,
                .geometry = geometry,
                .transformMatrix = context->transformStack.top(),
                .opacity = context->opacityStack.top(),
                .hasAlpha = true,
                .colorDescription = borderItem->colorDescription(),
                .renderingIntent = borderItem->renderingIntent(),
                .box = QVector4D(innerRect.x() + innerRect.width() * 0.5,
                                 innerRect.y() + innerRect.height() * 0.5,
                                 innerRect.width() * 0.5,
                                 innerRect.height() * 0.5),
                .borderRadius = outline.radius().scaled(context->renderTargetScale).rounded().toVector(),
                .borderThickness = thickness,
                .borderColor = outline.color(),
                .paintHole = hole,
                .clipRect = deviceClip,
            });
        }
    }

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() < 0) {
            continue;
        }
        if (childItem->explicitVisible()) {
            if (!createRenderNode(childItem, context, deviceClip, filter, holeFilter)) {
                return false;
            }
        }
    }

    context->transformStack.pop();
    context->opacityStack.pop();
    if (!context->cornerStack.isEmpty()) {
        context->cornerStack.pop();
    }
    return true;
}

void ItemRendererOpenGL::renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const Region &deviceRegion)
{
    const auto clipped = deviceRegion & renderTarget.transformedRect();
    if (clipped == renderTarget.transformedRect()) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    } else if (!clipped.isEmpty()) {
        glClearColor(0, 0, 0, 0);
        glEnable(GL_SCISSOR_TEST);

        const auto targetSize = renderTarget.size();
        for (const Rect &deviceRect : clipped.rects()) {
            const auto bufferRect = viewport.transform().map(deviceRect, renderTarget.transformedSize());
            glScissor(bufferRect.x(), targetSize.height() - (bufferRect.y() + bufferRect.height()), bufferRect.width(), bufferRect.height());
            glClear(GL_COLOR_BUFFER_BIT);
        }

        glDisable(GL_SCISSOR_TEST);
    }
}

bool ItemRendererOpenGL::renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const Region &deviceRegion, const WindowPaintData &data, const std::function<bool(Item *)> &filter, const std::function<bool(Item *)> &holeFilter)
{
    if (deviceRegion.isEmpty()) {
        return true;
    }

    RenderContext renderContext{
        .projectionMatrix = viewport.projectionMatrix(),
        .rootTransform = data.toMatrix(viewport.scale()), // TODO: unify transforms
        .deviceClip = deviceRegion & renderTarget.transformedRect(),
        .renderTargetScale = viewport.scale(),
        .viewportOrigin = viewport.scaledRenderRect().topLeft(),
        .renderOffset = viewport.renderOffset(),
        .deviceRect = viewport.deviceRect(),
    };

    renderContext.transformStack.push(QMatrix4x4());
    renderContext.opacityStack.push(data.opacity());

    if (!createRenderNode(item, &renderContext, viewport.deviceRect(), filter, holeFilter)) {
        return false;
    }

    int totalVertexCount = 0;
    for (const RenderNode &node : std::as_const(renderContext.renderNodes)) {
        totalVertexCount += node.geometry.count();
    }
    if (totalVertexCount == 0) {
        return true;
    }

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    const auto map = vbo->map<GLVertex2D>(totalVertexCount);
    if (!map) {
        EglContext::currentContext()->isFailed();
        return true;
    }

    for (int i = 0, v = 0; i < renderContext.renderNodes.count(); i++) {
        RenderNode &renderNode = renderContext.renderNodes[i];
        renderNode.firstVertex = v;
        renderNode.vertexCount = renderNode.geometry.count();
        renderNode.geometry.copy(map->subspan(v));
        v += renderNode.geometry.count();
    }

    vbo->unmap();
    vbo->bindArrays();

    glEnable(GL_SCISSOR_TEST);

    // Make sure the blend function is set up correctly in case we will be doing blending
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // The scissor region must be in the render target local coordinate system.
    const QSize bufferOffset = renderTarget.transform().map(QSize(viewport.renderOffset().x(), viewport.renderOffset().y()));
    const Rect offsetClip = Rect(QPoint(bufferOffset.width(), bufferOffset.height()), renderTarget.size() - 2 * bufferOffset);
    const Region scissorRegion = viewport.transform().map(deviceRegion & renderTarget.transformedRect(), renderTarget.transformedSize()) & offsetClip;

    ShaderTraits lastTraits;
    GLShader *shader = nullptr;
    for (int i = 0; i < renderContext.renderNodes.count(); i++) {
        const RenderNode &renderNode = renderContext.renderNodes[i];

        ShaderTraits traits = renderNode.traits;
        if (renderNode.opacity != 1.0 || data.brightness() != 1.0) {
            traits |= ShaderTrait::Modulate;
        }
        if (data.saturation() != 1.0) {
            traits |= ShaderTrait::AdjustSaturation;
        }
        if (data.brightness() != 1.0 || data.saturation() != 1.0) {
            // make sure that brightness and saturation adjustments are always applied in linear space
            traits |= ShaderTrait::TransformColorspace;
        } else {
            const auto colorTransformation = ColorPipeline::create(renderNode.colorDescription, renderTarget.colorDescription(), renderNode.renderingIntent,
                                                                   renderNode.hasFloatingPointColor ? ColorPipeline::InputType::FloatingPoint : ColorPipeline::InputType::FixedPoint);
            if (!colorTransformation.isIdentity()) {
                traits |= ShaderTrait::TransformColorspace;
            }
        }

        if (renderNode.paintHole) {
            traits = (traits & ShaderTrait::RoundedCorners) | ShaderTrait::UniformColor;
            glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            setBlendEnabled(true);
        } else {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            setBlendEnabled(renderNode.hasAlpha || renderNode.opacity < 1.0);
        }

        if (!shader || traits != lastTraits) {
            lastTraits = traits;
            if (shader) {
                ShaderManager::instance()->popShader();
            }
            shader = ShaderManager::instance()->pushShader(traits);
            if (traits & ShaderTrait::AdjustSaturation) {
                const auto toXYZ = renderTarget.colorDescription()->containerColorimetry().toXYZ();
                shader->setUniform(GLShader::FloatUniform::Saturation, data.saturation());
                shader->setUniform(GLShader::Vec3Uniform::PrimaryBrightness, QVector3D(toXYZ(1, 0), toXYZ(1, 1), toXYZ(1, 2)));
            }

            if (traits & ShaderTrait::MapTexture) {
                shader->setUniform(GLShader::IntUniform::Sampler, 0);
            } else if (traits & ShaderTrait::MapMultiPlaneTexture) {
                shader->setUniform(GLShader::IntUniform::Sampler, 0);
                shader->setUniform(GLShader::IntUniform::Sampler1, 1);
            }
        }
        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, renderContext.projectionMatrix * renderNode.transformMatrix);
        if (traits & ShaderTrait::Modulate) {
            shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, modulate(renderNode.opacity, data.brightness()));
        }
        if (traits & ShaderTrait::TransformColorspace) {
            shader->setColorspaceUniforms(renderNode.colorDescription, renderTarget.colorDescription(), renderNode.renderingIntent);
        }
        if (traits & ShaderTrait::YuvConversion) {
            shader->setUniform(GLShader::Mat4Uniform::YuvToRgb, renderNode.colorDescription->yuvMatrix());
        }
        if (traits & ShaderTrait::RoundedCorners) {
            shader->setUniform(GLShader::Vec4Uniform::Box, renderNode.box);
            shader->setUniform(GLShader::Vec4Uniform::CornerRadius, renderNode.borderRadius);
        }
        if (traits & ShaderTrait::Border) {
            shader->setUniform(GLShader::Vec4Uniform::Box, renderNode.box);
            shader->setUniform(GLShader::Vec4Uniform::CornerRadius, renderNode.borderRadius);
            shader->setUniform(GLShader::IntUniform::Thickness, renderNode.borderThickness);
            shader->setUniform(GLShader::ColorUniform::Color, renderNode.borderColor);
        }
        if (renderNode.paintHole) {
            shader->setUniform(GLShader::ColorUniform::Color, QColor(0, 0, 0, 255));
        }

        for (int i = 0; i < renderNode.textures.count() && !renderNode.paintHole; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            renderNode.textures[i]->bind();
        }

        const Rect renderTargetClip = viewport.transform().map(renderNode.clipRect, renderTarget.transformedSize());
        const Region clippedScissor = scissorRegion & renderTargetClip;
        vbo->draw(clippedScissor, GL_TRIANGLES, renderNode.firstVertex,
                  renderNode.vertexCount, true);

        for (int i = 0; i < renderNode.textures.count() && !renderNode.paintHole; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            renderNode.textures[i]->unbind();
        }

        if (renderNode.bufferReleasePoint) {
            m_releasePoints.insert(renderNode.bufferReleasePoint);
        }

        if (renderNode.layerDebugBox.has_value()) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            setBlendEnabled(true);
            if (shader) {
                ShaderManager::instance()->popShader();
            }
            lastTraits = ShaderTrait::Border;
            shader = ShaderManager::instance()->pushShader(lastTraits);
            shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, renderContext.projectionMatrix * renderNode.transformMatrix);

            const RectF box = renderNode.layerDebugBox->scaled(viewport.scale()).adjusted(10, 10, -10, -10);
            shader->setUniform(GLShader::Vec4Uniform::Box, QVector4D(box.horizontalCenter(), box.verticalCenter(), box.width() * 0.5, box.height() * 0.5));
            shader->setUniform(GLShader::Vec4Uniform::CornerRadius, QVector4D(0, 0, 0, 0));
            shader->setUniform(GLShader::IntUniform::Thickness, 10);
            if (renderNode.paintHole) {
                shader->setUniform(GLShader::ColorUniform::Color, QColor(0, 255, 0, 50));
            } else {
                shader->setUniform(GLShader::ColorUniform::Color, QColor(255, 0, 0, 50));
            }
            vbo->draw(clippedScissor, GL_TRIANGLES, renderNode.firstVertex,
                      renderNode.vertexCount, true);
        }
    }
    if (shader) {
        // some other code assumes texture 0 is active
        glActiveTexture(GL_TEXTURE0);
        ShaderManager::instance()->popShader();
    }

    if (m_debug.fractionalEnabled) {
        visualizeFractional(viewport, scissorRegion, renderContext);
    }

    vbo->unbindArrays();

    setBlendEnabled(false);

    glDisable(GL_SCISSOR_TEST);
    return true;
}

void ItemRendererOpenGL::visualizeFractional(const RenderViewport &viewport, const Region &logicalRegion, const RenderContext &renderContext)
{
    if (!m_debug.fractionalShader) {
        m_debug.fractionalShader = ShaderManager::instance()->generateShaderFromFile(
            ShaderTrait::MapTexture,
            QStringLiteral(":/scene/opengl/shaders/debug_fractional.vert"),
            QStringLiteral(":/scene/opengl/shaders/debug_fractional.frag"));
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

        setBlendEnabled(true);

        QVector2D size;
        if (!renderNode.textures.isEmpty()) {
            size = QVector2D(renderNode.textures[0]->width(), renderNode.textures[0]->height());
        }

        m_debug.fractionalShader->setUniform("geometrySize", size);
        m_debug.fractionalShader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, renderContext.projectionMatrix * renderNode.transformMatrix);

        vbo->draw(logicalRegion, GL_TRIANGLES, renderNode.firstVertex,
                  renderNode.vertexCount, true);
    }
}

void ItemRendererOpenGL::setLayerDebugging(bool enable)
{
    m_debug.layerEnabled = enable;
}

} // namespace KWin
