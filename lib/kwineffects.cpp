/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "kwineffects.h"

#include <QStringList>

#include "kdebug.h"

#include <assert.h>

namespace KWin
{

WindowPaintData::WindowPaintData()
    : opacity( 1.0 )
    , xScale( 1 )
    , yScale( 1 )
    , xTranslate( 0 )
    , yTranslate( 0 )
    , saturation( 1 )
    , brightness( 1 )
    {
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

void Effect::prePaintScreen( int* mask, QRegion* region, int time )
    {
    effects->prePaintScreen( mask, region, time );
    }

void Effect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    effects->paintScreen( mask, region, data );
    }
    
void Effect::postPaintScreen()
    {
    effects->postPaintScreen();
    }

void Effect::prePaintWindow( EffectWindow* w, int* mask, QRegion* paint, QRegion* clip, int time )
    {
    effects->prePaintWindow( w, mask, paint, clip, time );
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


//****************************************
// EffectWindowGroup
//****************************************

EffectWindowGroup::~EffectWindowGroup()
    {
    }


} // namespace
