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

        // Avoid including this effect while capturing previous screen state.
        m_capturing = true;

        auto &state = m_states[screen];
        if (renderOutput(state.m_prev, screen)) {
            state.m_oldTransform = screen->transform();
            state.m_oldGeometry = screen->geometry();
            state.m_timeLine.setDuration(std::chrono::milliseconds(long(animationTime(250ms))));
            state.m_timeLine.setEasingCurve(QEasingCurve::InOutCubic);
            state.m_angle = transformAngle(changeSet->transform.value(), state.m_oldTransform);
        } else {
            m_states.remove(screen);
        }

        m_capturing = false;
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
    if (!paintScreenInTexture(it->m_current, viewport, mask, region, screen)) {
        m_states.erase(it);
        effects->paintScreen(renderTarget, viewport, mask, region, screen);
        return;
    }

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
