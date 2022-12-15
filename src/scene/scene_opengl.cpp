/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    Based on glcompmgr code by Felix Bellaby.
    Using code from Compiz and Beryl.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_opengl.h"
#include "openglsurfacetexture.h"

#include <kwinglplatform.h>
#include <kwinoffscreenquickview.h>

#include "composite.h"
#include "core/output.h"
#include "decorations/decoratedclient.h"
#include "effects.h"
#include "main.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "utils/common.h"
#include "window.h"

#include <cmath>
#include <cstddef>

#include <QMatrix4x4>
#include <QPainter>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>
#include <QtMath>

namespace KWin
{

/************************************************
 * SceneOpenGL
 ***********************************************/

SceneOpenGL::SceneOpenGL(OpenGLBackend *backend)
    : m_backend(backend)
{
    // It is not legal to not have a vertex array object bound in a core context
    if (!GLPlatform::instance()->isGLES() && hasGLExtension(QByteArrayLiteral("GL_ARB_vertex_array_object"))) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
    }
}

SceneOpenGL::~SceneOpenGL()
{
    makeOpenGLContextCurrent();
}

void SceneOpenGL::paint(RenderTarget *renderTarget, const QRegion &region)
{
    GLVertexBuffer::streamingBuffer()->beginFrame();
    paintScreen(region);
    GLVertexBuffer::streamingBuffer()->endOfFrame();
}

void SceneOpenGL::paintBackground(const QRegion &region)
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

void SceneOpenGL::paintOffscreenQuickView(OffscreenQuickView *w)
{
    GLTexture *t = w->bufferAsTexture();
    if (!t) {
        return;
    }

    ShaderTraits traits = ShaderTrait::MapTexture;
    const qreal a = w->opacity();
    if (a != 1.0) {
        traits |= ShaderTrait::Modulate;
    }

    GLShader *shader = ShaderManager::instance()->pushShader(traits);
    const QRectF rect = scaledRect(w->geometry(), renderTargetScale());

    QMatrix4x4 mvp(renderTargetProjectionMatrix());
    mvp.translate(rect.x(), rect.y());
    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

    if (a != 1.0) {
        shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    t->bind();
    t->render(w->geometry(), renderTargetScale());
    t->unbind();
    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}

bool SceneOpenGL::makeOpenGLContextCurrent()
{
    return m_backend->makeCurrent();
}

void SceneOpenGL::doneOpenGLContextCurrent()
{
    m_backend->doneCurrent();
}

bool SceneOpenGL::supportsNativeFence() const
{
    return m_backend->supportsNativeFence();
}

Shadow *SceneOpenGL::createShadow(Window *window)
{
    return new SceneOpenGLShadow(window);
}

DecorationRenderer *SceneOpenGL::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
{
    return new SceneOpenGLDecorationRenderer(impl);
}

bool SceneOpenGL::animationsSupported() const
{
    return !GLPlatform::instance()->isSoftwareEmulation();
}

QVector<QByteArray> SceneOpenGL::openGLPlatformInterfaceExtensions() const
{
    return m_backend->extensions().toVector();
}

std::shared_ptr<GLTexture> SceneOpenGL::textureForOutput(Output *output) const
{
    return m_backend->textureForOutput(output);
}

std::unique_ptr<SurfaceTexture> SceneOpenGL::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return m_backend->createSurfaceTextureInternal(pixmap);
}

std::unique_ptr<SurfaceTexture> SceneOpenGL::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return m_backend->createSurfaceTextureWayland(pixmap);
}

std::unique_ptr<SurfaceTexture> SceneOpenGL::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return m_backend->createSurfaceTextureX11(pixmap);
}

void SceneOpenGL::doPaintBackground(const QVector<float> &vertices)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setColor(QColor(0, 0, 0, 0));
    vbo->setData(vertices.count() / 2, 2, vertices.data(), nullptr);

    ShaderBinder binder(ShaderTrait::UniformColor);
    binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, renderTargetProjectionMatrix());

    vbo->render(GL_TRIANGLES);
}

