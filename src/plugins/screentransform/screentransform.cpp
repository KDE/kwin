/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "screentransform.h"
#include "core/outputconfiguration.h"
#include "core/outputlayer.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"
#include "scene/workspacescene.h"

#include <QDebug>

using namespace std::chrono_literals;

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

    const QList<LogicalOutput *> screens = effects->screens();
    for (auto screen : screens) {
        addScreen(screen);
    }
    connect(effects, &EffectsHandler::screenAdded, this, &ScreenTransformEffect::addScreen);
    connect(effects, &EffectsHandler::screenRemoved, this, &ScreenTransformEffect::removeScreen);
}

ScreenTransformEffect::~ScreenTransformEffect() = default;

bool ScreenTransformEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing && effects->animationsSupported();
}

qreal transformAngle(OutputTransform current, OutputTransform old)
{
    auto ensureShort = [](int angle) {
        return angle > 180 ? angle - 360 : angle < -180 ? angle + 360
                                                        : angle;
    };
    // % 4 to ignore flipped cases (for now)
    return ensureShort((int(current.kind()) % 4 - int(old.kind()) % 4) * 90);
}

void ScreenTransformEffect::addScreen(LogicalOutput *screen)
{
    connect(screen, &LogicalOutput::aboutToChange, this, [this, screen](OutputChangeSet *changeSet) {
        const OutputTransform transform = changeSet->transform.value_or(screen->transform());
        if (screen->transform() == transform) {
            return;
        }

        // Avoid including this effect while capturing previous screen state.
        m_capturing = true;
        auto resetCapturing = qScopeGuard([this]() {
            m_capturing = false;
        });

        effects->makeOpenGLContextCurrent();
        auto texture = GLTexture::allocate(GL_RGBA16F, screen->pixelSize());
        if (!texture) {
            m_states.remove(screen);
            return;
        }
        auto &state = m_states[screen];
        state.m_oldTransform = screen->transform();
        state.m_oldGeometry = screen->geometry();
        state.m_timeLine.setDuration(std::chrono::milliseconds(long(animationTime(250ms))));
        state.m_timeLine.setEasingCurve(QEasingCurve::InOutCubic);
        state.m_angle = transformAngle(changeSet->transform.value(), state.m_oldTransform);
        state.m_prev.texture = std::move(texture);
        state.m_prev.framebuffer = std::make_unique<GLFramebuffer>(state.m_prev.texture.get());
        RenderTarget renderTarget(state.m_prev.framebuffer.get(), screen->blendingColor());

        Scene *scene = effects->scene();
        SceneView delegate(scene, screen, nullptr, nullptr);
        delegate.setViewport(screen->geometryF());
        delegate.setScale(screen->scale());
        scene->prePaint(&delegate);
        scene->paint(renderTarget, QPoint(), screen->geometry());
        scene->postPaint();
    });
}

void ScreenTransformEffect::removeScreen(LogicalOutput *screen)
{
    screen->disconnect(this);
    if (auto it = m_states.find(screen); it != m_states.end()) {
        effects->makeOpenGLContextCurrent();
        m_states.erase(it);
    }
}

void ScreenTransformEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    m_currentView = data.view;
    auto it = m_states.find(data.screen);
    if (it != m_states.end()) {
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
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    const auto opt = vbo->map<GLVertex2D>(6);
    if (!opt) {
        return nullptr;
    }
    const auto map = *opt;

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

void ScreenTransformEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &deviceRegion, LogicalOutput *screen)
{
    auto it = m_states.find(screen);
    if (it == m_states.end() || m_currentView->backendOutput() != screen->backendOutput()) {
        effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
        return;
    }

    // Render the screen in an offscreen texture.
    const QSize nativeSize = screen->geometry().size() * screen->scale();
    if (!it->m_current.texture || it->m_current.texture->size() != nativeSize
        || it->m_current.texture->internalFormat() != renderTarget.texture()->internalFormat()) {
        it->m_current.texture = GLTexture::allocate(renderTarget.texture()->internalFormat(), nativeSize);
        if (!it->m_current.texture) {
            m_states.remove(screen);
            return;
        }
        it->m_current.framebuffer = std::make_unique<GLFramebuffer>(it->m_current.texture.get());
    }

    RenderTarget fboRenderTarget(it->m_current.framebuffer.get(), renderTarget.colorDescription());
    RenderViewport fboViewport(viewport.renderRect(), viewport.scale(), fboRenderTarget, QPoint());

    GLFramebuffer::pushFramebuffer(it->m_current.framebuffer.get());
    effects->paintScreen(fboRenderTarget, fboViewport, mask, deviceRegion, screen);
    GLFramebuffer::popFramebuffer();

    const qreal blendFactor = it->m_timeLine.value();
    const QRectF screenRect = screen->geometry();
    const qreal angle = it->m_angle * (1 - blendFactor);

    const auto scale = viewport.scale();

    // Projection matrix + rotate transform.
    const QVector3D transformOrigin(screenRect.center());
    QMatrix4x4 modelViewProjectionMatrix(viewport.projectionMatrix());
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

    GLVertexBuffer *vbo = texturedRectVbo(lerp(it->m_oldGeometry, screenRect, blendFactor), scale);
    if (!vbo) {
        return;
    }

    ShaderManager *sm = ShaderManager::instance();
    sm->pushShader(m_shader.get());
    m_shader->setUniform(m_modelViewProjectioMatrixLocation, modelViewProjectionMatrix);
    m_shader->setUniform(m_blendFactorLocation, float(blendFactor));
    m_shader->setUniform(m_currentTextureLocation, 0);
    m_shader->setUniform(m_previousTextureLocation, 1);

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
    return !m_states.isEmpty() && !m_capturing;
}

} // namespace KWin

#include "moc_screentransform.cpp"
