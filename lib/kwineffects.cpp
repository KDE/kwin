/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

#include "kwineffects.h"

#include "kwinglutils.h"
#include "kwinxrenderutils.h"

#include <QtDBus/QtDBus>
#include <QVariant>
#include <QList>
#include <QtCore/QTimeLine>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>

#include <kdebug.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>

#include <assert.h>

#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xfixes.h>
#endif

namespace KWin
{

void WindowPrePaintData::setTranslucent()
    {
    mask |= Effect::PAINT_WINDOW_TRANSLUCENT;
    mask &= ~Effect::PAINT_WINDOW_OPAQUE;
    clip = QRegion(); // cannot clip, will be transparent
    }

void WindowPrePaintData::setTransformed()
    {
    mask |= Effect::PAINT_WINDOW_TRANSFORMED;
    }


WindowPaintData::WindowPaintData( EffectWindow* w )
    : opacity( w->opacity())
    , contents_opacity( 1.0 )
    , decoration_opacity( 1.0 )
    , xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    , saturation( 1 )
    , brightness( 1 )
    , shader( NULL )
    {
    quads = w->buildQuads();
    }

ScreenPaintData::ScreenPaintData()
    : xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    {
    }

//****************************************
// Effect
//****************************************

Effect::Effect()
    {
    }

Effect::~Effect()
    {
    }

void Effect::windowUserMovedResized( EffectWindow* , bool, bool )
    {
    }

void Effect::windowOpacityChanged( EffectWindow*, double )
    {
    }

void Effect::windowAdded( EffectWindow* )
    {
    }

void Effect::windowClosed( EffectWindow* )
    {
    }

void Effect::windowDeleted( EffectWindow* )
    {
    }

void Effect::windowActivated( EffectWindow* )
    {
    }

void Effect::windowMinimized( EffectWindow* )
    {
    }

void Effect::windowUnminimized( EffectWindow* )
    {
    }

void Effect::windowInputMouseEvent( Window, QEvent* )
    {
    }

void Effect::grabbedKeyboardEvent( QKeyEvent* )
    {
    }

void Effect::propertyNotify( EffectWindow* , long )
    {
    }

void Effect::desktopChanged( int )
    {
    }

void Effect::windowDamaged( EffectWindow*, const QRect& )
    {
    }

void Effect::windowGeometryShapeChanged( EffectWindow*, const QRect& )
    {
    }

void Effect::tabBoxAdded( int )
    {
    }

void Effect::tabBoxClosed()
    {
    }

void Effect::tabBoxUpdated()
    {
    }
bool Effect::borderActivated( ElectricBorder )
    {
    return false;
    }

void Effect::mouseChanged( const QPoint&, const QPoint&, Qt::MouseButtons,
    Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers )
    {
    }

void Effect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    effects->prePaintScreen( data, time );
    }

void Effect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    }
    
void Effect::postPaintScreen()
    {
    effects->postPaintScreen();
    }

void Effect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    effects->prePaintWindow( w, data, time );
    }

void Effect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->paintWindow( w, mask, region, data );
    }

void Effect::postPaintWindow( EffectWindow* w )
    {
    effects->postPaintWindow( w );
    }

void Effect::drawWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    effects->drawWindow( w, mask, region, data );
    }

QRect Effect::transformWindowDamage( EffectWindow* w, const QRect& r )
    {
    return effects->transformWindowDamage( w, r );
    }

void Effect::setPositionTransformations( WindowPaintData& data, QRect& region, EffectWindow* w,
    const QRect& r, Qt::AspectRatioMode aspect )
    {
    QSize size = w->size();
    size.scale( r.size(), aspect );
    data.xScale = size.width() / double( w->width());
    data.yScale = size.height() / double( w->height());
    int width = int( w->width() * data.xScale );
    int height = int( w->height() * data.yScale );
    int x = r.x() + ( r.width() - width ) / 2;
    int y = r.y() + ( r.height() - height ) / 2;
    region = QRect( x, y, width, height );
    data.xTranslate = x - w->x();
    data.yTranslate = y - w->y();
    }

int Effect::displayWidth()
    {
    return KWin::displayWidth();
    }

int Effect::displayHeight()
    {
    return KWin::displayHeight();
    }

QPoint Effect::cursorPos()
    {
    return effects->cursorPos();
    }