QVector4D SceneOpenGL::modulate(float opacity, float brightness) const
{
    const float a = opacity;
    const float rgb = opacity * brightness;

    return QVector4D(rgb, rgb, rgb, a);
}

void SceneOpenGL::setBlendEnabled(bool enabled)
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

static RenderGeometry clipQuads(const Item *item, const SceneOpenGL::RenderContext *context)
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

void SceneOpenGL::createRenderNode(Item *item, RenderContext *context)
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

QMatrix4x4 SceneOpenGL::modelViewProjectionMatrix(const WindowPaintData &data) const
{
    // An effect may want to override the default projection matrix in some cases,
    // such as when it is rendering a window on a render target that doesn't have
    // the same dimensions as the default framebuffer.
    //
    // Note that the screen transformation is not applied here.
    const QMatrix4x4 pMatrix = data.projectionMatrix();
    if (!pMatrix.isIdentity()) {
        return pMatrix;
    } else {
        return renderTargetProjectionMatrix();
    }
}

void SceneOpenGL::render(Item *item, int mask, const QRegion &region, const WindowPaintData &data)
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

    item->setTransform(data.toMatrix(renderTargetScale()));

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

    const GLVertexAttrib attribs[] = {
        {VA_Position, 2, GL_FLOAT, offsetof(GLVertex2D, position)},
        {VA_TexCoord, 2, GL_FLOAT, offsetof(GLVertex2D, texcoord)},
    };

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(attribs, 2, sizeof(GLVertex2D));

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

        const QMatrix4x4 textureMatrix = renderNode.texture->matrix(renderNode.coordinateType);
        if (!textureMatrix.isIdentity()) {
            // Adjust the vertex' texture coordinates with the specified matrix.
            const QVector2D coeff(textureMatrix(0, 0), textureMatrix(1, 1));
            const QVector2D offset(textureMatrix(0, 3), textureMatrix(1, 3));

            for (auto &vertex : renderNode.geometry) {
                vertex.texcoord = vertex.texcoord * coeff + offset;
            }
        }

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

    const QMatrix4x4 projectionMatrix = modelViewProjectionMatrix(data);
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

//****************************************
// SceneOpenGL::Shadow
//****************************************
class DecorationShadowTextureCache
{
public:
    ~DecorationShadowTextureCache();
    DecorationShadowTextureCache(const DecorationShadowTextureCache &) = delete;
    static DecorationShadowTextureCache &instance();

    void unregister(SceneOpenGLShadow *shadow);
    std::shared_ptr<GLTexture> getTexture(SceneOpenGLShadow *shadow);

private:
    DecorationShadowTextureCache() = default;
    struct Data
    {
        std::shared_ptr<GLTexture> texture;
        QVector<SceneOpenGLShadow *> shadows;
    };
    QHash<KDecoration2::DecorationShadow *, Data> m_cache;
};

DecorationShadowTextureCache &DecorationShadowTextureCache::instance()
{
    static DecorationShadowTextureCache s_instance;
    return s_instance;
}

DecorationShadowTextureCache::~DecorationShadowTextureCache()
{
    Q_ASSERT(m_cache.isEmpty());
}

void DecorationShadowTextureCache::unregister(SceneOpenGLShadow *shadow)
{
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        auto &d = it.value();
        // check whether the Vector of Shadows contains our shadow and remove all of them
        auto glIt = d.shadows.begin();
        while (glIt != d.shadows.end()) {
            if (*glIt == shadow) {
                glIt = d.shadows.erase(glIt);
            } else {
                glIt++;
            }
        }
        // if there are no shadows any more we can erase the cache entry
        if (d.shadows.isEmpty()) {
            it = m_cache.erase(it);
        } else {
            it++;
        }
    }
}

