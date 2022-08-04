/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "screentransform.h"
#include "kwinglutils.h"

#include <QDebug>

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(screentransform);
}

namespace KWin
{

ScreenTransformEffect::ScreenTransformEffect()
    : Effect()
{
    // Make sure that shaders in /effects/screentransform/shaders/* are loaded.
    ensureResources();

    m_shader = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture,
        QStringLiteral(":/effects/screentransform/shaders/crossfade.vert"),
        QStringLiteral(":/effects/screentransform/shaders/crossfade.frag"));

    m_modelViewProjectioMatrixLocation = m_shader->uniformLocation("modelViewProjectionMatrix");
    m_blendFactorLocation = m_shader->uniformLocation("blendFactor");
    m_previousTextureLocation = m_shader->uniformLocation("previousTexture");
    m_currentTextureLocation = m_shader->uniformLocation("currentTexture");

    const QList<EffectScreen *> screens = effects->screens();
    for (auto screen : screens) {
        addScreen(screen);
    }
    connect(effects, &EffectsHandler::screenAdded, this, &ScreenTransformEffect::addScreen);
    connect(effects, &EffectsHandler::screenRemoved, this, &ScreenTransformEffect::removeScreen);
}

ScreenTransformEffect::~ScreenTransformEffect() = default;

bool ScreenTransformEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing && effects->waylandDisplay() && effects->animationsSupported();
}

qreal transformAngle(EffectScreen::Transform current, EffectScreen::Transform old)
{
    auto ensureShort = [](int angle) {
        return angle > 180 ? angle - 360 : angle < -180 ? angle + 360
                                                        : angle;
    };
    // % 4 to ignore flipped cases (for now)
    return ensureShort((int(current) % 4 - int(old) % 4) * 90);
}

void ScreenTransformEffect::addScreen(EffectScreen *screen)
{
    connect(screen, &EffectScreen::changed, this, [this, screen] {
        auto &state = m_states[screen];
        if (screen->transform() == state.m_oldTransform) {
            effects->makeOpenGLContextCurrent();
            m_states.remove(screen);
            return;
        }
        state.m_timeLine.setDuration(std::chrono::milliseconds(long(animationTime(250))));
        state.m_timeLine.setEasingCurve(QEasingCurve::InOutCubic);
        state.m_angle = transformAngle(screen->transform(), state.m_oldTransform);
        Q_ASSERT(state.m_angle != 0);
        effects->addRepaintFull();
    });
    connect(screen, &EffectScreen::aboutToChange, this, [this, screen] {
        effects->makeOpenGLContextCurrent();
        auto &state = m_states[screen];
        state.m_oldTransform = screen->transform();
        state.m_oldGeometry = screen->geometry();
        state.m_prev.texture.reset(new GLTexture(GL_RGBA8, screen->geometry().size() * screen->devicePixelRatio()));
        state.m_prev.framebuffer.reset(new GLFramebuffer(state.m_prev.texture.get()));

        // Rendering the current scene into a texture
        GLFramebuffer::pushFramebuffer(state.m_prev.framebuffer.get());
        effects->renderScreen(screen);
        GLFramebuffer::popFramebuffer();

        // Now, the effect can cross-fade between current and previous state.
        state.m_captured = true;
    });
}

void ScreenTransformEffect::removeScreen(EffectScreen *screen)
{
    effects->makeOpenGLContextCurrent();
    m_states.remove(screen);
    effects->doneOpenGLContextCurrent();
}

void ScreenTransformEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto it = m_states.find(data.screen);
    if (it != m_states.end() && it->m_captured) {
        it->m_timeLine.advance(presentTime);
        if (it->m_timeLine.done()) {
            m_states.remove(data.screen);
        }
    }

    effects->prePaintScreen(data, presentTime);
}

