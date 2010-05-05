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

#include "tilinglayoutfactory.h"

#include <QtGlobal>
#include <QList>
#include <klocale.h>
#include <kdebug.h>

#include "notifications.h"
#include "tile.h"
#include "client.h"

#include "tilinglayouts/spiral/spiral.h"
#include "tilinglayouts/columns/columns.h"
#include "tilinglayouts/floating/floating.h"

// w is the workspace pointer
#define ADD_LAYOUT( lay ) \
            case lay##Layout:\
            kDebug(1212) << #lay;\
            layout = new lay( w );\
            layout->setLayoutType( lay##Layout );\
            Notify::raise( Notify::TilingLayoutChanged,                 \
                           i18n( "Layout changed to %1", i18n(#lay) ) ); \
            break

namespace KWin
{

TilingLayout* TilingLayoutFactory::createLayout( int type, Workspace *w )
    {
    Q_ASSERT( type != FirstLayout && type != LastLayout );
    TilingLayout *layout;

    /* For new layouts, make a case entry here */
    switch( type )
        {
        case DefaultLayout: // NOTE: fall through makes first layout default
            layout = createLayout( indexToLayoutIndex( options->tilingLayout ), w );
            break;

        ADD_LAYOUT( Spiral );
        ADD_LAYOUT( Columns );
        ADD_LAYOUT( Floating );

        default:
            kDebug(1212) << "INVALID LAYOUT!";
            return NULL;
        }
    return layout;
    }

// if next, goes next, otherwise previous
TilingLayout* TilingLayoutFactory::cycleLayout( TilingLayout *curr, bool next )
    {
    int type = curr->layoutType();

    if( next )
        {
        type++;

        if( type >= LastLayout )
            type = FirstLayout + 1;
        }
    else
        {
        type--;

        if( type <= FirstLayout )
            type = LastLayout - 1;
        }

    QList<Tile *> tiles = curr->tiles();

    TilingLayout *l = createLayout( type, curr->workspace() );

    foreach( Tile *t, tiles )
    {
        curr->removeTileNoArrange( t );
    }

    Tile *last = tiles.takeLast();
    foreach( Tile *t, tiles )
    {
        l->addTileNoArrange( t );
    }
    l->addTile( last );

    return l;
    }

/**
 * Returns the appropriate layout enum item
 * Meant to be used with a combo box.
 * This function handles the issues of DefaultL and First and Last layouts
 */
int TilingLayoutFactory::indexToLayoutIndex( int index )
    {
    int layout = DefaultLayout + index + 1;
    if( layout >= LastLayout )
        layout = DefaultLayout + 1;
    if( layout <= FirstLayout )
        layout = LastLayout - 1;
    return layout;
    }
} // end namespace