std::shared_ptr<GLTexture> DecorationShadowTextureCache::getTexture(SceneOpenGLShadow *shadow)
{
    Q_ASSERT(shadow->hasDecorationShadow());
    unregister(shadow);
    const auto &decoShadow = shadow->decorationShadow().toStrongRef();
    Q_ASSERT(!decoShadow.isNull());
    auto it = m_cache.find(decoShadow.data());
    if (it != m_cache.end()) {
        Q_ASSERT(!it.value().shadows.contains(shadow));
        it.value().shadows << shadow;
        return it.value().texture;
    }
    Data d;
    d.shadows << shadow;
    d.texture = std::make_shared<GLTexture>(shadow->decorationShadowImage());
    m_cache.insert(decoShadow.data(), d);
    return d.texture;
}

SceneOpenGLShadow::SceneOpenGLShadow(Window *window)
    : Shadow(window)
{
}

SceneOpenGLShadow::~SceneOpenGLShadow()
{
    Scene *scene = Compositor::self()->scene();
    if (scene) {
        scene->makeOpenGLContextCurrent();
        DecorationShadowTextureCache::instance().unregister(this);
        m_texture.reset();
    }
}

bool SceneOpenGLShadow::prepareBackend()
{
    if (hasDecorationShadow()) {
        // simplifies a lot by going directly to
        Scene *scene = Compositor::self()->scene();
        scene->makeOpenGLContextCurrent();
        m_texture = DecorationShadowTextureCache::instance().getTexture(this);

        return true;
    }
    const QSize top(shadowPixmap(ShadowElementTop).size());
    const QSize topRight(shadowPixmap(ShadowElementTopRight).size());
    const QSize right(shadowPixmap(ShadowElementRight).size());
    const QSize bottom(shadowPixmap(ShadowElementBottom).size());
    const QSize bottomLeft(shadowPixmap(ShadowElementBottomLeft).size());
    const QSize left(shadowPixmap(ShadowElementLeft).size());
    const QSize topLeft(shadowPixmap(ShadowElementTopLeft).size());
    const QSize bottomRight(shadowPixmap(ShadowElementBottomRight).size());

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()}) + std::max(top.width(), bottom.width()) + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()}) + std::max(left.height(), right.height()) + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const int innerRectTop = std::max({topLeft.height(), top.height(), topRight.height()});
    const int innerRectLeft = std::max({topLeft.width(), left.width(), bottomLeft.width()});

    QPainter p;
    p.begin(&image);

    p.drawPixmap(0, 0, topLeft.width(), topLeft.height(), shadowPixmap(ShadowElementTopLeft));
    p.drawPixmap(innerRectLeft, 0, top.width(), top.height(), shadowPixmap(ShadowElementTop));
    p.drawPixmap(width - topRight.width(), 0, topRight.width(), topRight.height(), shadowPixmap(ShadowElementTopRight));

    p.drawPixmap(0, innerRectTop, left.width(), left.height(), shadowPixmap(ShadowElementLeft));
    p.drawPixmap(width - right.width(), innerRectTop, right.width(), right.height(), shadowPixmap(ShadowElementRight));

    p.drawPixmap(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height(), shadowPixmap(ShadowElementBottomLeft));
    p.drawPixmap(innerRectLeft, height - bottom.height(), bottom.width(), bottom.height(), shadowPixmap(ShadowElementBottom));
    p.drawPixmap(width - bottomRight.width(), height - bottomRight.height(), bottomRight.width(), bottomRight.height(), shadowPixmap(ShadowElementBottomRight));

    p.end();

    // Check if the image is alpha-only in practice, and if so convert it to an 8-bpp format
    if (!GLPlatform::instance()->isGLES() && GLTexture::supportsSwizzle() && GLTexture::supportsFormatRG()) {
        QImage alphaImage(image.size(), QImage::Format_Alpha8);
        bool alphaOnly = true;

        for (ptrdiff_t y = 0; alphaOnly && y < image.height(); y++) {
            const uint32_t *const src = reinterpret_cast<const uint32_t *>(image.scanLine(y));
            uint8_t *const dst = reinterpret_cast<uint8_t *>(alphaImage.scanLine(y));

            for (ptrdiff_t x = 0; x < image.width(); x++) {
                if (src[x] & 0x00ffffff) {
                    alphaOnly = false;
                }

                dst[x] = qAlpha(src[x]);
            }
        }

        if (alphaOnly) {
            image = alphaImage;
        }
    }

    Scene *scene = Compositor::self()->scene();
    scene->makeOpenGLContextCurrent();
    m_texture = std::make_shared<GLTexture>(image);

    if (m_texture->internalFormat() == GL_R8) {
        // Swizzle red to alpha and all other channels to zero
        m_texture->bind();
        m_texture->setSwizzle(GL_ZERO, GL_ZERO, GL_ZERO, GL_RED);
    }

    return true;
}