//****************************************
// EffectsHandler
//****************************************

EffectsHandler::EffectsHandler(CompositingType type)
    : current_paint_screen( 0 )
    , current_paint_window( 0 )
    , current_draw_window( 0 )
    , current_transform( 0 )
    , compositing_type( type )
    {
    if( compositing_type == NoCompositing )
        return;
    KWin::effects = this;
    }

EffectsHandler::~EffectsHandler()
    {
    // All effects should already be unloaded by Impl dtor
    assert( loaded_effects.count() == 0 );
    }

QRect EffectsHandler::transformWindowDamage( EffectWindow* w, const QRect& r )
    {
    if( current_transform < loaded_effects.size())
        {
        QRect rr = loaded_effects[current_transform++].second->transformWindowDamage( w, r );
        --current_transform;
        return rr;
        }
    else
        return r;
    }

Window EffectsHandler::createInputWindow( Effect* e, const QRect& r, const QCursor& cursor )
    {
    return createInputWindow( e, r.x(), r.y(), r.width(), r.height(), cursor );
    }

Window EffectsHandler::createFullScreenInputWindow( Effect* e, const QCursor& cursor )
    {
    return createInputWindow( e, 0, 0, displayWidth(), displayHeight(), cursor );
    }

CompositingType EffectsHandler::compositingType() const
    {
    return compositing_type;
    }

bool EffectsHandler::saturationSupported() const
    {
    switch( compositing_type )
        {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
        case OpenGLCompositing:
            return GLTexture::saturationSupported();
#endif
        case XRenderCompositing:
            return false; // never
        default:
            abort();
        }
    }

void EffectsHandler::sendReloadMessage( const QString& effectname )
    {
    QDBusMessage message = QDBusMessage::createMethodCall("org.kde.kwin", "/KWin", "org.kde.KWin", "reloadEffect");
    message << QString("kwin4_effect_" + effectname);
    QDBusConnection::sessionBus().send(message);
    }

KConfigGroup EffectsHandler::effectConfig( const QString& effectname )
    {
    KSharedConfig::Ptr kwinconfig = KSharedConfig::openConfig( "kwinrc", KConfig::NoGlobals );
    return kwinconfig->group( "Effect-" + effectname );
    }

