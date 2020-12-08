/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "snaphelper.h"

#include <kwinglutils.h>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <kwinxrenderutils.h>
#include <xcb/render.h>
#endif

#include <QPainter>

namespace KWin
{

static const int s_lineWidth = 4;
static const QColor s_lineColor = QColor(128, 128, 128, 128);

static QRegion computeDirtyRegion(const QRect &windowRect)
{
    const QMargins outlineMargins(
        s_lineWidth / 2,
        s_lineWidth / 2,
        s_lineWidth / 2,
        s_lineWidth / 2
    );

    QRegion dirtyRegion;
    for (int i = 0; i < effects->numScreens(); ++i) {
        const QRect screenRect = effects->clientArea(ScreenArea, i, 0);

        QRect screenWindowRect = windowRect;
        screenWindowRect.moveCenter(screenRect.center());

        QRect verticalBarRect(0, 0, s_lineWidth, screenRect.height());
        verticalBarRect.moveCenter(screenRect.center());
        verticalBarRect.adjust(-1, -1, 1, 1);
        dirtyRegion += verticalBarRect;

        QRect horizontalBarRect(0, 0, screenRect.width(), s_lineWidth);
        horizontalBarRect.moveCenter(screenRect.center());
        horizontalBarRect.adjust(-1, -1, 1, 1);
        dirtyRegion += horizontalBarRect;

        const QRect outlineOuterRect = screenWindowRect
            .marginsAdded(outlineMargins)
            .adjusted(-1, -1, 1, 1);
        const QRect outlineInnerRect = screenWindowRect
            .marginsRemoved(outlineMargins)
            .adjusted(1, 1, -1, -1);
        dirtyRegion += QRegion(outlineOuterRect) - QRegion(outlineInnerRect);
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
    Q_UNUSED(flags)

    m_animation.timeLine.setDuration(
        std::chrono::milliseconds(static_cast<int>(animationTime(250))));
}

void SnapHelperEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    if (m_animation.active) {
        m_animation.timeLine.update(std::chrono::milliseconds(time));
    }

    effects->prePaintScreen(data, time);
}

void SnapHelperEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    const qreal opacityFactor = m_animation.active
        ? m_animation.timeLine.value()
        : 1.0;

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
        verts.reserve(effects->numScreens() * 24);
        for (int i = 0; i < effects->numScreens(); ++i) {
            const QRect rect = effects->clientArea(ScreenArea, i, 0);
            const int midX = rect.x() + rect.width() / 2;
            const int midY = rect.y() + rect.height() / 2 ;
            const int halfWidth = m_geometry.width() / 2;
            const int halfHeight = m_geometry.height() / 2;

            // Center vertical line.
            verts << rect.x() + rect.width() / 2 << rect.y();
            verts << rect.x() + rect.width() / 2 << rect.y() + rect.height();

            // Center horizontal line.
            verts << rect.x() << rect.y() + rect.height() / 2;
            verts << rect.x() + rect.width() << rect.y() + rect.height() / 2;

            // Top edge of the window outline.
            verts << midX - halfWidth - s_lineWidth / 2 << midY - halfHeight;
            verts << midX + halfWidth + s_lineWidth / 2 << midY - halfHeight;

            // Right edge of the window outline.
            verts << midX + halfWidth << midY - halfHeight + s_lineWidth / 2;
            verts << midX + halfWidth << midY + halfHeight - s_lineWidth / 2;

            // Bottom edge of the window outline.
            verts << midX + halfWidth + s_lineWidth / 2 << midY + halfHeight;
            verts << midX - halfWidth - s_lineWidth / 2 << midY + halfHeight;

            // Left edge of the window outline.
            verts << midX - halfWidth << midY + halfHeight - s_lineWidth / 2;
            verts << midX - halfWidth << midY - halfHeight + s_lineWidth / 2;
        }
        vbo->setData(verts.count() / 2, 2, verts.data(), nullptr);
        vbo->render(GL_LINES);