SceneOpenGLDecorationRenderer::SceneOpenGLDecorationRenderer(Decoration::DecoratedClientImpl *client)
    : DecorationRenderer(client)
    , m_texture()
{
}

SceneOpenGLDecorationRenderer::~SceneOpenGLDecorationRenderer()
{
    if (Scene *scene = Compositor::self()->scene()) {
        scene->makeOpenGLContextCurrent();
    }
}

static void clamp_row(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::copy(src, src + width, dest + left);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp_sides(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp(QImage &image, const QRect &viewport)
{
    Q_ASSERT(image.depth() == 32);
    if (viewport.isEmpty()) {
        image = {};
        return;
    }

    const QRect rect = image.rect();

    const int left = viewport.left() - rect.left();
    const int top = viewport.top() - rect.top();
    const int right = rect.right() - viewport.right();
    const int bottom = rect.bottom() - viewport.bottom();

    const int width = rect.width() - left - right;
    const int height = rect.height() - top - bottom;

    const uint32_t *firstRow = reinterpret_cast<uint32_t *>(image.scanLine(top));
    const uint32_t *lastRow = reinterpret_cast<uint32_t *>(image.scanLine(top + height - 1));

    for (int i = 0; i < top; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(i));
        clamp_row(left, width, right, firstRow + left, dest);
    }

    for (int i = 0; i < height; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + i));
        clamp_sides(left, width, right, dest + left, dest);
    }

    for (int i = 0; i < bottom; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + height + i));
        clamp_row(left, width, right, lastRow + left, dest);
    }
}

void SceneOpenGLDecorationRenderer::render(const QRegion &region)
{
    if (areImageSizesDirty()) {
        resizeTexture();
        resetImageSizesDirty();
    }

    if (!m_texture) {
        // for invalid sizes we get no texture, see BUG 361551
        return;
    }

    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);

    const qreal devicePixelRatio = effectiveDevicePixelRatio();
    const int topHeight = std::ceil(top.height() * devicePixelRatio);
    const int bottomHeight = std::ceil(bottom.height() * devicePixelRatio);
    const int leftWidth = std::ceil(left.width() * devicePixelRatio);

    const QPoint topPosition(0, 0);
    const QPoint bottomPosition(0, topPosition.y() + topHeight + (2 * TexturePad));
    const QPoint leftPosition(0, bottomPosition.y() + bottomHeight + (2 * TexturePad));
    const QPoint rightPosition(0, leftPosition.y() + leftWidth + (2 * TexturePad));

    const QRect dirtyRect = region.boundingRect();

    renderPart(top.toRect().intersected(dirtyRect), top.toRect(), topPosition, devicePixelRatio);
    renderPart(bottom.toRect().intersected(dirtyRect), bottom.toRect(), bottomPosition, devicePixelRatio);
    renderPart(left.toRect().intersected(dirtyRect), left.toRect(), leftPosition, devicePixelRatio, true);
    renderPart(right.toRect().intersected(dirtyRect), right.toRect(), rightPosition, devicePixelRatio, true);
}

