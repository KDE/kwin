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

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#endif

#include <KColorScheme>

namespace KWin
{

KWIN_EFFECT( resize, ResizeEffect )

ResizeEffect::ResizeEffect()
    : m_active( false )
    , m_resizeWindow( 0 )
    {
    reconfigure( ReconfigureAll );
    }

ResizeEffect::~ResizeEffect()
    {
    }

void ResizeEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    if( m_active )
        {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        }
    effects->prePaintScreen( data, time );
    }

void ResizeEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->paintWindow( w, mask, region, data );
    if( m_active && w == m_resizeWindow )
        {
        QRegion intersection = m_originalWindowRect.intersected( m_currentGeometry );
        QRegion paintRegion = m_originalWindowRect.united( m_currentGeometry ).subtracted( intersection );
        float alpha = 0.8f;
        QColor color = KColorScheme( QPalette::Normal, KColorScheme::Selection ).background().color();

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        if( effects->compositingType() == OpenGLCompositing)
            {
            glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            glColor4f( color.red() / 255.0f, color.green() / 255.0f, color.blue() / 255.0f, alpha );
            glBegin( GL_QUADS );
            foreach( const QRect &r, paintRegion.rects() )
                {
                glVertex2i( r.x(), r.y() );
                glVertex2i( r.x() + r.width(), r.y() );
                glVertex2i( r.x() + r.width(), r.y() + r.height() );
                glVertex2i( r.x(), r.y() + r.height() );
                }
            glEnd();
            glPopAttrib();
            }
#endif

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
        if( effects->compositingType() == XRenderCompositing)
            {
            XRenderColor col;
            col.alpha = int( alpha * 0xffff );
            col.red = int( alpha * 0xffff * color.red() / 255 );
            col.green = int( alpha * 0xffff * color.green() / 255 );
            col.blue= int( alpha * 0xffff * color.blue() / 255 );
            foreach( const QRect &r, paintRegion.rects() )
                XRenderFillRectangle( display(), PictOpOver, effects->xrenderBufferPicture(),
                    &col, r.x(), r.y(), r.width(), r.height());
            }
#endif
        }
    }

void ResizeEffect::windowUserMovedResized( EffectWindow* w, bool first, bool last )
    {
    if( first && last )
        {
        // not interested in maximized
        return;
        }
    if( first && w->isUserResize() && !w->isUserMove() )
        {
        m_active = true;
        m_resizeWindow = w;
        m_originalWindowRect = w->geometry();
        m_currentGeometry = w->geometry();
        w->addRepaintFull();
        }
    if( m_active && w == m_resizeWindow && last )
        {
        m_active = false;
        m_resizeWindow = NULL;
        effects->addRepaintFull();
        }
    }

void ResizeEffect::windowMoveResizeGeometryUpdate( EffectWindow* c, const QRect& geometry )
    {
    if( m_active && c == m_resizeWindow )
        {
        m_currentGeometry = geometry;
        effects->addRepaintFull();
        }
    }

} // namespace
