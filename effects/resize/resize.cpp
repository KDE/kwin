/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resize.h"
// KConfigSkeleton
#include "resizeconfig.h"

#include <kwinglutils.h>
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include "kwinxrenderutils.h"
#endif

#include <KColorScheme>

#include <QVector2D>
#include <QPainter>

namespace KWin
{

ResizeEffect::ResizeEffect()
    : AnimationEffect()
    , m_active(false)
    , m_resizeWindow(nullptr)
{
    initConfig<ResizeConfig>();
    reconfigure(ReconfigureAll);
    connect(effects, &EffectsHandler::windowStartUserMovedResized, this, &ResizeEffect::slotWindowStartUserMovedResized);
    connect(effects, &EffectsHandler::windowStepUserMovedResized, this, &ResizeEffect::slotWindowStepUserMovedResized);
    connect(effects, &EffectsHandler::windowFinishUserMovedResized, this, &ResizeEffect::slotWindowFinishUserMovedResized);
}

ResizeEffect::~ResizeEffect()
{
}

void ResizeEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_active) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }
    AnimationEffect::prePaintScreen(data, time);
}

void ResizeEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (m_active && w == m_resizeWindow)
        data.mask |= PAINT_WINDOW_TRANSFORMED;
    AnimationEffect::prePaintWindow(w, data, time);
}

void ResizeEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (m_active && w == m_resizeWindow) {
        if (m_features & TextureScale) {
            data += (m_currentGeometry.topLeft() - m_originalGeometry.topLeft());
            data *= QVector2D(float(m_currentGeometry.width())/m_originalGeometry.width(),
                              float(m_currentGeometry.height())/m_originalGeometry.height());
        }
        effects->paintWindow(w, mask, region, data);

        if (m_features & Outline) {
            QRegion intersection = m_originalGeometry.intersected(m_currentGeometry);
            QRegion paintRegion = QRegion(m_originalGeometry).united(m_currentGeometry).subtracted(intersection);
            float alpha = 0.8f;
            QColor color = KColorScheme(QPalette::Normal, KColorScheme::Selection).background().color();

            if (effects->isOpenGLCompositing()) {
                GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
                vbo->reset();
                vbo->setUseColor(true);
                ShaderBinder binder(ShaderTrait::UniformColor);
                binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, data.screenProjectionMatrix());
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                color.setAlphaF(alpha);
                vbo->setColor(color);
                QVector<float> verts;
                verts.reserve(paintRegion.rectCount() * 12);
                for (const QRect &r : paintRegion) {
                    verts << r.x() + r.width() << r.y();
                    verts << r.x() << r.y();
                    verts << r.x() << r.y() + r.height();
                    verts << r.x() << r.y() + r.height();
                    verts << r.x() + r.width() << r.y() + r.height();
                    verts << r.x() + r.width() << r.y();
                }
                vbo->setData(verts.count() / 2, 2, verts.data(), nullptr);
                vbo->render(GL_TRIANGLES);
                glDisable(GL_BLEND);
            }

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            if (effects->compositingType() == XRenderCompositing) {
                QVector<xcb_rectangle_t> rects;
                for (const QRect &r : paintRegion) {
                    xcb_rectangle_t rect = {int16_t(r.x()), int16_t(r.y()), uint16_t(r.width()), uint16_t(r.height())};
                    rects << rect;
                }
                xcb_render_fill_rectangles(xcbConnection(), XCB_RENDER_PICT_OP_OVER,
                                           effects->xrenderBufferPicture(), preMultiply(color, alpha),
                                           rects.count(), rects.constData());
            }
#endif
            if (effects->compositingType() == QPainterCompositing) {
                QPainter *painter = effects->scenePainter();
                painter->save();
                color.setAlphaF(alpha);
                for (const QRect &r : paintRegion) {
                    painter->fillRect(r, color);
                }
                painter->restore();
            }
        }
    } else {
        AnimationEffect::paintWindow(w, mask, region, data);
    }
}

void ResizeEffect::reconfigure(ReconfigureFlags)
{
    m_features = 0;
    ResizeConfig::self()->read();
    if (ResizeConfig::textureScale())
        m_features |= TextureScale;
    if (ResizeConfig::outline())
        m_features |= Outline;
}

void ResizeEffect::slotWindowStartUserMovedResized(EffectWindow *w)
{
    if (w->isUserResize() && !w->isUserMove()) {
        m_active = true;
        m_resizeWindow = w;
        m_originalGeometry = w->geometry();
        m_currentGeometry = w->geometry();
        w->addRepaintFull();
    }
}

void ResizeEffect::slotWindowFinishUserMovedResized(EffectWindow *w)
{
    if (m_active && w == m_resizeWindow) {
        m_active = false;
        m_resizeWindow = nullptr;
        if (m_features & TextureScale)
            animate(w, CrossFadePrevious, 0, 150, FPx2(1.0));
        effects->addRepaintFull();
    }
}

void ResizeEffect::slotWindowStepUserMovedResized(EffectWindow *w, const QRect &geometry)
{
    if (m_active && w == m_resizeWindow) {
        m_currentGeometry = geometry;
        effects->addRepaintFull();
    }
}

} // namespace