void SceneOpenGLDecorationRenderer::renderPart(const QRect &rect, const QRect &partRect,
                                               const QPoint &textureOffset,
                                               qreal devicePixelRatio, bool rotated)
{
    if (!rect.isValid()) {
        return;
    }
    // We allow partial decoration updates and it might just so happen that the
    // dirty region is completely contained inside the decoration part, i.e.
    // the dirty region doesn't touch any of the decoration's edges. In that
    // case, we should **not** pad the dirty region.
    const QMargins padding = texturePadForPart(rect, partRect);
    int verticalPadding = padding.top() + padding.bottom();
    int horizontalPadding = padding.left() + padding.right();

    QSize imageSize(toNativeSize(rect.width()), toNativeSize(rect.height()));
    if (rotated) {
        imageSize = QSize(imageSize.height(), imageSize.width());
    }
    QSize paddedImageSize = imageSize;
    paddedImageSize.rheight() += verticalPadding;
    paddedImageSize.rwidth() += horizontalPadding;
    QImage image(paddedImageSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(devicePixelRatio);
    image.fill(Qt::transparent);

    QRect padClip = QRect(padding.left(), padding.top(), imageSize.width(), imageSize.height());
    QPainter painter(&image);
    const qreal inverseScale = 1.0 / devicePixelRatio;
    painter.scale(inverseScale, inverseScale);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(padClip);
    painter.translate(padding.left(), padding.top());
    if (rotated) {
        painter.translate(0, imageSize.height());
        painter.rotate(-90);
    }
    painter.scale(devicePixelRatio, devicePixelRatio);
    painter.translate(-rect.topLeft());
    renderToPainter(&painter, rect);
    painter.end();

    // fill padding pixels by copying from the neighbour row
    clamp(image, padClip);

    QPoint dirtyOffset = (rect.topLeft() - partRect.topLeft()) * devicePixelRatio;
    if (padding.top() == 0) {
        dirtyOffset.ry() += TexturePad;
    }
    if (padding.left() == 0) {
        dirtyOffset.rx() += TexturePad;
    }
    m_texture->update(image, textureOffset + dirtyOffset);
}

const QMargins SceneOpenGLDecorationRenderer::texturePadForPart(
    const QRect &rect, const QRect &partRect)
{
    QMargins result = QMargins(0, 0, 0, 0);
    if (rect.top() == partRect.top()) {
        result.setTop(TexturePad);
    }
    if (rect.bottom() == partRect.bottom()) {
        result.setBottom(TexturePad);
    }
    if (rect.left() == partRect.left()) {
        result.setLeft(TexturePad);
    }
    if (rect.right() == partRect.right()) {
        result.setRight(TexturePad);
    }
    return result;
}

static int align(int value, int align)
{
    return (value + align - 1) & ~(align - 1);
}

void SceneOpenGLDecorationRenderer::resizeTexture()
{
    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);
    QSize size;

    size.rwidth() = toNativeSize(std::max(std::max(top.width(), bottom.width()),
                                          std::max(left.height(), right.height())));
    size.rheight() = toNativeSize(top.height()) + toNativeSize(bottom.height()) + toNativeSize(left.width()) + toNativeSize(right.width());

    size.rheight() += 4 * (2 * TexturePad);
    size.rwidth() += 2 * TexturePad;
    size.rwidth() = align(size.width(), 128);

    if (m_texture && m_texture->size() == size) {
        return;
    }

    if (!size.isEmpty()) {
        m_texture.reset(new GLTexture(GL_RGBA8, size.width(), size.height()));
        m_texture->setYInverted(true);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_texture->clear();
    } else {
        m_texture.reset();
    }
}

int SceneOpenGLDecorationRenderer::toNativeSize(int size) const
{
    return std::ceil(size * effectiveDevicePixelRatio());
}

} // namespace
