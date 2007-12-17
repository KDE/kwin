/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

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

#include "showpaint.h"

#include <kwinconfig.h>

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include <GL/gl.h>
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#endif

#include <math.h>

#include <qcolor.h>

namespace KWin
{

KWIN_EFFECT( showpaint, ShowPaintEffect )

static QColor colors[] = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta,
    Qt::yellow, Qt::gray };

ShowPaintEffect::ShowPaintEffect()
    : color_index( 0 )
    {
    }

void ShowPaintEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    painted = QRegion();
    effects->paintScreen( mask, region, data );
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing)
        paintGL();
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing)
        paintXrender();
#endif
    if( ++color_index == sizeof( colors ) / sizeof( colors[ 0 ] ))
        color_index = 0;
    }

void ShowPaintEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    painted |= region;
    effects->paintWindow( w, mask, region, data );
    }

// TODO I think we need some kind of generic paintRect()
void ShowPaintEffect::paintGL()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    float alpha = 0.2;
    const QColor& color = colors[ color_index ];
    glColor4f( color.red() / 255., color.green() / 255., color.blue() / 255., alpha );
    glBegin( GL_QUADS );
    foreach( QRect r, painted.rects())
        {
        glVertex2i( r.x(), r.y());
        glVertex2i( r.x() + r.width(), r.y());
        glVertex2i( r.x() + r.width(), r.y() + r.height());
        glVertex2i( r.x(), r.y() + r.height());
        }
    glEnd();
    glPopAttrib();
#endif
    }

void ShowPaintEffect::paintXrender()
    {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    XRenderColor col;
    int alpha = 0.2;
    const QColor& color = colors[ color_index ];
    col.alpha = int( alpha * 0xffff );
    col.red = int( alpha * 0xffff * color.red() / 255 );
    col.green = int( alpha * 0xffff * color.green() / 255 );
    col.blue= int( alpha * 0xffff * color.blue() / 255 );
    foreach( QRect r, painted.rects())
        XRenderFillRectangle( display(), PictOpOver, effects->xrenderBufferPicture(),
            &col, r.x(), r.y(), r.width(), r.height());
#endif
    }

} // namespace
