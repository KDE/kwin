/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Nikhil Marathe <nsm.nikhil@gmail.com>

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

// all the tiling related code that is extensions to existing KWin classes
// Includes Workspace for now

#include "client.h"
#include "workspace.h"
#include "tile.h"
#include "tilinglayout.h"
#include "tilinglayoutfactory.h"

#include <knotification.h>
#include <klocale.h>
#include <kwindowinfo.h>
#include <kwindowsystem.h>
#include "lib/kdecoration.h"


namespace KWin
{

bool Workspace::tilingEnabled() const
    {
    return tilingEnabled_;
    }

void Workspace::setTilingEnabled( bool tiling )
    {
    if( tilingEnabled() == tiling ) return;
    tilingEnabled_ = tiling;

    if( tilingEnabled_ )
        {
        tilingLayouts.resize( numberOfDesktops() + 1 );
        foreach( Client *c, stackingOrder() )
            {
            createTile( c );
            }
        }
    else
        {
        qDeleteAll( tilingLayouts );
        tilingLayouts.clear();
        }
    }

void Workspace::slotToggleTiling()
    {
    if ( tilingEnabled() )
        {
        setTilingEnabled( false );
        QString message = i18n( "Tiling Disabled" );
        KNotification::event( "tilingdisabled", message, QPixmap(), NULL, KNotification::CloseOnTimeout, KComponentData( "kwin" ) );
        }
    else
        {
        setTilingEnabled( true );
        QString message = i18n( "Tiling Enabled" );
        KNotification::event( "tilingenabled", message, QPixmap(), NULL, KNotification::CloseOnTimeout, KComponentData( "kwin" ) );
        }
    }

void Workspace::createTile( Client *c )
    {
    if( c == NULL )
        return;

    if( c->desktop() < 0 || c->desktop() >= tilingLayouts.size() ) return;

    kDebug(1212) << "Now tiling " << c->caption();
    if( !tilingEnabled() || !tileable(c) )
        return;

    Tile *t = new Tile( c, clientArea( PlacementArea, c ) );
    if( !tileable( c ) )
        {
        kDebug(1212) << c->caption() << "is not tileable";
        t->floatTile();
        }

    if( !tilingLayouts.value(c->desktop()) )
        {
        tilingLayouts[c->desktop()] = TilingLayoutFactory::createLayout( TilingLayoutFactory::DefaultLayout, this );
        }
    tilingLayouts[c->desktop()]->addTile( t );
    tilingLayouts[c->desktop()]->commit();
    }

void Workspace::removeTile( Client *c )
    {
    if( tilingLayouts[ c->desktop() ] )
        tilingLayouts[ c->desktop() ]->removeTile( c );
    }

bool Workspace::tileable( Client *c )
    {

    kDebug(1212) << c->caption();
    KWindowInfo info = KWindowSystem::windowInfo( c->window(), -1U, NET::WM2WindowClass );
    kDebug(1212) << "WINDOW CLASS IS "<< info.windowClassClass();
    if( info.windowClassClass() == "Plasma-desktop" )
        {
        return false;
        }
    // TODO: if application specific settings
    // to ignore, put them here

    if( !c->isNormalWindow() )
        {
            return false;
        }

    // 0 means tile it, if we get 1 (floating), don't tile
    if( c->rules()->checkTilingOption( 0 ) == 1 )
        {
        return false;
        }

    kDebug() << "Tiling" << c;
    return true;
    }

void Workspace::belowCursor()
    {
        // TODO
    }

Tile* Workspace::getNiceTile() const
    {
    if( !tilingEnabled() ) return NULL;
    if( !tilingLayouts.value( activeClient()->desktop() ) ) return NULL;
    
    return tilingLayouts[ activeClient()->desktop() ]->findTile( activeClient() );
    // TODO
    }

void Workspace::updateAllTiles()
    {
    foreach( TilingLayout *t, tilingLayouts )
        {
        if( !t ) continue;
        t->commit();
        }
    }

/*
 * Resize the neighbouring clients to close any gaps
 */
void Workspace::notifyTilingWindowResize( Client *c, const QRect &moveResizeGeom, const QRect &orig )
    {
    if( tilingLayouts.value( c->desktop() ) == NULL )
        return;
    tilingLayouts[ c->desktop() ]->clientResized( c, moveResizeGeom, orig );
    }

void Workspace::notifyTilingWindowMove( Client *c, const QRect &moveResizeGeom, const QRect &orig )
    {
    if( tilingLayouts.value( c->desktop() ) == NULL )
        {
        c->setGeometry( moveResizeGeom );
        return;
        }
    tilingLayouts[ c->desktop() ]->clientMoved( c, moveResizeGeom, orig );
    updateAllTiles();
    }

void Workspace::notifyTilingWindowResizeDone( Client *c, const QRect &moveResizeGeom, const QRect &orig, bool canceled )
    {
    if( canceled )
        notifyTilingWindowResize( c, orig, moveResizeGeom );
    else
        notifyTilingWindowResize( c, moveResizeGeom, orig );
    }

void Workspace::notifyTilingWindowMoveDone( Client *c, const QRect &moveResizeGeom, const QRect &orig, bool canceled )
    {
    if( canceled )
        notifyTilingWindowMove( c, orig, moveResizeGeom );
    else
        notifyTilingWindowMove( c, moveResizeGeom, orig );
    }

void Workspace::notifyTilingWindowDesktopChanged( Client *c, int old_desktop )
    {
    if( c->desktop() < 1 || c->desktop() > numberOfDesktops() )
        return;

    if( tilingLayouts.value( old_desktop ) )
        {
        Tile *t = tilingLayouts[ old_desktop ]->findTile( c );
 
        // TODO: copied from createTile(), move this into separate method?
        if( !tilingLayouts.value( c->desktop() ) )
            {
            tilingLayouts[c->desktop()] = TilingLayoutFactory::createLayout( TilingLayoutFactory::DefaultLayout, this );
            }

        if( t )
            tilingLayouts[ c->desktop() ]->addTile( t );

        tilingLayouts[ old_desktop ]->removeTile( c );
        tilingLayouts[ old_desktop ]->commit();
        }
    }

/*
 * Implements the 3 raising modes in Window Behaviour -> Advanced
 */
void Workspace::notifyTilingWindowActivated( Client *c )
    {
    if( c == NULL )
        return;

    if( options->tilingRaisePolicy == 1 ) // individual raise/lowers
        return;

    if( tilingLayouts.value( c->desktop() ) )
        {
        QList<Tile *> tiles = tilingLayouts[ c->desktop() ]->tiles();

        StackingUpdatesBlocker blocker( this );

        Tile *tile_to_raise = tilingLayouts[ c->desktop() ]->findTile( c );

        if( !tile_to_raise )
            {
            return;
            }

        kDebug(1212) << "FOUND TILE";
        bool raise_floating = false;
        if( options->tilingRaisePolicy == 2 ) // floating always on top
            raise_floating = true;
        else
            raise_floating = tile_to_raise->floating();

        foreach( Tile *t, tiles )
            {
            if( t->floating() == raise_floating && t != tile_to_raise )
                raiseClient( t->client() );
            }
        // raise the current tile last so that it ends up on top
        // but only if it supposed to be raised, required to support tilingRaisePolicy
        kDebug(1212) << "Raise floating? " << raise_floating << "to raise is floating?" << tile_to_raise->floating();
        if( tile_to_raise->floating() == raise_floating )
            raiseClient( tile_to_raise->client() );
        }
    }

void Workspace::notifyTilingWindowMinimizeToggled( Client *c )
    {
    if( tilingLayouts.value( c->desktop() ) )
        {
        tilingLayouts[ c->desktop() ]->clientMinimizeToggled( c );
        }
    }

void Workspace::notifyTilingWindowMaximized( Client *c, Options::WindowOperation op )
    {
    if( tilingLayouts.value( c->desktop() ) )
        {
        Tile *t = tilingLayouts[ c->desktop() ]->findTile( c );
        if( !t )
            {
            createTile( c );
            t = tilingLayouts[ c->desktop() ]->findTile( c );

            // if still no tile, it couldn't be tiled
            // so ignore it
            if( !t )
                return;
            }

        // if window IS tiled and a maximize
        // is attempted, make the window float.
        // That is all we do since that can
        // mess up the layout.
        // In all other cases, don't do
        // anything, let the user manage toggling
        // using Meta+F
        if ( !t->floating()
             && (   op == Options::MaximizeOp
                 || op == Options::HMaximizeOp
                 || op == Options::VMaximizeOp ) )
            {
            tilingLayouts[ c->desktop() ]->toggleFloatTile( c );
            }

        }
    }

Tile* Workspace::findAdjacentTile( Tile *ref, int d )
    {
    QRect reference = ref->geometry();
    QPoint origin = reference.center();

    Tile *closest = NULL;
    int minDist = -1;

    QList<Tile *> tiles = tilingLayouts[ ref->client()->desktop() ]->tiles();

    foreach( Tile *t, tiles )
        {
        if( t->client() == ref->client() || t->ignoreGeometry() )
            continue;

        bool consider = false;

        QRect other = t->geometry();
        QPoint otherCenter = other.center();

        switch( d )
            {
            case Tile::Top:
                consider = otherCenter.y() < origin.y()
                        && other.bottom() < reference.top();
                break;

            case Tile::Right:
                consider = otherCenter.x() > origin.x()
                        && other.left() > reference.right();
                break;

            case Tile::Bottom:
                consider = otherCenter.y() > origin.y()
                        && other.top() > reference.bottom();
                break;

            case Tile::Left:
                consider = otherCenter.x() < origin.x()
                        && other.right() < reference.left();
                break;

            default:
                abort();
            }

        if( consider )
            {
            int dist = ( otherCenter - origin ).manhattanLength();
            if( minDist > dist || minDist < 0 )
                {
                    minDist = dist;
                    closest = t;
                }
            }
        }
    return closest;
    }

void Workspace::focusTile( int d )
    {
    Tile *t = getNiceTile();
    if( t )
        {
            Tile *adj = findAdjacentTile(t, d);
            if( adj )
                activateClient( adj->client() );
        }
    }

void Workspace::moveTile( int d )
    {
    Tile *t = getNiceTile();
    if( t )
        {
            Tile* adj = findAdjacentTile( t, d );

            tilingLayouts[ t->client()->desktop() ]->swapTiles( t, adj );
        }
    }

void Workspace::slotFocusTileLeft()
    {
    focusTile( Tile::Left );
    }

void Workspace::slotFocusTileRight()
    {
    focusTile( Tile::Right );
    }

void Workspace::slotFocusTileTop()
    {
    focusTile( Tile::Top );
    }

void Workspace::slotFocusTileBottom()
    {
    focusTile( Tile::Bottom );
    }

void Workspace::slotMoveTileLeft()
    {
    moveTile( Tile::Left );
    }

void Workspace::slotMoveTileRight()
    {
    moveTile( Tile::Right );
    }

void Workspace::slotMoveTileTop()
    {
    moveTile( Tile::Top );
    }

void Workspace::slotMoveTileBottom()
    {
    moveTile( Tile::Bottom );
    }

void Workspace::slotToggleFloating()
    {
    Client *c = activeClient();
    if( tilingLayouts.value( c->desktop() ) )
        {
        tilingLayouts[ c->desktop() ]->toggleFloatTile( c );
        }
    }

void Workspace::slotNextTileLayout()
    {
    if( tilingLayouts.value( currentDesktop() ) )
        {

        tilingLayouts.replace( currentDesktop(), TilingLayoutFactory::nextLayout( tilingLayouts[currentDesktop()] ) );

        tilingLayouts[currentDesktop()]->commit();
        }
    }

void Workspace::slotPreviousTileLayout()
    {
    if( tilingLayouts.value( currentDesktop() ) )
        {

        tilingLayouts.replace( currentDesktop(), TilingLayoutFactory::previousLayout( tilingLayouts[currentDesktop()] ) );

        tilingLayouts[currentDesktop()]->commit();
        }
    }

KDecorationDefines::Position Workspace::supportedTilingResizeMode( Client *c, KDecorationDefines::Position currentMode )
    {
    if( tilingLayouts.value( c->desktop() ) )
        {
        return tilingLayouts[c->desktop()]->resizeMode( c, currentMode );
        }
    return currentMode;
    }

void Workspace::dumpTiles() const
    {
    foreach( TilingLayout *t, tilingLayouts )
        {
        if( !t ) {
            kDebug(1212) << "EMPTY DESKTOP";
            continue;
        }
        kDebug(1212) << "Desktop" << tilingLayouts.indexOf( t );
        foreach( Tile *tile, t->tiles() )
            {
            tile->dumpTile( "--" );
            }
        }
    }
} // namespace

