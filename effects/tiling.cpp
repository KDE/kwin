/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2008 Lucas Murray <lmurray@undefinedfire.com>

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

#include "tiling.h"

#include <math.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <klocale.h>

namespace KWin
{

KWIN_EFFECT( tiling, TilingEffect )

//-----------------------------------------------------------------------------
// TilingEffect

QHash<EffectWindow*, TilingWindow*> TilingEffect::m_tilingData;

TilingEffect::TilingEffect()
    : m_tilingMode( TilingModeNone )
    {
    // Load shortcuts
    KActionCollection* actionCollection = new KActionCollection( this );

    KAction* a = (KAction*) actionCollection->addAction( "TilingAdd" );
    a->setText( i18n( "Tiling - Add to frame" ));
    a->setGlobalShortcut( KShortcut( Qt::ALT + Qt::Key_A ));
    connect( a, SIGNAL( triggered(bool) ), this, SLOT( frameAdd() ));

    KAction* b = ( KAction* )actionCollection->addAction( "TilingSplit" );
    b->setText( i18n( "Tiling - Add to split frame" ));
    b->setGlobalShortcut( KShortcut( Qt::ALT + Qt::Key_S ));
    connect( b, SIGNAL( triggered(bool) ), this, SLOT( frameSplitAdd() ));

    reconfigure( ReconfigureAll );
    }

void TilingEffect::reconfigure( ReconfigureFlags )
    {
    // Add all existing windows to the window list
    // TODO: Enabling desktop effects should trigger windowAdded() on all windows
    m_tilingData.clear();
    foreach( EffectWindow *w, effects->stackingOrder() )
        m_tilingData[w] = new TilingWindow( w );
    }

void TilingEffect::windowAdded( EffectWindow *w )
    {
    m_tilingData[w] = new TilingWindow( w );
    }

void TilingEffect::windowClosed( EffectWindow *w )
    {
    delete m_tilingData[w];
    m_tilingData.remove( w );
    }

void TilingEffect::windowUserMovedResized( EffectWindow *w, bool first, bool last )
    {
    // Resize all other windows on the tiling surface
    if( m_tilingData[w]->isTiled() )
        {
        if( w->size() == m_tilingData[w]->tilingFrame()->size() )
            { // User moved without resizing, undo
            if( w->pos() != m_tilingData[w]->tilingFrame()->topLeft() );
                effects->moveWindow( w, m_tilingData[w]->tilingFrame()->topLeft() );
            return;
            }
        m_tilingData[w]->tilingFrame()->surface()->resizedWindow( w );
        }

    // We don't do this here as shortcuts do not activate while moving a window
    //if( isInTilingSwitch() && last ) // Add the window to the tiling grid on button release
    //    doTilingSwitch( w );//, QPoint( x_root, y_root ));
    }

void TilingEffect::frameAdd()
    {
    EffectWindow *w = effects->activeWindow();
    if( m_tilingData[w]->isTiled() )
        {
        kDebug( 1212 ) << "Removed window from tiling surface";
        TilingSurface *surface = TilingSurface::getOrMakeSurface( w->desktop(), w->screen() );
        surface->removeFromSurface( w, TilingModeAddRemove );
        }
    else
        {
        kDebug( 1212 ) << "Entered tiling mode add/remove";
        enterTilingMode( TilingModeAddRemove );
        doTilingSwitch( w ); // TODO: This should be in windowUserMovedResized()
        }
    }

void TilingEffect::frameSplitAdd()
    {
    EffectWindow *w = effects->activeWindow();
    if( m_tilingData[w]->isTiled() )
        {
        kDebug( 1212 ) << "Removed and merged window from tiling surface";
        TilingSurface *surface = TilingSurface::getOrMakeSurface( w->desktop(), w->screen() );
        surface->removeFromSurface( w, TilingModeSplitMerge );
        }
    else
        {
        kDebug( 1212 ) << "Entered tiling mode split/merge";
        enterTilingMode( TilingModeSplitMerge );
        doTilingSwitch( w ); // TODO: This should be in windowUserMovedResized()
        }
    }

void TilingEffect::doTilingSwitch( EffectWindow *w )//, QPoint mousePos )
    {
    if( !isInTilingSwitch() )
        return;
    TilingSurface* surface = TilingSurface::getOrMakeSurface( w->desktop(), w->screen() );
    //mousePos -= workspace()->screenGeometry( workspace()->screenNumber( mousePos )).topLeft(); // Position relative to the screen
    surface->addToSurface( effects->activeWindow(), /*mousePos*/w->geometry().center(), m_tilingMode );
    exitTilingMode();
    }

//-----------------------------------------------------------------------------
// TilingSurface

QList<TilingSurface*> TilingSurface::m_surfaces;

TilingSurface::TilingSurface( int desktop, int screen )
    : m_desktop( desktop )
    , m_screen( screen )
    , m_frames()
    , m_windows()
    {
    // Always at least one frame on a surface
    TilingFrame *frame = new TilingFrame( this, geometry() );
    m_frames.append( frame );
    }

TilingSurface* TilingSurface::getSurface( int desktop, int screen )
    {
    foreach( TilingSurface *surface, m_surfaces )
        if( surface->desktop() == desktop && surface->screen() == screen )
            return surface;
    return NULL; // Doesn't exist
    }

TilingSurface* TilingSurface::makeSurface( int desktop, int screen )
    {
    TilingSurface *surface = new TilingSurface( desktop, screen );
    m_surfaces.append( surface );
    return surface;
    }

TilingSurface* TilingSurface::getOrMakeSurface( int desktop, int screen )
    {
    TilingSurface *surface = getSurface( desktop, screen );
    if( surface != NULL )
        return surface;
    return makeSurface( desktop, screen );
    }

void TilingSurface::addToSurface( EffectWindow *window, QPoint pos, TilingMode mode )
    {
    if( !m_windows.contains( window ))
        m_windows.append( window );
    TilingFrame *frame = getFrame( pos );

    if( mode == TilingModeAddRemove && m_frames.count() < 2)
        mode = TilingModeSplitMerge; // Never allow only one filled frame on a surface
    switch( mode )
        {
        default:
            assert( false );
        case TilingModeAddRemove:
            kDebug( 1212 ) << "Doing add/remove tiling mode";
            TilingEffect::m_tilingData[window]->setTilingFrame( frame );
            effects->setWindowGeometry( window, QRect( *frame )); // TODO: setGeometry() goes here, or in setTilingFrame()?
            break;
        case TilingModeSplitMerge:
            kDebug( 1212 ) << "Doing split/merge tiling mode";
            TilingFrame *newFrame;

            // Work out which side the cursor is closest to
            int side; // Top, right, bottom, left
            float x = float( pos.x() - frame->x() ) / float( frame->width() );
            float y = float( pos.y() - frame->y() ) / float( frame->height() );
            if( x < 0.5f && y < 0.5f )
                { // Top left
                if( x > y )
                    side = 0;
                else
                    side = 3;
                }
            else if( x > 0.5f && y < 0.5f )
                { // Top right
                if( 1.0f - x > y )
                    side = 0;
                else
                    side = 1;
                }
            else if( x < 0.5f && y > 0.5f )
                { // Bottom left
                if( x > 1.0f - y )
                    side = 2;
                else
                    side = 3;
                }
            else if( x > 0.5f && y > 0.5f )
                { // Bottom right
                if( x > y )
                    side = 1;
                else
                    side = 2;
                }

            switch( side )
                {
                case 0: // Top
                    newFrame = new TilingFrame( this,
                        QRect( frame->x(),
                               frame->y(),
                               frame->width(),
                               frame->height() / 2 )
                        );
                    frame->setGeometry(
                        QRect( frame->x(),
                               frame->y() + newFrame->height(),
                               frame->width(),
                               frame->height() - newFrame->height() )
                        );
                    break;
                case 1: // Right
                    newFrame = new TilingFrame( this,
                        // As we are using integer division odd widths divide unevenly
                        QRect( frame->x() + frame->width() - frame->width() / 2,
                               frame->y(),
                               frame->width() / 2,
                               frame->height() )
                        );
                    frame->setGeometry(
                        QRect( frame->x(),
                               frame->y(),
                               frame->width() - newFrame->width(),
                               frame->height() )
                        );
                    break;
                case 2: // Bottom
                    newFrame = new TilingFrame( this,
                        // As we are using integer division odd heights divide unevenly
                        QRect( frame->x(),
                               frame->y() + frame->height() - frame->height() / 2,
                               frame->width(),
                               frame->height() / 2 )
                        );
                    frame->setGeometry(
                        QRect( frame->x(),
                               frame->y(),
                               frame->width(),
                               frame->height() - newFrame->height() )
                        );
                    break;
                case 3: // Left
                    newFrame = new TilingFrame( this,
                        QRect( frame->x(),
                               frame->y(),
                               frame->width() / 2,
                               frame->height() )
                        );
                    frame->setGeometry(
                        QRect( frame->x() + newFrame->width(),
                               frame->y(),
                               frame->width() - newFrame->width(),
                               frame->height() )
                        );
                    break;
                }

            m_frames.append( newFrame );
            TilingEffect::m_tilingData[window]->setTilingFrame( newFrame );
            effects->setWindowGeometry( window, QRect( *newFrame )); // TODO: setGeometry() goes here, or in setTilingFrame()?
            break;
        }
    }

void TilingSurface::removeFromSurface( EffectWindow *window, TilingMode mode )
    {
    if( m_windows.removeOne( window ))
        {
        TilingEffect::m_tilingData[window]->setTilingFrame( NULL );
        // TODO: setGeometry() goes here?
        // TODO: Merge frame mode
        }
    }

TilingFrame* TilingSurface::getFrame( QPoint pos ) const
    {
    foreach( TilingFrame *frame, m_frames )
        if( frame->contains( pos ))
            return frame;
    assert( false ); // Should never get here
    }

void TilingSurface::resizedWindow( EffectWindow *window )
    {
    QList<TilingFrame*> frames;
    bool frameAdded;
    int minimum, maximum;
    QRect oldGeometry = QRect( *( TilingEffect::m_tilingData[window]->tilingFrame() ));
    QRect newGeometry = window->geometry();

    // Each side has it's own, duplicated code block, TODO: Somehow merge them?

    // Has the top moved?
    if( oldGeometry.y() != newGeometry.y() )
        {
        frames.append( TilingEffect::m_tilingData[window]->tilingFrame() );
        minimum = oldGeometry.x();
        maximum = oldGeometry.x() + oldGeometry.width();
        do // Work out which frames have the same border line as the resized one
            {
            frameAdded = false;
            foreach( TilingFrame *frame, m_frames )
                {
                if( frames.contains( frame ))
                    continue;
                if(( oldGeometry.y() == frame->y()
                  || oldGeometry.y() == frame->y() + frame->height() )
                 && minimum <= frame->x() + frame->width()
                 && maximum >= frame->x() )
                    {
                    frames.append( frame );
                    if( minimum > frame->x() )
                        minimum = frame->x();
                    if( maximum < frame->x() + frame->width() )
                        maximum = frame->x() + frame->width();
                    frameAdded = true;
                    break;
                    }
                }
            } while( frameAdded );
        foreach( TilingFrame *frame, frames )
            {
            if( oldGeometry.y() == frame->y() )
                frame->setTop( newGeometry.y() );
            if( oldGeometry.y() == frame->y() + frame->height() )
                frame->setBottom( newGeometry.y() - 1 );
            frame->resized();
            }
        }

    // Has the right moved?
    if( oldGeometry.x() + oldGeometry.width() != newGeometry.x() + newGeometry.width() )
        {
        frames.append( TilingEffect::m_tilingData[window]->tilingFrame() );
        minimum = oldGeometry.y();
        maximum = oldGeometry.y() + oldGeometry.height();
        do // Work out which frames have the same border line as the resized one
            {
            frameAdded = false;
            foreach( TilingFrame *frame, m_frames )
                {
                if( frames.contains( frame ))
                    continue;
                if(( oldGeometry.x() + oldGeometry.width() == frame->x()
                  || oldGeometry.x() + oldGeometry.width() == frame->x() + frame->width() )
                 && minimum <= frame->y() + frame->height()
                 && maximum >= frame->y() )
                    {
                    frames.append( frame );
                    if( minimum > frame->y() )
                        minimum = frame->y();
                    if( maximum < frame->y() + frame->height() )
                        maximum = frame->y() + frame->height();
                    frameAdded = true;
                    break;
                    }
                }
            } while( frameAdded );
        foreach( TilingFrame *frame, frames )
            {
            if( oldGeometry.x() + oldGeometry.width() == frame->x() )
                frame->setLeft( newGeometry.x() + newGeometry.width() );
            if( oldGeometry.x() + oldGeometry.width() == frame->x() + frame->width() )
                frame->setRight( newGeometry.x() + newGeometry.width() - 1 );
            frame->resized();
            }
        }

    // Has the bottom moved?
    if( oldGeometry.y() + oldGeometry.height() != newGeometry.y() + newGeometry.height() )
        {
        frames.append( TilingEffect::m_tilingData[window]->tilingFrame() );
        minimum = oldGeometry.x();
        maximum = oldGeometry.x() + oldGeometry.width();
        do // Work out which frames have the same border line as the resized one
            {
            frameAdded = false;
            foreach( TilingFrame *frame, m_frames )
                {
                if( frames.contains( frame ))
                    continue;
                if(( oldGeometry.y() + oldGeometry.height() == frame->y()
                  || oldGeometry.y() + oldGeometry.height() == frame->y() + frame->height() )
                 && minimum <= frame->x() + frame->width()
                 && maximum >= frame->x() )
                    {
                    frames.append( frame );
                    if( minimum > frame->x() )
                        minimum = frame->x();
                    if( maximum < frame->x() + frame->width() )
                        maximum = frame->x() + frame->width();
                    frameAdded = true;
                    break;
                    }
                }
            } while( frameAdded );
        foreach( TilingFrame *frame, frames )
            {
            if( oldGeometry.y() + oldGeometry.height() == frame->y() )
                frame->setTop( newGeometry.y() + newGeometry.height() );
            if( oldGeometry.y() + oldGeometry.height() == frame->y() + frame->height() )
                frame->setBottom( newGeometry.y() + newGeometry.height() - 1 );
            frame->resized();
            }
        }

    // Has the left moved?
    if( oldGeometry.x() != newGeometry.x() )
        {
        frames.append( TilingEffect::m_tilingData[window]->tilingFrame() );
        minimum = oldGeometry.y();
        maximum = oldGeometry.y() + oldGeometry.height();
        do // Work out which frames have the same border line as the resized one
            {
            frameAdded = false;
            foreach( TilingFrame *frame, m_frames )
                {
                if( frames.contains( frame ))
                    continue;
                if(( oldGeometry.x() == frame->x()
                  || oldGeometry.x() == frame->x() + frame->width() )
                 && minimum <= frame->y() + frame->height()
                 && maximum >= frame->y() )
                    {
                    frames.append( frame );
                    if( minimum > frame->y() )
                        minimum = frame->y();
                    if( maximum < frame->y() + frame->height() )
                        maximum = frame->y() + frame->height();
                    frameAdded = true;
                    break;
                    }
                }
            } while( frameAdded );
        foreach( TilingFrame *frame, frames )
            {
            if( oldGeometry.x() == frame->x() )
                frame->setLeft( newGeometry.x() );
            if( oldGeometry.x() == frame->x() + frame->width() )
                frame->setRight( newGeometry.x() - 1 );
            frame->resized();
            }
        }
    }

void TilingSurface::resizeFrame( TilingFrame *frame, KDecorationDefines::Position mode, QRect newGeometry )
    {
    // TODO
    kDebug( 1212 ) << "resizeFrame() called";
    //updateWindowGeoms();
    }

/*void TilingSurface::updateWindowGeoms( EffectWindow *skip )
    {
    foreach( EffectWindow *window, m_windows )
        if( window != skip )
            window->setGeometry( getGeom( window->gridRect() ));
    }*/

//-----------------------------------------------------------------------------
// TilingFrame

TilingFrame::TilingFrame( TilingSurface *surface, QRect geometry )
    : QRect( geometry )
    , m_surface( surface )
    {
    }

void TilingFrame::resized()
    {
    // TODO: Optimize!
    QList<EffectWindow*> windows = m_surface->windowsOnSurface();
    foreach( EffectWindow *window, windows )
        if( TilingEffect::m_tilingData[window]->tilingFrame() == this )
            effects->setWindowGeometry( window, QRect( *this ));
    }

} // namespace

#include "tiling.moc"
