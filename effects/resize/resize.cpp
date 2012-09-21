/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "resize.h"
// KConfigSkeleton
#include "resizeconfig.h"

#include <kwinglutils.h>
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#endif

#include <KColorScheme>

#include <QtGui/QVector2D>

namespace KWin
{

KWIN_EFFECT(resize, ResizeEffect)

ResizeEffect::ResizeEffect()
    : m_active(false)
    , m_resizeWindow(0)
{
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowStartUserMovedResized(KWin::EffectWindow*)), this, SLOT(slotWindowStartUserMovedResized(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowStepUserMovedResized(KWin::EffectWindow*,QRect)), this, SLOT(slotWindowStepUserMovedResized(KWin::EffectWindow*,QRect)));
    connect(effects, SIGNAL(windowFinishUserMovedResized(KWin::EffectWindow*)), this, SLOT(slotWindowFinishUserMovedResized(KWin::EffectWindow*)));
}

ResizeEffect::~ResizeEffect()
{
}

void ResizeEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (m_active) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }
    effects->prePaintScreen(data, time);
}

void ResizeEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (m_active && w == m_resizeWindow)
        data.mask |= PAINT_WINDOW_TRANSFORMED;
    effects->prePaintWindow(w, data, time);
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
                ShaderBinder binder(ShaderManager::ColorShader);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                color.setAlphaF(alpha);
                vbo->setColor(color);
                QVector<float> verts;
                verts.reserve(paintRegion.rects().count() * 12);
                foreach (const QRect & r, paintRegion.rects()) {
                    verts << r.x() + r.width() << r.y();
                    verts << r.x() << r.y();
                    verts << r.x() << r.y() + r.height();
                    verts << r.x() << r.y() + r.height();
                    verts << r.x() + r.width() << r.y() + r.height();
                    verts << r.x() + r.width() << r.y();
                }
                vbo->setData(verts.count() / 2, 2, verts.data(), NULL);
                vbo->render(GL_TRIANGLES);
                glDisable(GL_BLEND);
            }

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            if (effects->compositingType() == XRenderCompositing) {
                XRenderColor col;
                col.alpha = int(alpha * 0xffff);
                col.red = int(alpha * 0xffff * color.red() / 255);
                col.green = int(alpha * 0xffff * color.green() / 255);
                col.blue = int(alpha * 0xffff * color.blue() / 255);
                foreach (const QRect & r, paintRegion.rects())
                XRenderFillRectangle(display(), PictOpOver, effects->xrenderBufferPicture(),
                                     &col, r.x(), r.y(), r.width(), r.height());
            }
#endif
        }
    } else
        effects->paintWindow(w, mask, region, data);
}

void ResizeEffect::reconfigure(ReconfigureFlags)
{
    m_features = 0;
    ResizeConfig::self()->readConfig();
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
        m_resizeWindow = NULL;
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
