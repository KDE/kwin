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

#ifndef KWIN_TILINGLAYOUT_H
#define KWIN_TILINGLAYOUT_H

#include <QRect>
#include <QList>

#include "lib/kdecoration.h"

namespace KWin
{

class Workspace;
class Client;
class Tile;

class TilingLayout
    {
    public:
        TilingLayout( Workspace *w );
        virtual ~TilingLayout();
        
        virtual void clientResized( Client *c, const QRect &moveResizeGeom, const QRect &orig );
        void clientMoved( Client *c, const QRect &moveResizeGeom, const QRect &orig );
        void clientMinimizeToggled( Client *c );

        void commit();

        void setLayoutType( int t );
        int layoutType() const;

        void addTile( Tile *t );
        void addTile( Client *c );
        void removeTile( Tile *t );
        void removeTile( Client *c );
        void toggleFloatTile( Client *c );
        void swapTiles( Tile *a, Tile *b );

        virtual KDecorationDefines::Position resizeMode( Client *c, KDecorationDefines::Position currentMode ) const;

        const QList<Tile *>& tiles() const;
        Tile* findTile( Client *c ) const;

    protected:
        Workspace * workspace() const;
        const QRect layoutArea( Tile *t ) const;
        // currently only required by floating layout
        virtual void postAddTile( Tile *t );
        virtual void preRemoveTile( Tile *t );
        
    private:
        unsigned int findTilePos( Client *c ) const;
        virtual void arrange( QRect wgeom ) = 0;

        void addTileNoArrange( Tile *t );
        void removeTileNoArrange( Tile *t );

        Tile* findTileBelowPoint( const QPoint &p ) const;


        QList<Tile *> m_tiles;
        int m_layoutType;
        Workspace *m_workspace;


        friend class TilingLayoutFactory;
    };

inline void TilingLayout::setLayoutType( int t )
    {
    m_layoutType = t;
    }

inline int TilingLayout::layoutType() const
    {
    return m_layoutType;
    }

inline const QList<Tile *>& TilingLayout::tiles() const
    {
    return m_tiles;
    }

inline Workspace* TilingLayout::workspace() const
    {
    return m_workspace;
    }

inline void TilingLayout::postAddTile( Tile *t )
    {
    }

inline void TilingLayout::preRemoveTile( Tile *t )
    {
    }

} // end namespace

#endif
