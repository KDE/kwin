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

namespace KWin
{
ScreenTransformEffect::ScreenTransformEffect()
    : Effect()
{
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
        return angle > 180 ? angle - 360 : angle < -180 ? angle + 360 : angle;
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
        state.m_timeLine.setEasingCurve(QEasingCurve::OutCirc);
        state.m_lastPresentTime = std::chrono::milliseconds::zero();
        state.m_angle = transformAngle(screen->transform(), state.m_oldTransform);
        Q_ASSERT(state.m_angle != 0);
        effects->addRepaintFull();
    });
    connect(screen, &EffectScreen::aboutToChange, this, [this, screen] {
        effects->makeOpenGLContextCurrent();
        auto &state = m_states[screen];
        state.m_oldTransform = screen->transform();
        state.m_texture.reset(new GLTexture(GL_RGBA8, screen->geometry().size() * screen->devicePixelRatio()));

        // Rendering the current scene into a texture
        const bool c = state.m_texture->create();
        Q_ASSERT(c);
        GLRenderTarget renderTarget(*state.m_texture);
        GLRenderTarget::pushRenderTarget(&renderTarget);

        GLVertexBuffer::setVirtualScreenGeometry(screen->geometry());
        GLRenderTarget::setVirtualScreenGeometry(screen->geometry());
        GLVertexBuffer::setVirtualScreenScale(screen->devicePixelRatio());
        GLRenderTarget::setVirtualScreenScale(screen->devicePixelRatio());

        effects->renderScreen(screen);
        state.m_captured = true;
        GLRenderTarget::popRenderTarget();
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
    if (isScreenTransforming(data.screen)) {
        std::chrono::milliseconds delta = std::chrono::milliseconds::zero();
        auto &state = m_states[data.screen];
        if (state.m_lastPresentTime.count()) {
            delta = presentTime - state.m_lastPresentTime;
        }
        state.m_lastPresentTime = presentTime;
        if (state.isSecondHalf()) {
            data.mask |= PAINT_SCREEN_TRANSFORMED;
        }

        state.m_timeLine.update(delta);
        if (state.m_timeLine.done()) {
            m_states.remove(data.screen);
        }
    }

    effects->prePaintScreen(data, presentTime);
}

void ScreenTransformEffect::paintScreen(int mask, const QRegion &region, KWin::ScreenPaintData &data)
{
    auto screen = data.screen();
    if (isScreenTransforming(screen)) {
        auto &state = m_states[screen];
        if (state.isSecondHalf()) {
            data.setRotationAngle(state.m_angle / 2 * (1 - state.m_timeLine.value()));
            auto center = screen->geometry().center();
            data.setRotationOrigin(QVector3D(center.x(), center.y(), 0));
            effects->addRepaintFull();
        }
    }

    effects->paintScreen(mask, region, data);
    if (isScreenTransforming(screen)) {
        auto &state = m_states[screen];

        if (!state.isSecondHalf()) {
            Q_ASSERT(state.m_texture);

            ShaderBinder binder(ShaderTrait::MapTexture);
            GLShader *shader(binder.shader());
            if (!shader) {
                return;
            }
            const QRect screenGeometry = screen->geometry();
            const QRect textureRect = {0, 0, state.m_texture->width(), state.m_texture->height()};

            QMatrix4x4 matrix(data.projectionMatrix());
            // Go to the former texture centre
            matrix.translate(screenGeometry.width() / 2, screenGeometry.height() / 2);
            // Invert the transformation
            matrix.rotate(state.m_angle / 2 * (1 + 1 - state.m_timeLine.value()), 0, 0, 1);
            // Go to the screen centre
            matrix.translate(-textureRect.width() / 2, -textureRect.height() / 2);
            shader->setUniform(GLShader::ModelViewProjectionMatrix, matrix);

            state.m_texture->bind();
            state.m_texture->render(screen->geometry(), textureRect);
            state.m_texture->unbind();
        }
        effects->addRepaintFull();
    }
}

void ScreenTransformEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto screen = w->screen();
    if (isScreenTransforming(screen)) {
        auto &state = m_states[screen];
        if (!state.isSecondHalf()) {
            data.setTranslucent();
        }
    }
    effects->prePaintWindow(w, data, presentTime);
}

void ScreenTransformEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    auto screen = w->screen();
    if (isScreenTransforming(screen)) {
        auto &state = m_states[screen];
        if (!state.isSecondHalf()) {
            // During the first half we want all on 0 because we'll be rendering our texture
            data.multiplyOpacity(0.0);
            data.multiplyBrightness(0.0);
        }
    }
    effects->paintWindow(w, mask, region, data);
}

bool ScreenTransformEffect::isActive() const
{
    return !m_states.isEmpty();
}

bool ScreenTransformEffect::isScreenTransforming(EffectScreen *screen) const
{
    auto it = m_states.constFind(screen);
    return it != m_states.constEnd() && it->m_captured;
}

ScreenTransformEffect::ScreenState::~ScreenState() = default;

} // namespace KWin
