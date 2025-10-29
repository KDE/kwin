/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/offscreeneffect.h"
#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/eglcontext.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "scene/windowitem.h"

namespace KWin
{

struct OffscreenData
{
public:
    virtual ~OffscreenData();
    void setDirty();
    void setShader(GLShader *newShader);
    void setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode);

    void paint(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, const QRegion &deviceRegion,
               const WindowPaintData &data, const WindowQuadList &quads);

    void maybeRender(EffectWindow *window);

    std::unique_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_fbo;
    bool m_isDirty = true;
    GLShader *m_shader = nullptr;
    RenderGeometry::VertexSnappingMode m_vertexSnappingMode = RenderGeometry::VertexSnappingMode::Round;
    QMetaObject::Connection m_windowDamagedConnection;
    ItemEffect m_windowEffect;
};

class OffscreenEffectPrivate
{
public:
    std::map<EffectWindow *, std::unique_ptr<OffscreenData>> windows;
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
    offscreenData->m_windowEffect = ItemEffect(window->windowItem());
    offscreenData->m_windowDamagedConnection =
        connect(window, &EffectWindow::windowDamaged, this, &OffscreenEffect::handleWindowDamaged);

    if (d->windows.size() == 1) {
        setupConnections();
    }
}

void OffscreenEffect::unredirect(EffectWindow *window)
{
    auto it = d->windows.find(window);
    if (it == d->windows.end()) {
        return;
    }

    if (!EglContext::currentContext()) {
        effects->openglContext()->makeCurrent();
    }

    d->windows.erase(it);
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
    const qreal scale = window->screen()->scale();
    const QRectF logicalGeometry = snapToPixels(window->expandedGeometry(), scale);
    const QSize textureSize = (logicalGeometry.size() * scale).toSize();

    if (textureSize.isEmpty()) {
        m_fbo.reset();
        m_texture.reset();
        return;
    }
    if (!m_texture || m_texture->size() != textureSize) {
        m_texture = GLTexture::allocate(GL_RGBA8, textureSize);
        if (!m_texture) {
            return;
        }
        m_texture->setFilter(GL_LINEAR);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_fbo = std::make_unique<GLFramebuffer>(m_texture.get());
        m_isDirty = true;
    }

    if (m_isDirty) {
        RenderTarget renderTarget(m_fbo.get());
        RenderViewport viewport(logicalGeometry, scale, renderTarget);
        GLFramebuffer::pushFramebuffer(m_fbo.get());
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        WindowPaintData data;
        data.setOpacity(1.0);

        const int mask = Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_WINDOW_TRANSLUCENT;
        effects->drawWindow(renderTarget, viewport, window, mask, infiniteRegion(), data);

        GLFramebuffer::popFramebuffer();
        m_isDirty = false;
    }
}

OffscreenData::~OffscreenData()
{
    QObject::disconnect(m_windowDamagedConnection);
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

void OffscreenData::paint(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, const QRegion &deviceRegion,
                          const WindowPaintData &data, const WindowQuadList &quads)
{
    if (!m_texture) {
        return;
    }
    GLShader *shader = m_shader ? m_shader : ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::AdjustSaturation | ShaderTrait::TransformColorspace);
    ShaderBinder binder(shader);

    const double scale = viewport.scale();

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    RenderGeometry geometry;
    geometry.setVertexSnappingMode(m_vertexSnappingMode);
    for (auto &quad : quads) {
        geometry.appendWindowQuad(quad, scale);
    }
    geometry.postProcessTextureCoordinates(m_texture->matrix(NormalizedCoordinates));

    const auto map = vbo->map<GLVertex2D>(geometry.size());
    if (!map) {
        return;
    }
    geometry.copy(*map);
    vbo->unmap();

    vbo->bindArrays();

    const qreal rgb = data.brightness() * data.opacity();
    const qreal a = data.opacity();

    QMatrix4x4 mvp = viewport.projectionMatrix();
    mvp.translate(std::round(window->x() * scale), std::round(window->y() * scale));

    const auto toXYZ = renderTarget.colorDescription()->containerColorimetry().toXYZ();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp * data.toMatrix(scale));
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
    shader->setUniform(GLShader::FloatUniform::Saturation, data.saturation());
    shader->setUniform(GLShader::Vec3Uniform::PrimaryBrightness, QVector3D(toXYZ(1, 0), toXYZ(1, 1), toXYZ(1, 2)));
    shader->setUniform(GLShader::IntUniform::TextureWidth, m_texture->width());
    shader->setUniform(GLShader::IntUniform::TextureHeight, m_texture->height());
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);

    const bool clipping = deviceRegion != infiniteRegion();
    const QRegion clipRegion = clipping ? viewport.transform().map(deviceRegion, renderTarget.transformedSize()) : infiniteRegion();

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

void OffscreenEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, const QRegion &deviceRegion, WindowPaintData &data)
{
    const auto it = d->windows.find(window);
    if (it == d->windows.end()) {
        effects->drawWindow(renderTarget, viewport, window, mask, deviceRegion, data);
        return;
    }
    OffscreenData *offscreenData = it->second.get();

    const QRectF expandedGeometry = snapToPixels(window->expandedGeometry(), viewport.scale());
    const QRectF frameGeometry = snapToPixels(window->frameGeometry(), viewport.scale());

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
    offscreenData->paint(renderTarget, viewport, window, deviceRegion, data, quads);
}

void OffscreenEffect::handleWindowDamaged(EffectWindow *window)
{
    if (const auto it = d->windows.find(window); it != d->windows.end()) {
        it->second->setDirty();
    }
}

void OffscreenEffect::handleWindowDeleted(EffectWindow *window)
{
    unredirect(window);
}

void OffscreenEffect::setupConnections()
{
    d->windowDeletedConnection =
        connect(effects, &EffectsHandler::windowDeleted, this, &OffscreenEffect::handleWindowDeleted);
}

void OffscreenEffect::destroyConnections()
{
    disconnect(d->windowDeletedConnection);

    d->windowDeletedConnection = {};
}

void OffscreenEffect::setVertexSnappingMode(RenderGeometry::VertexSnappingMode mode)
{
    d->vertexSnappingMode = mode;
    for (auto &window : std::as_const(d->windows)) {
        window.second->setVertexSnappingMode(mode);
    }
}

bool OffscreenEffect::blocksDirectScanout() const
{
    return false;
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

void CrossFadeEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, const QRegion &deviceRegion, WindowPaintData &data)
{
    const auto it = d->windows.find(window);

    // paint the new window (if applicable) underneath
    if (data.crossFadeProgress() > 0 || it == d->windows.end()) {
        Effect::drawWindow(renderTarget, viewport, window, mask, deviceRegion, data);
    }

    if (it == d->windows.end()) {
        return;
    }
    CrossFadeWindowData *offscreenData = it->second.get();

    // paint old snapshot on top
    WindowPaintData previousWindowData = data;
    previousWindowData.setOpacity((1.0 - data.crossFadeProgress()) * data.opacity());

    const QRectF expandedGeometry = snapToPixels(window->expandedGeometry(), viewport.scale());
    const QRectF frameGeometry = snapToPixels(window->frameGeometry(), viewport.scale());

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
    offscreenData->paint(renderTarget, viewport, window, deviceRegion, previousWindowData, quads);
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
    offscreenData->m_windowEffect = ItemEffect(window->windowItem());

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
    auto it = d->windows.find(window);
    if (it == d->windows.end()) {
        return;
    }

    if (!EglContext::currentContext()) {
        effects->openglContext()->makeCurrent();
    }

    d->windows.erase(it);
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

bool CrossFadeEffect::blocksDirectScanout() const
{
    return false;
}

} // namespace KWin

#include "moc_offscreeneffect.cpp"
