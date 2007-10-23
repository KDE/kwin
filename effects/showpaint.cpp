/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "showpaint.h"

#include <config-X11.h>

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#endif
#ifdef HAVE_XRENDER
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
#ifdef HAVE_OPENGL
    if( effects->compositingType() == OpenGLCompositing)
        paintGL();
#endif
#ifdef HAVE_XRENDER
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
#ifdef HAVE_OPENGL
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
#ifdef HAVE_XRENDER
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
