/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "snaphelper.h"

#include <kwinglutils.h>

#include <QPainter>

namespace KWin
{

static const int s_lineWidth = 4;
static const QColor s_lineColor = QColor(128, 128, 128, 128);

static QRegion computeDirtyRegion(const QRectF &windowRect)
{
    const QMargins outlineMargins(
        s_lineWidth / 2,
        s_lineWidth / 2,
        s_lineWidth / 2,
        s_lineWidth / 2);

    QRegion dirtyRegion;

    const QList<EffectScreen *> screens = effects->screens();
    for (EffectScreen *screen : screens) {
        const QRectF screenRect = effects->clientArea(ScreenArea, screen, effects->currentDesktop());

        QRectF screenWindowRect = windowRect;
        screenWindowRect.moveCenter(screenRect.center());

        QRectF verticalBarRect(0, 0, s_lineWidth, screenRect.height());
        verticalBarRect.moveCenter(screenRect.center());
        verticalBarRect.adjust(-1, -1, 1, 1);
        dirtyRegion += verticalBarRect.toAlignedRect();

        QRectF horizontalBarRect(0, 0, screenRect.width(), s_lineWidth);
        horizontalBarRect.moveCenter(screenRect.center());
        horizontalBarRect.adjust(-1, -1, 1, 1);
        dirtyRegion += horizontalBarRect.toAlignedRect();

        const QRectF outlineOuterRect = screenWindowRect
                                            .marginsAdded(outlineMargins)
                                            .adjusted(-1, -1, 1, 1);
        const QRectF outlineInnerRect = screenWindowRect
                                            .marginsRemoved(outlineMargins)
                                            .adjusted(1, 1, -1, -1);
        dirtyRegion += QRegion(outlineOuterRect.toRect()) - QRegion(outlineInnerRect.toRect());
    }

    return dirtyRegion;
}

SnapHelperEffect::SnapHelperEffect()
{
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowClosed, this, &SnapHelperEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowStartUserMovedResized, this, &SnapHelperEffect::slotWindowStartUserMovedResized);
    connect(effects, &EffectsHandler::windowFinishUserMovedResized, this, &SnapHelperEffect::slotWindowFinishUserMovedResized);
    connect(effects, &EffectsHandler::windowFrameGeometryChanged, this, &SnapHelperEffect::slotWindowFrameGeometryChanged);
}

SnapHelperEffect::~SnapHelperEffect()
{
}

void SnapHelperEffect::reconfigure(ReconfigureFlags flags)
{
    m_animation.timeLine.setDuration(
        std::chrono::milliseconds(static_cast<int>(animationTime(250))));
}

void SnapHelperEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_animation.active) {
        m_animation.timeLine.advance(presentTime);
    }

    effects->prePaintScreen(data, presentTime);
}

void SnapHelperEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    const qreal opacityFactor = m_animation.active
        ? m_animation.timeLine.value()
        : 1.0;
    const QList<EffectScreen *> screens = effects->screens();

    const auto scale = effects->renderTargetScale();

    // Display the guide
    if (effects->isOpenGLCompositing()) {
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->reset();
        vbo->setUseColor(true);
        ShaderBinder binder(ShaderTrait::UniformColor);
        binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        QColor color = s_lineColor;
        color.setAlphaF(color.alphaF() * opacityFactor);
        vbo->setColor(color);

        glLineWidth(s_lineWidth);
        QVector<float> verts;
        verts.reserve(screens.count() * 24);
        for (EffectScreen *screen : screens) {
            const QRectF rect = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
            const int midX = rect.x() + rect.width() / 2;
            const int midY = rect.y() + rect.height() / 2;
            const int halfWidth = m_geometry.width() / 2;
            const int halfHeight = m_geometry.height() / 2;

            // Center vertical line.
            verts << (rect.x() + rect.width() / 2) * scale << rect.y() * scale;
            verts << (rect.x() + rect.width() / 2) * scale << (rect.y() + rect.height()) * scale;

            // Center horizontal line.
            verts << rect.x() * scale << (rect.y() + rect.height() / 2) * scale;
            verts << (rect.x() + rect.width()) * scale << (rect.y() + rect.height() / 2) * scale;

            // Top edge of the window outline.
            verts << (midX - halfWidth - s_lineWidth / 2) * scale << (midY - halfHeight) * scale;
            verts << (midX + halfWidth + s_lineWidth / 2) * scale << (midY - halfHeight) * scale;

            // Right edge of the window outline.
            verts << (midX + halfWidth) * scale << (midY - halfHeight + s_lineWidth / 2) * scale;
            verts << (midX + halfWidth) * scale << (midY + halfHeight - s_lineWidth / 2) * scale;

            // Bottom edge of the window outline.
            verts << (midX + halfWidth + s_lineWidth / 2) * scale << (midY + halfHeight) * scale;
            verts << (midX - halfWidth - s_lineWidth / 2) * scale << (midY + halfHeight) * scale;

            // Left edge of the window outline.
            verts << (midX - halfWidth) * scale << (midY + halfHeight - s_lineWidth / 2) * scale;
            verts << (midX - halfWidth) * scale << (midY - halfHeight + s_lineWidth / 2) * scale;
        }
        vbo->setData(verts.count() / 2, 2, verts.data(), nullptr);
        vbo->render(GL_LINES);

        glDisable(GL_BLEND);
        glLineWidth(1.0);
    } else if (effects->compositingType() == QPainterCompositing) {
        QPainter *painter = effects->scenePainter();
        painter->save();
        QColor color = s_lineColor;
        color.setAlphaF(color.alphaF() * opacityFactor);
        QPen pen(color);
        pen.setWidth(s_lineWidth);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        for (EffectScreen *screen : screens) {
            const QRectF rect = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
            // Center lines.
            painter->drawLine(rect.center().x(), rect.y(), rect.center().x(), rect.y() + rect.height());
            painter->drawLine(rect.x(), rect.center().y(), rect.x() + rect.width(), rect.center().y());

            // Window outline.
            QRectF outlineRect(0, 0, m_geometry.width(), m_geometry.height());
            outlineRect.moveCenter(rect.center());
            painter->drawRect(outlineRect);
        }
        painter->restore();
    }
}

void SnapHelperEffect::postPaintScreen()
{
    if (m_animation.active) {
        effects->addRepaint(computeDirtyRegion(m_geometry));
    }

    if (m_animation.timeLine.done()) {
        m_animation.active = false;
    }

    effects->postPaintScreen();
}

void SnapHelperEffect::slotWindowClosed(EffectWindow *w)
{
    if (w != m_window) {
        return;
    }

    m_window = nullptr;

    m_animation.active = true;
    m_animation.timeLine.setDirection(TimeLine::Backward);

    if (m_animation.timeLine.done()) {
        m_animation.timeLine.reset();
    }

    effects->addRepaint(computeDirtyRegion(m_geometry));
}

void SnapHelperEffect::slotWindowStartUserMovedResized(EffectWindow *w)
{
    if (!w->isMovable()) {
        return;
    }

    m_window = w;
    m_geometry = w->frameGeometry();

    m_animation.active = true;
    m_animation.timeLine.setDirection(TimeLine::Forward);

    if (m_animation.timeLine.done()) {
        m_animation.timeLine.reset();
    }

    effects->addRepaint(computeDirtyRegion(m_geometry));
}

void SnapHelperEffect::slotWindowFinishUserMovedResized(EffectWindow *w)
{
    if (w != m_window) {
        return;
    }

    m_window = nullptr;
    m_geometry = w->frameGeometry();

    m_animation.active = true;
    m_animation.timeLine.setDirection(TimeLine::Backward);

    if (m_animation.timeLine.done()) {
        m_animation.timeLine.reset();
    }

    effects->addRepaint(computeDirtyRegion(m_geometry));
}

void SnapHelperEffect::slotWindowFrameGeometryChanged(EffectWindow *w, const QRectF &old)
{
    if (w != m_window) {
        return;
    }

    m_geometry = w->frameGeometry();

    effects->addRepaint(computeDirtyRegion(old));
}

bool SnapHelperEffect::isActive() const
{
    return m_window != nullptr || m_animation.active;
}

} // namespace KWin
