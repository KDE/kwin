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
    std::unique_ptr<GLTexture> texture;
    std::unique_ptr<GLFramebuffer> fbo;
    bool isDirty = true;
    GLShader *shader = nullptr;
};

class OffscreenEffectPrivate
{
public:
    QHash<EffectWindow *, OffscreenData *> windows;
    QMetaObject::Connection windowDamagedConnection;
    QMetaObject::Connection windowDeletedConnection;

    void paint(EffectWindow *window, GLTexture *texture, const QRegion &region,
               const WindowPaintData &data, const WindowQuadList &quads, GLShader *offscreenShader);

    GLTexture *maybeRender(EffectWindow *window, OffscreenData *offscreenData);
    bool live = true;
};

OffscreenEffect::OffscreenEffect(QObject *parent)
    : Effect(parent)
    , d(new OffscreenEffectPrivate)
{
}

OffscreenEffect::~OffscreenEffect()
{
    qDeleteAll(d->windows);
}

bool OffscreenEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void OffscreenEffect::setLive(bool live)
{
    Q_ASSERT(d->windows.isEmpty());
    d->live = live;
}

void OffscreenEffect::redirect(EffectWindow *window)
{
    OffscreenData *&offscreenData = d->windows[window];
    if (offscreenData) {
        return;
    }
    offscreenData = new OffscreenData;

    if (d->windows.count() == 1) {
        setupConnections();
    }

    if (!d->live) {
        effects->makeOpenGLContextCurrent();
        d->maybeRender(window, offscreenData);
    }
}

void OffscreenEffect::unredirect(EffectWindow *window)
{
    delete d->windows.take(window);
    if (d->windows.isEmpty()) {
        destroyConnections();
    }
}

void OffscreenEffect::apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    Q_UNUSED(window)
    Q_UNUSED(mask)
    Q_UNUSED(data)
    Q_UNUSED(quads)
}

GLTexture *OffscreenEffectPrivate::maybeRender(EffectWindow *window, OffscreenData *offscreenData)
{
    const QRect geometry = window->expandedGeometry().toAlignedRect();
    QSize textureSize = geometry.size();

    if (const EffectScreen *screen = window->screen()) {
        textureSize *= screen->devicePixelRatio();
    }

    if (!offscreenData->texture || offscreenData->texture->size() != textureSize) {
        offscreenData->texture.reset(new GLTexture(GL_RGBA8, textureSize));
        offscreenData->texture->setFilter(GL_LINEAR);
        offscreenData->texture->setWrapMode(GL_CLAMP_TO_EDGE);
        offscreenData->fbo.reset(new GLFramebuffer(offscreenData->texture.get()));
        offscreenData->isDirty = true;
    }

    if (offscreenData->isDirty) {
        GLFramebuffer::pushFramebuffer(offscreenData->fbo.get());
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRect(0, 0, geometry.width(), geometry.height()));

        WindowPaintData data;
        data.setXTranslation(-geometry.x());
        data.setYTranslation(-geometry.y());
        data.setOpacity(1.0);
        data.setProjectionMatrix(projectionMatrix);

        const int mask = Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_WINDOW_TRANSLUCENT;
        effects->drawWindow(window, mask, infiniteRegion(), data);

        GLFramebuffer::popFramebuffer();
        offscreenData->isDirty = false;
    }

    return offscreenData->texture.get();
}

void OffscreenEffectPrivate::paint(EffectWindow *window, GLTexture *texture, const QRegion &region,
                                const WindowPaintData &data, const WindowQuadList &quads, GLShader *offscreenShader)
{
    GLShader *shader = offscreenShader ? offscreenShader : ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::AdjustSaturation);
    ShaderBinder binder(shader);

    const bool indexedQuads = GLVertexBuffer::supportsIndexedQuads();
    const GLenum primitiveType = indexedQuads ? GL_QUADS : GL_TRIANGLES;
    const int verticesPerQuad = indexedQuads ? 4 : 6;

    const GLVertexAttrib attribs[] = {
        {VA_Position, 2, GL_FLOAT, offsetof(GLVertex2D, position)},
        {VA_TexCoord, 2, GL_FLOAT, offsetof(GLVertex2D, texcoord)},
    };

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(attribs, 2, sizeof(GLVertex2D));
    const size_t size = verticesPerQuad * quads.count() * sizeof(GLVertex2D);
    GLVertex2D *map = static_cast<GLVertex2D *>(vbo->map(size));

    quads.makeInterleavedArrays(primitiveType, map, texture->matrix(NormalizedCoordinates));
    vbo->unmap();
    vbo->bindArrays();

    const qreal rgb = data.brightness() * data.opacity();
    const qreal a = data.opacity();

    QMatrix4x4 mvp = data.screenProjectionMatrix();
    mvp.translate(window->x(), window->y());

    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp * data.toMatrix());
    shader->setUniform(GLShader::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
    shader->setUniform(GLShader::Saturation, data.saturation());
    shader->setUniform(GLShader::TextureWidth, texture->width());
    shader->setUniform(GLShader::TextureHeight, texture->height());

    const bool clipping = region != infiniteRegion();
    const QRegion clipRegion = clipping ? effects->mapToRenderTarget(region) : infiniteRegion();

    if (clipping) {
        glEnable(GL_SCISSOR_TEST);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    texture->bind();
    vbo->draw(clipRegion, primitiveType, 0, verticesPerQuad * quads.count(), clipping);
    texture->unbind();

    glDisable(GL_BLEND);
    if (clipping) {
        glDisable(GL_SCISSOR_TEST);
    }
    vbo->unbindArrays();
}

void OffscreenEffect::drawWindow(EffectWindow *window, int mask, const QRegion &region, WindowPaintData &data)
{
    OffscreenData *offscreenData = d->windows.value(window);
    if (!offscreenData) {
        effects->drawWindow(window, mask, region, data);
        return;
    }

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

    GLTexture *texture = d->maybeRender(window, offscreenData);
    d->paint(window, texture, region, data, quads, offscreenData->shader);
}

void OffscreenEffect::handleWindowDamaged(EffectWindow *window)
{
    OffscreenData *offscreenData = d->windows.value(window);
    if (offscreenData) {
        offscreenData->isDirty = true;
    }
}

void OffscreenEffect::handleWindowDeleted(EffectWindow *window)
{
    unredirect(window);
}

void OffscreenEffect::setupConnections()
{
    if (d->live) {
        d->windowDamagedConnection =
            connect(effects, &EffectsHandler::windowDamaged, this, &OffscreenEffect::handleWindowDamaged);
    }
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

void OffscreenEffect::setShader(EffectWindow *window, GLShader *shader)
{
    OffscreenData *offscreenData = d->windows.value(window);
    if (offscreenData) {
        offscreenData->shader = shader;
    }
}

} // namespace KWin
