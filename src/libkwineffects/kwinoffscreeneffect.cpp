/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinoffscreeneffect.h"
#include "kwingltexture.h"
#include "kwinglutils.h"

namespace KWin
{

struct OffscreenData
{
public:
    virtual ~OffscreenData();
    void setDirty();
    void setShader(GLShader *newShader);
    void setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode);

    void paint(EffectWindow *window, const QRegion &region,
               const WindowPaintData &data, const WindowQuadList &quads);

    void maybeRender(EffectWindow *window);

private:
    std::unique_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_fbo;
    bool m_isDirty = true;
    GLShader *m_shader = nullptr;
    RenderGeometry::VertexSnappingMode m_vertexSnappingMode = RenderGeometry::VertexSnappingMode::Round;
};

class OffscreenEffectPrivate
{
public:
    std::map<EffectWindow *, std::unique_ptr<OffscreenData>> windows;
    QMetaObject::Connection windowDamagedConnection;
    QMetaObject::Connection windowDeletedConnection;
    RenderGeometry::VertexSnappingMode vertexSnappingMode = RenderGeometry::VertexSnappingMode::Round;
};

OffscreenEffect::OffscreenEffect(QObject *parent)
    : Effect(parent)
    , d(std::make_unique<OffscreenEffectPrivate>())
{
}

OffscreenEffect::~OffscreenEffect() = default;

bool OffscreenEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void OffscreenEffect::redirect(EffectWindow *window)
{
    std::unique_ptr<OffscreenData> &offscreenData = d->windows[window];
    if (offscreenData) {
        return;
    }
    offscreenData = std::make_unique<OffscreenData>();
    offscreenData->setVertexSnappingMode(d->vertexSnappingMode);

    if (d->windows.size() == 1) {
        setupConnections();
    }
}

void OffscreenEffect::unredirect(EffectWindow *window)
{
    d->windows.erase(window);
    if (d->windows.empty()) {
        destroyConnections();
    }
}

void OffscreenEffect::setShader(EffectWindow *window, GLShader *shader)
{
    if (const auto it = d->windows.find(window); it != d->windows.end()) {
        it->second->setShader(shader);
    }
}

void OffscreenEffect::apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads)
{
}

void OffscreenData::maybeRender(EffectWindow *window)
{
    QRectF logicalGeometry = window->expandedGeometry();
    QRectF deviceGeometry = scaledRect(logicalGeometry, effects->renderTargetScale());

    QSize textureSize = deviceGeometry.toAlignedRect().size();

    if (!m_texture || m_texture->size() != textureSize) {
        m_texture.reset(new GLTexture(GL_RGBA8, textureSize));
        m_texture->setFilter(GL_LINEAR);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_fbo.reset(new GLFramebuffer(m_texture.get()));
        m_isDirty = true;
    }

    if (m_isDirty) {
        GLFramebuffer::pushFramebuffer(m_fbo.get());
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRectF(0, 0, deviceGeometry.width(), deviceGeometry.height()));

        WindowPaintData data;
        data.setXTranslation(-logicalGeometry.x());
        data.setYTranslation(-logicalGeometry.y());
        data.setOpacity(1.0);
        data.setProjectionMatrix(projectionMatrix);

        const int mask = Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_WINDOW_TRANSLUCENT;
        effects->drawWindow(window, mask, infiniteRegion(), data);

        GLFramebuffer::popFramebuffer();
        m_isDirty = false;
    }
}

OffscreenData::~OffscreenData()
{
}

void OffscreenData::setDirty()
{
    m_isDirty = true;
}

void OffscreenData::setShader(GLShader *newShader)
{
    m_shader = newShader;
}

void OffscreenData::setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode)
{
    m_vertexSnappingMode = mode;
}