static GLVertexBuffer *texturedRectVbo(const QRectF &geometry, qreal scale)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();

    const GLVertexAttrib attribs[] = {
        {VA_Position, 2, GL_FLOAT, offsetof(GLVertex2D, position)},
        {VA_TexCoord, 2, GL_FLOAT, offsetof(GLVertex2D, texcoord)},
    };
    vbo->setAttribLayout(attribs, 2, sizeof(GLVertex2D));

    auto map = static_cast<GLVertex2D *>(vbo->map(6 * sizeof(GLVertex2D)));

    auto deviceGeometry = scaledRect(geometry, scale);

    // first triangle
    map[0] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.top()),
        .texcoord = QVector2D(0.0, 1.0),
    };
    map[1] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.bottom()),
        .texcoord = QVector2D(1.0, 0.0),
    };
    map[2] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.bottom()),
        .texcoord = QVector2D(0.0, 0.0),
    };

    // second triangle
    map[3] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.top()),
        .texcoord = QVector2D(0.0, 1.0),
    };
    map[4] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.top()),
        .texcoord = QVector2D(1.0, 1.0),
    };
    map[5] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.bottom()),
        .texcoord = QVector2D(1.0, 0.0),
    };

    vbo->unmap();
    return vbo;
}

static qreal lerp(qreal a, qreal b, qreal t)
{
    return (1 - t) * a + t * b;
}

static QRectF lerp(const QRectF &a, const QRectF &b, qreal t)
{
    QRectF ret;
    ret.setWidth(lerp(a.width(), b.width(), t));
    ret.setHeight(lerp(a.height(), b.height(), t));
    ret.moveCenter(b.center());
    return ret;
}

void ScreenTransformEffect::paintScreen(int mask, const QRegion &region, KWin::ScreenPaintData &data)
{
    EffectScreen *screen = data.screen();

    auto it = m_states.find(screen);
    if (it == m_states.end() || !it->m_captured) {
        effects->paintScreen(mask, region, data);
        return;
    }

    // Render the screen in an offscreen texture.
    const QSize nativeSize = screen->geometry().size() * screen->devicePixelRatio();
    if (!it->m_current.texture || it->m_current.texture->size() != nativeSize) {
        it->m_current.texture.reset(new GLTexture(GL_RGBA8, nativeSize));
        it->m_current.framebuffer.reset(new GLFramebuffer(it->m_current.texture.get()));
    }

    GLFramebuffer::pushFramebuffer(it->m_current.framebuffer.get());
    effects->paintScreen(mask, region, data);
    GLFramebuffer::popFramebuffer();

    const qreal blendFactor = it->m_timeLine.value();
    const QRectF screenRect = screen->geometry();
    const qreal angle = it->m_angle * (1 - blendFactor);

    const auto scale = effects->renderTargetScale();

    // Projection matrix + rotate transform.
    const QVector3D transformOrigin(screenRect.center());
    QMatrix4x4 modelViewProjectionMatrix(data.projectionMatrix());
    modelViewProjectionMatrix.translate(transformOrigin * scale);
    modelViewProjectionMatrix.rotate(angle, 0, 0, 1);
    modelViewProjectionMatrix.translate(-transformOrigin * scale);

    glActiveTexture(GL_TEXTURE1);
    it->m_prev.texture->bind();
    glActiveTexture(GL_TEXTURE0);
    it->m_current.texture->bind();

    // Clear the background.
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    ShaderManager *sm = ShaderManager::instance();
    sm->pushShader(m_shader.get());
    m_shader->setUniform(m_modelViewProjectioMatrixLocation, modelViewProjectionMatrix);
    m_shader->setUniform(m_blendFactorLocation, float(blendFactor));
    m_shader->setUniform(m_currentTextureLocation, 0);
    m_shader->setUniform(m_previousTextureLocation, 1);

    GLVertexBuffer *vbo = texturedRectVbo(lerp(it->m_oldGeometry, screenRect, blendFactor), scale);
    vbo->bindArrays();
    vbo->draw(GL_TRIANGLES, 0, 6);
    vbo->unbindArrays();
    sm->popShader();

    glActiveTexture(GL_TEXTURE1);
    it->m_prev.texture->unbind();
    glActiveTexture(GL_TEXTURE0);
    it->m_current.texture->unbind();

    effects->addRepaintFull();
}

bool ScreenTransformEffect::isActive() const
{
    return !m_states.isEmpty();
}

} // namespace KWin