bool EffectsHandler::paintText( const QString& text, const QPoint& center, int maxwidth,
        const QColor& color, const QFont& font )
{
    QPainter p;
    // Calculate size of the text
    QFontMetrics fm( font );
    QString painttext = fm.elidedText( text, Qt::ElideRight, maxwidth );
    QRect textrect = fm.boundingRect( painttext );

    // Create temporary QPixmap where the text will be drawn onto
    QPixmap textPixmap( textrect.width(), textrect.height());
    textPixmap.fill( Qt::transparent );

    // Draw the text
    p.begin( &textPixmap );
    p.setFont( font );
    p.setRenderHint( QPainter::TextAntialiasing );
    p.setPen( color );
    p.drawText( -textrect.topLeft(), painttext );
    p.end();

    // Area covered by text
    QRect area( center.x() - textrect.width() / 2, center.y() - textrect.height() / 2,
                 textrect.width(), textrect.height() );

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        {
        GLTexture textTexture( textPixmap, GL_TEXTURE_RECTANGLE_ARB );
        glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        textTexture.bind();
        const float verts[ 4 * 2 ] =
            {
            area.x(), area.y(),
            area.x(), area.y() + area.height(),
            area.x() + area.width(), area.y() + area.height(),
            area.x() + area.width(), area.y()
            };
        const float texcoords[ 4 * 2 ] =
            {
            0, textPixmap.height(),
            0, 0,
            textPixmap.width(), 0,
            textPixmap.width(), textPixmap.height()
            };
        renderGLGeometry( 4, verts, texcoords );
        textTexture.unbind();
        glPopAttrib();
        return true;
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        {
        XRenderPicture textPicture( textPixmap );
        XRenderComposite( display(), textPixmap.depth() == 32 ? PictOpOver : PictOpSrc,
            textPicture, None, effects->xrenderBufferPicture(),
            0, 0, 0, 0, area.x(), area.y(), area.width(), area.height());
        return true;
        }
#endif
    return false;
}

bool EffectsHandler::paintTextWithBackground( const QString& text, const QPoint& center, int maxwidth,
        const QColor& color, const QColor& bgcolor, const QFont& font )
{
    // Calculate size of the text
    QFontMetrics fm( font );
    QString painttext = fm.elidedText( text, Qt::ElideRight, maxwidth );
    QRect textrect = fm.boundingRect( painttext );

    // Area covered by text
    QRect area( center.x() - textrect.width() / 2, center.y() - textrect.height() / 2,
                textrect.width(), textrect.height() );

#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
    {
        glColor4f( bgcolor.redF(), bgcolor.greenF(), bgcolor.blueF(), bgcolor.alphaF() );
        renderRoundBox( area.adjusted( -8, -3, 8, 3 ), 5 );

        return paintText( text, center, maxwidth, color, font );
    }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
    {
        xRenderRoundBox( effects->xrenderBufferPicture(), area.adjusted( -8, -3, 8, 3 ), 5, bgcolor );
        return paintText( text, center, maxwidth, color, font );
    }
#endif
    return false;
}


EffectsHandler* effects = 0;


//****************************************
// EffectWindow
//****************************************

EffectWindow::EffectWindow()
    {
    }

EffectWindow::~EffectWindow()
    {
    }

bool EffectWindow::isOnCurrentDesktop() const
    {
    return isOnDesktop( effects->currentDesktop());
    }

bool EffectWindow::isOnDesktop( int d ) const
    {
    return desktop() == d || isOnAllDesktops();
    }

bool EffectWindow::hasDecoration() const
    {
    return contentsRect() != QRect( 0, 0, width(), height());
    }


//****************************************
// EffectWindowGroup
//****************************************

EffectWindowGroup::~EffectWindowGroup()
    {
    }

//****************************************
// GlobalShortcutsEditor
//****************************************

GlobalShortcutsEditor::GlobalShortcutsEditor( QWidget *parent ) :
        KShortcutsEditor( parent, GlobalAction )
    {
    }

/***************************************************************
 WindowQuad
***************************************************************/

WindowQuad WindowQuad::makeSubQuad( double x1, double y1, double x2, double y2 ) const
    {
    assert( x1 < x2 && y1 < y2 && x1 >= left() && x2 <= right() && y1 >= top() && y2 <= bottom());
#ifndef NDEBUG
    if( isTransformed())
        kFatal( 1212 ) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
    WindowQuad ret( *this );
    // vertices are clockwise starting from topleft
    ret.verts[ 0 ].px = x1;
    ret.verts[ 3 ].px = x1;
    ret.verts[ 1 ].px = x2;
    ret.verts[ 2 ].px = x2;
    ret.verts[ 0 ].py = y1;
    ret.verts[ 1 ].py = y1;
    ret.verts[ 2 ].py = y2;
    ret.verts[ 3 ].py = y2;
    // original x/y are supposed to be the same, no transforming is done here
    ret.verts[ 0 ].ox = x1;
    ret.verts[ 3 ].ox = x1;
    ret.verts[ 1 ].ox = x2;
    ret.verts[ 2 ].ox = x2;
    ret.verts[ 0 ].oy = y1;
    ret.verts[ 1 ].oy = y1;
    ret.verts[ 2 ].oy = y2;
    ret.verts[ 3 ].oy = y2;
    double my_tleft = verts[ 0 ].tx;
    double my_tright = verts[ 2 ].tx;
    double my_ttop = verts[ 0 ].ty;
    double my_tbottom = verts[ 2 ].ty;
    double tleft = ( x1 - left()) / ( right() - left()) * ( my_tright - my_tleft ) + my_tleft;
    double tright = ( x2 - left()) / ( right() - left()) * ( my_tright - my_tleft ) + my_tleft;
    double ttop = ( y1 - top()) / ( bottom() - top()) * ( my_tbottom - my_ttop ) + my_ttop;
    double tbottom = ( y2 - top()) / ( bottom() - top()) * ( my_tbottom - my_ttop ) + my_ttop;
    ret.verts[ 0 ].tx = tleft;
    ret.verts[ 3 ].tx = tleft;
    ret.verts[ 1 ].tx = tright;
    ret.verts[ 2 ].tx = tright;
    ret.verts[ 0 ].ty = ttop;
    ret.verts[ 1 ].ty = ttop;
    ret.verts[ 2 ].ty = tbottom;
    ret.verts[ 3 ].ty = tbottom;
    return ret;
    }

bool WindowQuad::smoothNeeded() const
    {
    // smoothing is needed if the width or height of the quad does not match the original size
    double width = verts[ 1 ].ox - verts[ 0 ].ox;
    double height = verts[ 2 ].oy - verts[ 1 ].oy;
    return( verts[ 1 ].px - verts[ 0 ].px != width || verts[ 2 ].px - verts[ 3 ].px != width
        || verts[ 2 ].py - verts[ 1 ].py != height || verts[ 3 ].py - verts[ 0 ].py != height );
    }

/***************************************************************
 WindowQuadList
***************************************************************/

WindowQuadList WindowQuadList::splitAtX( double x ) const
    {
    WindowQuadList ret;
    foreach( WindowQuad quad, *this )
        {
#ifndef NDEBUG
        if( quad.isTransformed())
            kFatal( 1212 ) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        bool wholeleft = true;
        bool wholeright = true;
        for( int i = 0;
             i < 4;
             ++i )
            {
            if( quad[ i ].x() < x )
                wholeright = false;
            if( quad[ i ].x() >= x )
                wholeleft = false;
            }
        if( wholeleft || wholeright ) // is whole in one split part
            {
            ret.append( quad );
            continue;
            }
        ret.append( quad.makeSubQuad( quad.left(), quad.top(), x, quad.bottom()));
        ret.append( quad.makeSubQuad( x, quad.top(), quad.right(), quad.bottom()));
        }
    return ret;
    }

WindowQuadList WindowQuadList::splitAtY( double y ) const
    {
    WindowQuadList ret;
    foreach( WindowQuad quad, *this )
        {
#ifndef NDEBUG
        if( quad.isTransformed())
            kFatal( 1212 ) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        bool wholetop = true;
        bool wholebottom = true;
        for( int i = 0;
             i < 4;
             ++i )
            {
            if( quad[ i ].y() < y )
                wholebottom = false;
            if( quad[ i ].y() >= y )
                wholetop = false;
            }
        if( wholetop || wholebottom ) // is whole in one split part
            {
            ret.append( quad );
            continue;
            }
        ret.append( quad.makeSubQuad( quad.left(), quad.top(), quad.right(), y ));
        ret.append( quad.makeSubQuad( quad.left(), y, quad.right(), quad.bottom()));
        }
    return ret;
    }

WindowQuadList WindowQuadList::makeGrid( int maxquadsize ) const
    {
    if( empty())
        return *this;
    // find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();
    foreach( WindowQuad quad, *this )
        {
#ifndef NDEBUG
        if( quad.isTransformed())
            kFatal( 1212 ) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        left = qMin( left, quad.left());
        right = qMax( right, quad.right());
        top = qMin( top, quad.top());
        bottom = qMax( bottom, quad.bottom());
        }
    WindowQuadList ret;
    for( double x = left;
         x < right;
         x += maxquadsize )
        {
        for( double y = top;
             y < bottom;
             y += maxquadsize )
            {
            foreach( WindowQuad quad, *this )
                {
                if( QRectF( QPointF( quad.left(), quad.top()), QPointF( quad.right(), quad.bottom()))
                    .intersects( QRectF( x, y, maxquadsize, maxquadsize )))
                    {
                    ret.append( quad.makeSubQuad( qMax( x, quad.left()), qMax( y, quad.top()),
                        qMin( quad.right(), x + maxquadsize ), qMin( quad.bottom(), y + maxquadsize )));
                    }
                }
            }
        }
    return ret;
    }

WindowQuadList WindowQuadList::makeRegularGrid( int xSubdivisions, int ySubdivisions ) const
    {
    if( empty())
        return *this;
    // find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();
    foreach( WindowQuad quad, *this )
        {
#ifndef NDEBUG
        if( quad.isTransformed())
            kFatal( 1212 ) << "Splitting quads is allowed only in pre-paint calls!" ;
#endif
        left = qMin( left, quad.left());
        right = qMax( right, quad.right());
        top = qMin( top, quad.top());
        bottom = qMax( bottom, quad.bottom());
        }

    double xincrement = (right - left) / xSubdivisions;
    double yincrement = (bottom - top) / ySubdivisions;
    WindowQuadList ret;
    for( double y = top;
         y < bottom;
         y += yincrement )
        {
        for( double x = left;
             x < right;
             x +=  xincrement)
            {
            foreach( WindowQuad quad, *this )
                {
                if( QRectF( QPointF( quad.left(), quad.top()), QPointF( quad.right(), quad.bottom()))
                    .intersects( QRectF( x, y, xincrement, yincrement )))
                    {
                    ret.append( quad.makeSubQuad( qMax( x, quad.left()), qMax( y, quad.top()),
                        qMin( quad.right(), x + xincrement ), qMin( quad.bottom(), y + yincrement )));
                    }
                }
            }
        }
    return ret;
    }

void WindowQuadList::makeArrays( float** vertices, float** texcoords ) const
    {
    *vertices = new float[ count() * 4 * 2 ];
    *texcoords = new float[ count() * 4 * 2 ];
    float* vpos = *vertices;
    float* tpos = *texcoords;
    for( int i = 0;
         i < count();
         ++i )
        for( int j = 0;
             j < 4;
             ++j )
            {
            *vpos++ = at( i )[ j ].x();
            *vpos++ = at( i )[ j ].y();
            *tpos++ = at( i )[ j ].tx;
            *tpos++ = at( i )[ j ].ty;
            }
    }

WindowQuadList WindowQuadList::select( WindowQuadType type ) const
    {
    foreach( WindowQuad q, *this )
        {
        if( q.type != type ) // something else than ones to select, make a copy and filter
            {
            WindowQuadList ret;
            foreach( WindowQuad q, *this )
                {
                if( q.type == type )
                    ret.append( q );
                }
            return ret;
            }
        }
    return *this; // nothing to filter out
    }

WindowQuadList WindowQuadList::filterOut( WindowQuadType type ) const
    {
    foreach( WindowQuad q, *this )
        {
        if( q.type == type ) // something to filter out, make a copy and filter
            {
            WindowQuadList ret;
            foreach( WindowQuad q, *this )
                {
                if( q.type != type )
                    ret.append( q );
                }
            return ret;
            }
        }
    return *this; // nothing to filter out
    }

bool WindowQuadList::smoothNeeded() const
    {
    foreach( WindowQuad q, *this )
        if( q.smoothNeeded())
            return true;
    return false;
    }

/***************************************************************
 PaintClipper
***************************************************************/

QStack< QRegion >* PaintClipper::areas = NULL;

PaintClipper::PaintClipper( const QRegion& allowed_area )
    : area( allowed_area )
    {
    push( area );
    }

PaintClipper::~PaintClipper()
    {
    pop( area );
    }

void PaintClipper::push( const QRegion& allowed_area )
    {
    if( allowed_area == infiniteRegion()) // don't push these
        return;
    if( areas == NULL )
        areas = new QStack< QRegion >;
    areas->push( allowed_area );
    }

void PaintClipper::pop( const QRegion& allowed_area )
    {
    if( allowed_area == infiniteRegion())
        return;
    Q_ASSERT( areas != NULL );
    Q_ASSERT( areas->top() == allowed_area );
    areas->pop();
    if( areas->isEmpty())
        {
        delete areas;
        areas = NULL;
        }
    }

bool PaintClipper::clip()
    {
    return areas != NULL;
    }

QRegion PaintClipper::paintArea()
    {
    assert( areas != NULL ); // can be called only with clip() == true
    QRegion ret = QRegion( 0, 0, displayWidth(), displayHeight());
    foreach( QRegion r, *areas )
        ret &= r;
    return ret;
    }

struct PaintClipper::Iterator::Data
    {
    Data() : index( 0 ) {}
    int index;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    QVector< QRect > rects;
#endif
    };

PaintClipper::Iterator::Iterator()
    : data( new Data )
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( clip() && effects->compositingType() == OpenGLCompositing )
        {
        glPushAttrib( GL_SCISSOR_BIT );
        glEnable( GL_SCISSOR_TEST );
        data->rects = paintArea().rects();
        data->index = -1;
        next(); // move to the first one
        }
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( clip() && effects->compositingType() == XRenderCompositing )
        {
        XserverRegion region = toXserverRegion( paintArea());
        XFixesSetPictureClipRegion( display(), effects->xrenderBufferPicture(), 0, 0, region );
        XFixesDestroyRegion( display(), region ); // it's ref-counted
        }
#endif
    }

