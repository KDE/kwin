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
//#include "kwinxrenderutils.h"

namespace KWin
{

KWIN_EFFECT( snaphelper, SnapHelperEffect )
KWIN_EFFECT_SUPPORTED( snaphelper, SnapHelperEffect::supported() )

SnapHelperEffect::SnapHelperEffect()
    : m_active( false )
    , m_window( NULL )
    {
    m_timeline.setCurveShape( TimeLine::LinearCurve );
    reconfigure( ReconfigureAll );

    /*if( effects->compositingType() == XRenderCompositing )
        {
        XGCValues gcattr;
        // TODO: Foreground color
        gcattr.line_width = 4;
        m_gc = XCreateGC( display(), rootWindow(), GCLineWidth, &gcattr );
        }*/
    }

SnapHelperEffect::~SnapHelperEffect()
    {
    //if( effects->compositingType() == XRenderCompositing )
    //    XFreeGC( display(), m_gc );
    }

void SnapHelperEffect::reconfigure( ReconfigureFlags )
    {
    m_timeline.setDuration( animationTime( 250 ));
    }

bool SnapHelperEffect::supported()
    {
    return effects->compositingType() == OpenGLCompositing;
    }

void SnapHelperEffect::prePaintScreen( ScreenPrePaintData &data, int time )
    {
    double oldValue = m_timeline.value();
    if( m_active )
        m_timeline.addTime( time );
    else
        m_timeline.removeTime( time );
    if( oldValue != m_timeline.value() )
        effects->addRepaintFull();
    effects->prePaintScreen( data, time );
    }

void SnapHelperEffect::postPaintScreen()
    {
    effects->postPaintScreen();
    if( m_timeline.value() != 0.0 )
        { // Display the guide
        if( effects->compositingType() == OpenGLCompositing )
            {
            glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

            glColor4f( 0.5, 0.5, 0.5, m_timeline.value() * 0.5 );
            glLineWidth( 4.0 ); 
            glBegin( GL_LINES );
                for( int i = 0; i < effects->numScreens(); i++ )
                    {
                    const QRect& rect = effects->clientArea( ScreenArea, i, 0 );
                    int midX = rect.x() + rect.width() / 2;
                    int midY = rect.y() + rect.height() / 2 ;
                    int halfWidth = m_window->width() / 2;
                    int halfHeight = m_window->height() / 2;

                    // Center lines
                    glVertex2f( rect.x() + rect.width() / 2, rect.y() );
                    glVertex2f( rect.x() + rect.width() / 2, rect.y() + rect.height() );
                    glVertex2f( rect.x(), rect.y() + rect.height() / 2 );
                    glVertex2f( rect.x() + rect.width(), rect.y() + rect.height() / 2 );

                    // Window outline
                    // The +/- 2 is to prevent line overlap
                    glVertex2f( midX - halfWidth + 2, midY - halfHeight );
                    glVertex2f( midX + halfWidth + 2, midY - halfHeight );
                    glVertex2f( midX + halfWidth, midY - halfHeight + 2 );
                    glVertex2f( midX + halfWidth, midY + halfHeight + 2 );
                    glVertex2f( midX + halfWidth - 2, midY + halfHeight );
                    glVertex2f( midX - halfWidth - 2, midY + halfHeight );
                    glVertex2f( midX - halfWidth, midY + halfHeight - 2 );
                    glVertex2f( midX - halfWidth, midY - halfHeight - 2 );
                    }
            glEnd();

            glDisable( GL_BLEND );
            glPopAttrib();
            }
        /*if( effects->compositingType() == XRenderCompositing )
            { // TODO
            for( int i = 0; i < effects->numScreens(); i++ )
                {
                const QRect& rect = effects->clientArea( ScreenArea, i, 0 );
                int midX = rect.x() + rect.width() / 2;
                int midY = rect.y() + rect.height() / 2 ;
                int halfWidth = m_window->width() / 2;
                int halfHeight = m_window->height() / 2;

                XSegment segments[6];

                // Center lines
                segments[0].x1 = rect.x() + rect.width() / 2;
                segments[0].y1 = rect.y();
                segments[0].x2 = rect.x() + rect.width() / 2;
                segments[0].y2 = rect.y() + rect.height();
                segments[1].x1 = rect.x();
                segments[1].y1 = rect.y() + rect.height() / 2;
                segments[1].x2 = rect.x() + rect.width();
                segments[1].y2 = rect.y() + rect.height() / 2;

                // Window outline
                // The +/- 2 is to prevent line overlap
                segments[2].x1 = midX - halfWidth + 2;
                segments[2].y1 = midY - halfHeight;
                segments[2].x2 = midX + halfWidth + 2;
                segments[2].y2 = midY - halfHeight;
                segments[3].x1 = midX + halfWidth;
                segments[3].y1 = midY - halfHeight + 2;
                segments[3].x2 = midX + halfWidth;
                segments[3].y2 = midY + halfHeight + 2;
                segments[4].x1 = midX + halfWidth - 2;
                segments[4].y1 = midY + halfHeight;
                segments[4].x2 = midX - halfWidth - 2;
                segments[4].y2 = midY + halfHeight;
                segments[5].x1 = midX - halfWidth;
                segments[5].y1 = midY + halfHeight - 2;
                segments[5].x2 = midX - halfWidth;
                segments[5].y2 = midY - halfHeight - 2;

                XDrawSegments( display(), effects->xrenderBufferPicture(), m_gc, segments, 6 );
                }
            }*/
        }
    else if( m_window )
        {
        if( m_window->isDeleted() )
            m_window->unrefWindow();
        m_window = NULL;
        }
    }

void SnapHelperEffect::windowClosed( EffectWindow* w )
    {
    if( m_window == w )
        {
        m_window->refWindow();
        m_active = false;
        }
    }

void SnapHelperEffect::windowUserMovedResized( EffectWindow* w, bool first, bool last )
    {
    if( first && !last && w->isMovable() )
        {
        m_active = true;
        m_window = w;
        effects->addRepaintFull();
        }
    else if( last )
        m_active = false;
    }

} // namespace