void OffscreenData::paint(EffectWindow *window, const QRegion &region,
                          const WindowPaintData &data, const WindowQuadList &quads)
{
    GLShader *shader = m_shader ? m_shader : ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::AdjustSaturation);
    ShaderBinder binder(shader);

    const qreal scale = effects->renderTargetScale();

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(GLVertexBuffer::GLVertex2DLayout, 2, sizeof(GLVertex2D));

    RenderGeometry geometry;
    geometry.setVertexSnappingMode(m_vertexSnappingMode);
    for (auto &quad : quads) {
        geometry.appendWindowQuad(quad, scale);
    }
    geometry.postProcessTextureCoordinates(m_texture->matrix(NormalizedCoordinates));

    GLVertex2D *map = static_cast<GLVertex2D *>(vbo->map(geometry.count() * sizeof(GLVertex2D)));
    geometry.copy(std::span(map, geometry.count()));
    vbo->unmap();

    vbo->bindArrays();

    const qreal rgb = data.brightness() * data.opacity();
    const qreal a = data.opacity();

    QMatrix4x4 mvp = data.projectionMatrix();
    mvp.translate(window->x() * scale, window->y() * scale);

    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp * data.toMatrix(effects->renderTargetScale()));
    shader->setUniform(GLShader::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
    shader->setUniform(GLShader::Saturation, data.saturation());
    shader->setUniform(GLShader::TextureWidth, m_texture->width());
    shader->setUniform(GLShader::TextureHeight, m_texture->height());

    const bool clipping = region != infiniteRegion();
    const QRegion clipRegion = clipping ? effects->mapToRenderTarget(region) : infiniteRegion();

    if (clipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_texture->bind();
    vbo->draw(clipRegion, GL_TRIANGLES, 0, geometry.count(), clipping);
    m_texture->unbind();

    glDisable(GL_BLEND);
    if (clipping) {
        glDisable(GL_SCISSOR_TEST);
    }
    vbo->unbindArrays();
}

void OffscreenEffect::drawWindow(EffectWindow *window, int mask, const QRegion &region, WindowPaintData &data)
{
    const auto it = d->windows.find(window);
    if (it == d->windows.end()) {
        effects->drawWindow(window, mask, region, data);
        return;
    }
    OffscreenData *offscreenData = it->second.get();

    const QRectF expandedGeometry = window->expandedGeometry();
    const QRectF frameGeometry = window->frameGeometry();

    QRectF visibleRect = expandedGeometry;
    visibleRect.moveTopLeft(expandedGeometry.topLeft() - frameGeometry.topLeft());
    WindowQuad quad;
    quad[0] = WindowVertex(visibleRect.topLeft(), QPointF(0, 0));
    quad[1] = WindowVertex(visibleRect.topRight(), QPointF(1, 0));
    quad[2] = WindowVertex(visibleRect.bottomRight(), QPointF(1, 1));
    quad[3] = WindowVertex(visibleRect.bottomLeft(), QPointF(0, 1));

    WindowQuadList quads;
    quads.append(quad);
    apply(window, mask, data, quads);

    offscreenData->maybeRender(window);
    offscreenData->paint(window, region, data, quads);
}

void OffscreenEffect::handleWindowDamaged(EffectWindow *window)
{
    if (const auto it = d->windows.find(window); it != d->windows.end()) {
        it->second->setDirty();
    }
}

void OffscreenEffect::handleWindowDeleted(EffectWindow *window)
{
    effects->makeOpenGLContextCurrent();
    unredirect(window);
}

void OffscreenEffect::setupConnections()
{
    d->windowDamagedConnection =
        connect(effects, &EffectsHandler::windowDamaged, this, &OffscreenEffect::handleWindowDamaged);

    d->windowDeletedConnection =
        connect(effects, &EffectsHandler::windowDeleted, this, &OffscreenEffect::handleWindowDeleted);
}

void OffscreenEffect::destroyConnections()
{
    disconnect(d->windowDamagedConnection);
    disconnect(d->windowDeletedConnection);

    d->windowDamagedConnection = {};
    d->windowDeletedConnection = {};
}

void OffscreenEffect::setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode)
{
    d->vertexSnappingMode = mode;
    for (auto &window : std::as_const(d->windows)) {
        window.second->setVertexSnappingMode(mode);
    }
}

class CrossFadeWindowData : public OffscreenData
{
public:
    QRectF frameGeometryAtCapture;
};

class CrossFadeEffectPrivate
{
public:
    std::map<EffectWindow *, std::unique_ptr<CrossFadeWindowData>> windows;
    qreal progress;
};

CrossFadeEffect::CrossFadeEffect(QObject *parent)
    : Effect(parent)
    , d(std::make_unique<CrossFadeEffectPrivate>())
{
}

CrossFadeEffect::~CrossFadeEffect() = default;

