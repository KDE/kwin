/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#include "snaphelper.h"

#include "kwinglutils.h"
#include "kwinxrenderutils.h"

namespace KWin
{

KWIN_EFFECT(snaphelper, SnapHelperEffect)

SnapHelperEffect::SnapHelperEffect()
    : m_active(false)
    , m_window(NULL)
{
    m_timeline.setCurveShape(QTimeLine::LinearCurve);
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowStartUserMovedResized(KWin::EffectWindow*)), this, SLOT(slotWindowStartUserMovedResized(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowFinishUserMovedResized(KWin::EffectWindow*)), this, SLOT(slotWindowFinishUserMovedResized(KWin::EffectWindow*)));

    /*if ( effects->compositingType() == XRenderCompositing )
        {
        XGCValues gcattr;
        // TODO: Foreground color
        gcattr.line_width = 4;
        m_gc = XCreateGC( display(), rootWindow(), GCLineWidth, &gcattr );
        }*/
}

SnapHelperEffect::~SnapHelperEffect()
{
    //if ( effects->compositingType() == XRenderCompositing )
    //    XFreeGC( display(), m_gc );
}

void SnapHelperEffect::reconfigure(ReconfigureFlags)
{
    m_timeline.setDuration(animationTime(250));
}

void SnapHelperEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    double oldValue = m_timeline.currentValue();
    if (m_active)
        m_timeline.setCurrentTime(m_timeline.currentTime() + time);
    else
        m_timeline.setCurrentTime(m_timeline.currentTime() - time);
    if (oldValue != m_timeline.currentValue())
        effects->addRepaintFull();
    effects->prePaintScreen(data, time);
}

void SnapHelperEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (m_timeline.currentValue() != 0.0) {
        // Display the guide
        if (effects->compositingType() == OpenGLCompositing) {
#ifndef KWIN_HAVE_OPENGLES
            glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
#endif
            GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
            vbo->reset();
            vbo->setUseColor(true);
            if (ShaderManager::instance()->isValid()) {
                ShaderManager::instance()->pushShader(ShaderManager::ColorShader);
            }
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            QColor color;
            color.setRedF(0.5);
            color.setGreenF(0.5);
            color.setBlueF(0.5);
            color.setAlphaF(m_timeline.currentValue() * 0.5);
            vbo->setColor(color);
            glLineWidth(4.0);
            QVector<float> verts;
            verts.reserve(effects->numScreens() * 24);
            for (int i = 0; i < effects->numScreens(); i++) {
                const QRect& rect = effects->clientArea(ScreenArea, i, 0);
                int midX = rect.x() + rect.width() / 2;
                int midY = rect.y() + rect.height() / 2 ;
                int halfWidth = m_window->width() / 2;
                int halfHeight = m_window->height() / 2;

                // Center lines
                verts << rect.x() + rect.width() / 2 << rect.y();
                verts << rect.x() + rect.width() / 2 << rect.y() + rect.height();
                verts << rect.x() << rect.y() + rect.height() / 2;
                verts << rect.x() + rect.width() << rect.y() + rect.height() / 2;

                // Window outline
                // The +/- 2 is to prevent line overlap
                verts << midX - halfWidth + 2 << midY - halfHeight;
                verts << midX + halfWidth + 2 << midY - halfHeight;
                verts << midX + halfWidth << midY - halfHeight + 2;
                verts << midX + halfWidth << midY + halfHeight + 2;
                verts << midX + halfWidth - 2 << midY + halfHeight;
                verts << midX - halfWidth - 2 << midY + halfHeight;
                verts << midX - halfWidth << midY + halfHeight - 2;
                verts << midX - halfWidth << midY - halfHeight - 2;
            }
            vbo->setData(verts.count() / 2, 2, verts.data(), NULL);
            vbo->render(GL_LINES);
            if (ShaderManager::instance()->isValid()) {
                ShaderManager::instance()->popShader();
            }

            glDisable(GL_BLEND);
            glLineWidth(1.0);
#ifndef KWIN_HAVE_OPENGLES
            glPopAttrib();
#endif
        }
        if ( effects->compositingType() == XRenderCompositing ) {
            for ( int i = 0; i < effects->numScreens(); i++ ) {
                const QRect& rect = effects->clientArea( ScreenArea, i, 0 );
                int midX = rect.x() + rect.width() / 2;
                int midY = rect.y() + rect.height() / 2 ;
                int halfWidth = m_window->width() / 2;
                int halfHeight = m_window->height() / 2;

                XRectangle rects[6];
                // Center lines
                rects[0].x = rect.x() + rect.width() / 2 - 2;
                rects[0].y = rect.y();
                rects[0].width = 4;
                rects[0].height = rect.height();
                rects[1].x = rect.x();
                rects[1].y = rect.y() + rect.height() / 2 - 2;
                rects[1].width = rect.width();
                rects[1].height = 4;

                // Window outline
                // The +/- 4 is to prevent line overlap
                rects[2].x = midX - halfWidth + 4;
                rects[2].y = midY - halfHeight;
                rects[2].width = 2*halfWidth - 4;
                rects[2].height = 4;
                rects[3].x = midX + halfWidth - 4;
                rects[3].y = midY - halfHeight + 4;
                rects[3].width = 4;
                rects[3].height = 2*halfHeight - 4;
                rects[4].x = midX - halfWidth;
                rects[4].y = midY + halfHeight - 4;
                rects[4].width = 2*halfWidth - 4;
                rects[4].height = 4;
                rects[5].x = midX - halfWidth;
                rects[5].y = midY - halfHeight;
                rects[5].width = 4;
                rects[5].height = 2*halfHeight - 4;

                XRenderColor c = preMultiply(QColor(128, 128, 128, m_timeline.currentValue()*128));
                XRenderFillRectangles(display(), PictOpOver, effects->xrenderBufferPicture(), &c, rects, 6);
            }
        }
    } else if (m_window && !m_active) {
        if (m_window->isDeleted())
            m_window->unrefWindow();
        m_window = NULL;
    }
}

void SnapHelperEffect::slotWindowClosed(EffectWindow* w)
{
    if (m_window == w) {
        m_window->refWindow();
        m_active = false;
    }
}

void SnapHelperEffect::slotWindowStartUserMovedResized(EffectWindow *w)
{
    if (w->isMovable()) {
        m_active = true;
        m_window = w;
        effects->addRepaintFull();
    }
}

void SnapHelperEffect::slotWindowFinishUserMovedResized(EffectWindow *w)
{
    Q_UNUSED(w)
    if (m_active) {
        m_active = false;
        effects->addRepaintFull();
    }
}

bool SnapHelperEffect::isActive() const
{
    return m_active || m_timeline.currentValue() != 0.0;
}

} // namespace
