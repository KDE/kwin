/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "sessionfade.h"
#include "core/outputconfiguration.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"
#include "scene/workspacescene.h"

#include <KScreenLocker/KsldApp>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(screentransform);
}

using namespace std::chrono_literals;

namespace KWin
{

SessionFadeEffect::SessionFadeEffect()
    : Effect()
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin/SessionFade"),
                                                 QStringLiteral("org.kde.KWin.SessionFade"),
                                                 this,
                                                 QDBusConnection::ExportAllSlots);

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

    connect(effects, &EffectsHandler::screenRemoved, this, &SessionFadeEffect::removeScreen);
    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateAboutToChange, this, [this](ScreenLocker::KSldApp::LockState newState) {
        switch (newState) {
        case ScreenLocker::KSldApp::AcquiringLock:
            startFadeOut();
            break;
        case ScreenLocker::KSldApp::Locked:
            break;
        case ScreenLocker::KSldApp::Unlocked:
            startFadeIn(true);
            break;
        }
    });
}

SessionFadeEffect::~SessionFadeEffect() = default;

bool SessionFadeEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing && effects->waylandDisplay() && effects->animationsSupported();
}

void SessionFadeEffect::removeScreen(Output *output)
{
    m_states.remove(output);
}

void SessionFadeEffect::saveCurrentState()
{
    Scene *scene = effects->scene();
    for (auto screen : workspace()->outputs()) {
        RenderLayer layer(screen->renderLoop());
        SceneDelegate delegate(scene, screen);
        delegate.setLayer(&layer);

        // Avoid including this effect while capturing previous screen state.
        m_capturing = true;
        scene->prePaint(&delegate);

        effects->makeOpenGLContextCurrent();
        if (auto texture = GLTexture::allocate(GL_RGBA8, screen->pixelSize())) {
            auto &state = m_states[screen];
            state.m_prev.texture = std::move(texture);
            state.m_prev.framebuffer = std::make_unique<GLFramebuffer>(state.m_prev.texture.get());

            RenderTarget renderTarget(state.m_prev.framebuffer.get());
            scene->paint(renderTarget, screen->geometry());
        } else {
            m_states.remove(screen);
        }

        scene->postPaint();
        effects->doneOpenGLContextCurrent();
        m_capturing = false;
    }
    effects->addRepaintFull();
}

void SessionFadeEffect::startFadeOut()
{
    m_timeLine.reset();
    m_timeLine.setDuration(std::chrono::milliseconds(long(animationTime(250ms))));
    m_timeLine.setEasingCurve(QEasingCurve::InCurve);
    m_timeLine.setDirection(TimeLine::Backward);
    saveCurrentState();
}

void SessionFadeEffect::startFadeIn(bool fadeWithState)
{
    m_timeLine.reset();
    m_timeLine.setDirection(TimeLine::Forward);
    m_timeLine.setDuration(std::chrono::milliseconds(long(animationTime(250ms))));
    m_timeLine.setEasingCurve(QEasingCurve::InCubic);
    if (fadeWithState) {
        saveCurrentState();
    } else {
        for (const auto outputs = workspace()->outputs(); auto s : outputs) {
            m_states[s].m_prev.framebuffer.reset();
            m_states[s].m_prev.texture.reset();
        }
    }
}

void SessionFadeEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    m_timeLine.advance(presentTime);
    if (m_timeLine.done() && m_timeLine.direction() == TimeLine::Forward) {
        m_states.remove(data.screen);
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

void SessionFadeEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, KWin::Output *screen)
{
    auto it = m_states.find(screen);
    if (it == m_states.end()) {
        effects->paintScreen(renderTarget, viewport, mask, region, screen);
        return;
    }
    effects->makeOpenGLContextCurrent();

    // Render the screen in an offscreen texture.
    const QSize nativeSize = screen->pixelSize();
    if (m_timeLine.direction() == TimeLine::Forward) {
        if (!it->m_current.texture || it->m_current.texture->size() != nativeSize) {
            it->m_current.texture = GLTexture::allocate(GL_RGBA8, nativeSize);
            if (!it->m_current.texture) {
                m_states.remove(screen);
                return;
            }
            it->m_current.framebuffer = std::make_unique<GLFramebuffer>(it->m_current.texture.get());
        }

        // If we're fading in, we want to render it fresh
        RenderTarget fboRenderTarget(it->m_current.framebuffer.get());
        RenderViewport fboViewport(viewport.renderRect(), viewport.scale(), fboRenderTarget);
        GLFramebuffer::pushFramebuffer(it->m_current.framebuffer.get());
        effects->paintScreen(fboRenderTarget, fboViewport, mask, region, screen);
        GLFramebuffer::popFramebuffer();
    }

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    if (m_timeLine.direction() == TimeLine::Forward && it->m_prev.texture) {
        const qreal blendFactor = m_timeLine.value();
        const QRectF screenRect = screen->geometry();

        const auto scale = viewport.scale();

        glActiveTexture(GL_TEXTURE1);
        it->m_prev.texture->bind();
        glActiveTexture(GL_TEXTURE0);
        it->m_current.texture->bind();

        // Clear the background.
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        GLVertexBuffer *vbo = texturedRectVbo(screenRect, scale);
        if (!vbo) {
            return;
        }

        ShaderManager *sm = ShaderManager::instance();
        ShaderBinder binder(m_shader.get());
        m_shader->setUniform(m_modelViewProjectioMatrixLocation, viewport.projectionMatrix());
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
    } else {
        GLShader *shader = ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::Modulate);
        ShaderBinder binder(shader);

        const qreal alpha = m_timeLine.value();
        const auto toXYZ = renderTarget.colorDescription().colorimetry().toXYZ();
        shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
        shader->setUniform(GLShader::Vec3Uniform::PrimaryBrightness, QVector3D(toXYZ(1, 0), toXYZ(1, 1), toXYZ(1, 2)));

        shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(alpha, alpha, alpha, alpha));
        if (it->m_current.texture) {
            it->m_current.texture->render(nativeSize);
        }
        if (it->m_prev.texture) {
            it->m_prev.texture->render(nativeSize);
        }
    }
    effects->addRepaintFull();
}

void SessionFadeEffect::prePaintWindow(KWin::EffectWindow *w, KWin::WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (!m_firstPresent.has_value()) {
        m_firstPresent = presentTime;
        startFadeIn(false);
    }

    if (m_timeLine.done() && w->isLockScreen() && m_states.contains(w->screen())) {
        startFadeIn(false);
    }
}

bool SessionFadeEffect::isActive() const
{
    return !m_firstPresent.has_value() || (!m_states.isEmpty() && !m_capturing);
}

} // namespace KWin

#include "moc_sessionfade.cpp"