PaintClipper::Iterator::~Iterator()
    {
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( clip() && effects->compositingType() == OpenGLCompositing )
        glPopAttrib();
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( clip() && effects->compositingType() == XRenderCompositing )
        XFixesSetPictureClipRegion( display(), effects->xrenderBufferPicture(), 0, 0, None );
#endif
    delete data;
    }

bool PaintClipper::Iterator::isDone()
    {
    if( !clip())
        return data->index == 1; // run once
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        return data->index >= data->rects.count(); // run once per each area
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        return data->index == 1; // run once
#endif
    abort();
    }

void PaintClipper::Iterator::next()
    {
    data->index++;
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( clip() && effects->compositingType() == OpenGLCompositing && data->index < data->rects.count())
        {
        const QRect& r = data->rects[ data->index ];
        // Scissor rect has to be given in OpenGL coords
        glScissor( r.x(), displayHeight() - r.y() - r.height(), r.width(), r.height());
        }
#endif
    }

QRect PaintClipper::Iterator::boundingRect() const
    {
    if( !clip())
        return infiniteRegion();
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
    if( effects->compositingType() == OpenGLCompositing )
        return data->rects[ data->index ];
#endif
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
    if( effects->compositingType() == XRenderCompositing )
        return paintArea().boundingRect();
#endif
    abort();
    }