void CrossFadeEffect::drawWindow(EffectWindow *window, int mask, const QRegion &region, WindowPaintData &data)
{
    const auto it = d->windows.find(window);

    // paint the new window (if applicable) underneath
    if (data.crossFadeProgress() > 0 || it == d->windows.end()) {
        Effect::drawWindow(window, mask, region, data);
    }

    if (it == d->windows.end()) {
        return;
    }
    CrossFadeWindowData *offscreenData = it->second.get();

    // paint old snapshot on top
    WindowPaintData previousWindowData = data;
    previousWindowData.setOpacity((1.0 - data.crossFadeProgress()) * data.opacity());

    const QRectF expandedGeometry = window->expandedGeometry();
    const QRectF frameGeometry = window->frameGeometry();

    // This is for the case of *non* live effect, when the window buffer we saved has a different size
    // compared to the size the window has now. The "old" window will be rendered scaled to the current
    // window geometry, but everything will be scaled, also the shadow if there is any, making the window
    // frame not line up anymore with window->frameGeometry()
    // to fix that, we consider how much the shadow will have scaled, and use that as margins to the
    // current frame geometry. this causes the scaled window to visually line up perfectly with frameGeometry,
    // having the scaled shadow all outside of it.
    const qreal widthRatio = offscreenData->frameGeometryAtCapture.width() / frameGeometry.width();
    const qreal heightRatio = offscreenData->frameGeometryAtCapture.height() / frameGeometry.height();

    const QMarginsF margins(
        (expandedGeometry.x() - frameGeometry.x()) / widthRatio,
        (expandedGeometry.y() - frameGeometry.y()) / heightRatio,
        (frameGeometry.right() - expandedGeometry.right()) / widthRatio,
        (frameGeometry.bottom() - expandedGeometry.bottom()) / heightRatio);

    QRectF visibleRect = QRectF(QPointF(0, 0), frameGeometry.size()) - margins;

    WindowQuad quad;
    quad[0] = WindowVertex(visibleRect.topLeft(), QPointF(0, 0));
    quad[1] = WindowVertex(visibleRect.topRight(), QPointF(1, 0));
    quad[2] = WindowVertex(visibleRect.bottomRight(), QPointF(1, 1));
    quad[3] = WindowVertex(visibleRect.bottomLeft(), QPointF(0, 1));

    WindowQuadList quads;
    quads.append(quad);
    offscreenData->paint(window, region, previousWindowData, quads);
}

void CrossFadeEffect::redirect(EffectWindow *window)
{
    if (d->windows.empty()) {
        connect(effects, &EffectsHandler::windowDeleted, this, &CrossFadeEffect::handleWindowDeleted);
    }

    std::unique_ptr<CrossFadeWindowData> &offscreenData = d->windows[window];
    if (offscreenData) {
        return;
    }
    offscreenData = std::make_unique<CrossFadeWindowData>();

    // Avoid including blur and contrast effects. During a normal painting cycle they
    // won't be included, but since we call effects->drawWindow() outside usual compositing
    // cycle, we have to prevent backdrop effects kicking in.
    const QVariant blurRole = window->data(WindowForceBlurRole);
    window->setData(WindowForceBlurRole, QVariant());
    const QVariant contrastRole = window->data(WindowForceBackgroundContrastRole);
    window->setData(WindowForceBackgroundContrastRole, QVariant());

    effects->makeOpenGLContextCurrent();
    offscreenData->maybeRender(window);
    offscreenData->frameGeometryAtCapture = window->frameGeometry();

    window->setData(WindowForceBlurRole, blurRole);
    window->setData(WindowForceBackgroundContrastRole, contrastRole);
}

void CrossFadeEffect::unredirect(EffectWindow *window)
{
    d->windows.erase(window);
    if (d->windows.empty()) {
        disconnect(effects, &EffectsHandler::windowDeleted, this, &CrossFadeEffect::handleWindowDeleted);
    }
}

void CrossFadeEffect::handleWindowDeleted(EffectWindow *window)
{
    unredirect(window);
}

void CrossFadeEffect::setShader(EffectWindow *window, GLShader *shader)
{
    if (const auto it = d->windows.find(window); it != d->windows.end()) {
        it->second->setShader(shader);
    }
}

} // namespace KWin