        glDisable(GL_BLEND);
        glLineWidth(1.0);
    }
    if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        for (int i = 0; i < effects->numScreens(); ++i) {
            const QRect rect = effects->clientArea(ScreenArea, i, 0);
            const int midX = rect.x() + rect.width() / 2;
            const int midY = rect.y() + rect.height() / 2 ;
            const int halfWidth = m_geometry.width() / 2;
            const int halfHeight = m_geometry.height() / 2;

            xcb_rectangle_t rects[6];

            // Center vertical line.
            rects[0].x = rect.x() + rect.width() / 2 - s_lineWidth / 2;
            rects[0].y = rect.y();
            rects[0].width = s_lineWidth;
            rects[0].height = rect.height();

            // Center horizontal line.
            rects[1].x = rect.x();
            rects[1].y = rect.y() + rect.height() / 2 - s_lineWidth / 2;
            rects[1].width = rect.width();
            rects[1].height = s_lineWidth;

            // Top edge of the window outline.
            rects[2].x = midX - halfWidth - s_lineWidth / 2;
            rects[2].y = midY - halfHeight - s_lineWidth / 2;
            rects[2].width = 2 * halfWidth + s_lineWidth;
            rects[2].height = s_lineWidth;

            // Right edge of the window outline.
            rects[3].x = midX + halfWidth - s_lineWidth / 2;
            rects[3].y = midY - halfHeight + s_lineWidth / 2;
            rects[3].width = s_lineWidth;
            rects[3].height = 2 * halfHeight - s_lineWidth;

            // Bottom edge of the window outline.
            rects[4].x = midX - halfWidth - s_lineWidth / 2;
            rects[4].y = midY + halfHeight - s_lineWidth / 2;
            rects[4].width = 2 * halfWidth + s_lineWidth;
            rects[4].height = s_lineWidth;

            // Left edge of the window outline.
            rects[5].x = midX - halfWidth - s_lineWidth / 2;
            rects[5].y = midY - halfHeight + s_lineWidth / 2;
            rects[5].width = s_lineWidth;
            rects[5].height = 2 * halfHeight - s_lineWidth;

            QColor color = s_lineColor;
            color.setAlphaF(color.alphaF() * opacityFactor);

            xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_OVER, effects->xrenderBufferPicture(),
                                       preMultiply(color), 6, rects);
        }
#endif
    }
    if (effects->compositingType() == QPainterCompositing) {
        QPainter *painter = effects->scenePainter();
        painter->save();
        QColor color = s_lineColor;
        color.setAlphaF(color.alphaF() * opacityFactor);
        QPen pen(color);
        pen.setWidth(s_lineWidth);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        for (int i = 0; i < effects->numScreens(); ++i) {
            const QRect rect = effects->clientArea(ScreenArea, i, 0);
            // Center lines.
            painter->drawLine(rect.center().x(), rect.y(), rect.center().x(), rect.y() + rect.height());
            painter->drawLine(rect.x(), rect.center().y(), rect.x() + rect.width(), rect.center().y());

            // Window outline.
            QRect outlineRect(0, 0, m_geometry.width(), m_geometry.height());
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
    m_geometry = w->geometry();

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
    m_geometry = w->geometry();

    m_animation.active = true;
    m_animation.timeLine.setDirection(TimeLine::Backward);

    if (m_animation.timeLine.done()) {
        m_animation.timeLine.reset();
    }

    effects->addRepaint(computeDirtyRegion(m_geometry));
}

void SnapHelperEffect::slotWindowFrameGeometryChanged(EffectWindow *w, const QRect &old)
{
    if (w != m_window) {
        return;
    }

    m_geometry = w->geometry();

    effects->addRepaint(computeDirtyRegion(old));
}

bool SnapHelperEffect::isActive() const
{
    return m_window != nullptr || m_animation.active;
}

} // namespace KWin
