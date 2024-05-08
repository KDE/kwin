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
        const QRectF screenRect = scaledRect(screen->geometry(), viewport.scale());
        m_crossfader.render(viewport.projectionMatrix(), it->m_prev.texture.get(), it->m_current.texture.get(), screenRect, m_timeLine.value());
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