/***************************************************************
 TimeLine
***************************************************************/

TimeLine::TimeLine(const int duration)
    {
    m_Time = 0;
    m_CurveShape = TimeLine::EaseInCurve;
    m_Duration = duration;
    m_TimeLine = new QTimeLine(m_Duration);
    m_TimeLine->setFrameRange(0, m_Duration);
    m_TimeLine->setCurveShape(QTimeLine::EaseInCurve);
    }

TimeLine::TimeLine(const TimeLine &other)
    {
    m_Time = other.m_Time;
    m_CurveShape = other.m_CurveShape;
    m_Duration = other.m_Duration;
    m_TimeLine = new QTimeLine(m_Duration);
    m_TimeLine->setFrameRange(0, m_Duration);
    setProgress(m_Progress);
    setCurveShape(m_CurveShape);
    }

TimeLine::~TimeLine()
    {
    delete m_TimeLine;
    }

int TimeLine::duration() const
    {
    return m_Duration;
    }

void TimeLine::setDuration(const int msec)
    {
    m_Duration = msec;
    m_TimeLine->setDuration(msec);
    }

double TimeLine::value() const
    {
    return valueForTime(m_Time);
    }

double TimeLine::valueForTime(const int msec) const
    {
    // Catch non QTimeLine CurveShapes here, (but there are none right now)


    // else use QTimeLine ...
    return m_TimeLine->valueForTime(msec);
    }

