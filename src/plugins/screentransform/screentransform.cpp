/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "screentransform.h"
#include "core/outputconfiguration.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"
#include "scene/workspacescene.h"

#include <QDebug>

using namespace std::chrono_literals;

namespace KWin
{

ScreenTransformEffect::ScreenTransformEffect()
    : Effect()
{
    const QList<Output *> screens = effects->screens();
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

qreal transformAngle(OutputTransform current, OutputTransform old)
{
    auto ensureShort = [](int angle) {
        return angle > 180 ? angle - 360 : angle < -180 ? angle + 360
                                                        : angle;
    };
    // % 4 to ignore flipped cases (for now)
    return ensureShort((int(current.kind()) % 4 - int(old.kind()) % 4) * 90);
}

void ScreenTransformEffect::addScreen(Output *screen)
{
    connect(screen, &Output::aboutToChange, this, [this, screen](OutputChangeSet *changeSet) {
        const OutputTransform transform = changeSet->transform.value_or(screen->transform());
        if (screen->transform() == transform) {
            return;
        }

        Scene *scene = effects->scene();
        RenderLayer layer(screen->renderLoop());
        SceneDelegate delegate(scene, screen);
        delegate.setLayer(&layer);

        // Avoid including this effect while capturing previous screen state.
        m_capturing = true;
        auto resetCapturing = qScopeGuard([this]() {
            m_capturing = false;
        });

        scene->prePaint(&delegate);

        effects->makeOpenGLContextCurrent();
        if (auto texture = GLTexture::allocate(GL_RGBA8, screen->pixelSize())) {
            auto &state = m_states[screen];
            state.m_oldTransform = screen->transform();
            state.m_oldGeometry = screen->geometry();
            state.m_timeLine.setDuration(std::chrono::milliseconds(long(animationTime(250ms))));
            state.m_timeLine.setEasingCurve(QEasingCurve::InOutCubic);
            state.m_angle = transformAngle(changeSet->transform.value(), state.m_oldTransform);
            state.m_prev.texture = std::move(texture);
            state.m_prev.framebuffer = std::make_unique<GLFramebuffer>(state.m_prev.texture.get());

            RenderTarget renderTarget(state.m_prev.framebuffer.get());
            scene->paint(renderTarget, screen->geometry());
        } else {
            m_states.remove(screen);
        }

        scene->postPaint();
    });
}

void ScreenTransformEffect::removeScreen(Output *screen)
{
    screen->disconnect(this);
    if (auto it = m_states.find(screen); it != m_states.end()) {
        effects->makeOpenGLContextCurrent();
        m_states.erase(it);
    }
}

void ScreenTransformEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto it = m_states.find(data.screen);
    if (it != m_states.end()) {
        it->m_timeLine.advance(presentTime);
        if (it->m_timeLine.done()) {
            m_states.remove(data.screen);
        }
    }

    effects->prePaintScreen(data, presentTime);
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

void ScreenTransformEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, KWin::Output *screen)
{
    auto it = m_states.find(screen);
    if (it == m_states.end()) {
        effects->paintScreen(renderTarget, viewport, mask, region, screen);
        return;
    }

    // Render the screen in an offscreen texture.
    const QSize nativeSize = screen->geometry().size() * screen->scale();
    if (!it->m_current.texture || it->m_current.texture->size() != nativeSize) {
        it->m_current.texture = GLTexture::allocate(GL_RGBA8, nativeSize);
        if (!it->m_current.texture) {
            m_states.remove(screen);
            return;
        }
        it->m_current.framebuffer = std::make_unique<GLFramebuffer>(it->m_current.texture.get());
    }

    RenderTarget fboRenderTarget(it->m_current.framebuffer.get());
    RenderViewport fboViewport(viewport.renderRect(), viewport.scale(), fboRenderTarget);

    GLFramebuffer::pushFramebuffer(it->m_current.framebuffer.get());
    effects->paintScreen(fboRenderTarget, fboViewport, mask, region, screen);
    GLFramebuffer::popFramebuffer();

    const qreal blendFactor = it->m_timeLine.value();
    const QRectF screenRect = screen->geometry();
    const qreal angle = it->m_angle * (1 - blendFactor);

    const auto scale = viewport.scale();

    // Projection matrix + rotate transform.
    const QRectF rect = scaledRect(lerp(it->m_oldGeometry, screenRect, blendFactor), scale);
    const QVector3D transformOrigin(screenRect.center());
    QMatrix4x4 modelViewProjectionMatrix(viewport.projectionMatrix());
    modelViewProjectionMatrix.translate(transformOrigin * scale);
    modelViewProjectionMatrix.rotate(angle, 0, 0, 1);
    modelViewProjectionMatrix.translate(-transformOrigin * scale);

    m_crossfader.render(modelViewProjectionMatrix, it->m_prev.texture.get(), it->m_current.texture.get(), rect, blendFactor);

    effects->addRepaintFull();
}

bool ScreenTransformEffect::isActive() const
{
    return !m_states.isEmpty() && !m_capturing;
}

} // namespace KWin

#include "moc_screentransform.cpp"