void TimeLine::addTime(const int msec)
    {
    m_Time = qMin(m_Duration, m_Time + msec);
    m_Progress = (double)m_Time / m_Duration;
    }

void TimeLine::removeTime(const int msec)
    {
    m_Time = qMax(0, m_Time - msec);
    m_Progress = (double)m_Time / m_Duration;
    }

void TimeLine::setProgress(const double progress)
    {
    m_Progress = progress;
    m_Time = qRound(m_Duration * progress);
    }

double TimeLine::progress() const
    {
    return m_Progress;
    }

int TimeLine::time() const
    {
    return m_Time;
    }

void TimeLine::addProgress(const double progress)
    {
    m_Progress += progress;
    m_Time = (int)(m_Duration * m_Progress);
    }

void TimeLine::setCurveShape(CurveShape curveShape)
    {
    switch (curveShape)
        {
        case EaseInCurve:
            m_TimeLine->setCurveShape(QTimeLine::EaseInCurve);
            break;
        case EaseOutCurve:
            m_TimeLine->setCurveShape(QTimeLine::EaseOutCurve);
            break;
        case EaseInOutCurve:
            m_TimeLine->setCurveShape(QTimeLine::EaseInOutCurve);
            break;
        case LinearCurve:
            m_TimeLine->setCurveShape(QTimeLine::LinearCurve);
            break;
        case SineCurve:
            m_TimeLine->setCurveShape(QTimeLine::SineCurve);
            break;
        }
        m_CurveShape = curveShape;
    }

} // namespace
