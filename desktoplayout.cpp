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

#include "desktoplayout.h"

namespace KWin
{

DesktopLayout::DesktopLayout()
    : m_count( 0 ) // This is an invalid state
    , m_gridSize( 1, 2 ) // Default to two rows
    , m_grid( new int[2] )
    , m_current( 0 )
    , m_dynamic( false )
    {
    m_grid[0] = 0;
    m_grid[1] = 0;
    }

DesktopLayout::~DesktopLayout()
    {
    delete[] m_grid;
    }

void DesktopLayout::setNumberOfDesktops( int count )
    {
    m_count = count;
    // Make sure our grid is valid. TODO: Is there a sane way to avoid overriding the existing grid?
    setNETDesktopLayout( Qt::Horizontal, m_count / m_gridSize.height() + 1, m_gridSize.height(), 0 );
    }

void DesktopLayout::setNETDesktopLayout( Qt::Orientation orientation, int width, int height,
    int startingCorner )
    {
    Q_UNUSED( startingCorner ); // Not really worth implementing right now.

    // Calculate valid grid size
    assert( width > 0 && height > 0 );
    if(( width <= 0 ) && ( height > 0 ))
       width = ( m_count + height - 1 ) / height;
    else if(( height <= 0 ) && ( width > 0 ))
       height = ( m_count + width - 1 ) / width;

    // Set private variables
    delete[] m_grid;
    m_gridSize = QSize( width, height );
    int size = width * height;
    m_grid = new int[size];

    // Populate grid
    int desktop = 1;
    if( orientation == Qt::Horizontal )
        for( int y = 0; y < height; y++ )
            for( int x = 0; x < width; x++ )
                m_grid[y * width + x] = (desktop <= m_count ? desktop++ : 0);
    else
        for( int x = 0; x < width; x++ )
            for( int y = 0; y < height; y++ )
                m_grid[y * width + x] = (desktop <= m_count ? desktop++ : 0);
    }

QPoint DesktopLayout::desktopGridCoords( int id ) const
    {
    for( int y = 0; y < m_gridSize.height(); y++ )
        for( int x = 0; x < m_gridSize.width(); x++ )
            if( m_grid[y * m_gridSize.height() + x] == id )
                return QPoint( x, y );
    return QPoint( -1, -1 );
    }

QPoint DesktopLayout::desktopCoords( int id ) const
    {
    QPoint coords = desktopGridCoords( id );
    if( coords.x() == -1 )
        return QPoint( -1, -1 );
    return QPoint( coords.x() * displayWidth(), coords.y() * displayHeight() );
    }

int DesktopLayout::desktopAbove( int id, bool wrap ) const
    {
    if( id == 0 )
        id = currentDesktop();
    QPoint coords = desktopGridCoords( id );
    assert( coords.x() >= 0 );
    for(;;)
        {
        coords.ry()--;
        if( coords.y() < 0 )
            {
            if( wrap )
                coords.setY( m_gridSize.height() - 1 );
            else
                return id; // Already at the top-most desktop
            }
        int desktop = desktopAtCoords( coords );
        if( desktop > 0 )
            return desktop;
        }
    }

int DesktopLayout::desktopToRight( int id, bool wrap ) const
    {
    if( id == 0 )
        id = currentDesktop();
    QPoint coords = desktopGridCoords( id );
    assert( coords.x() >= 0 );
    for(;;)
        {
        coords.rx()++;
        if( coords.x() >= m_gridSize.width() )
            {
            if( wrap )
                coords.setX( 0 );
            else
                return id; // Already at the right-most desktop
            }
        int desktop = desktopAtCoords( coords );
        if( desktop > 0 )
            return desktop;
        }
    }

int DesktopLayout::desktopBelow( int id, bool wrap ) const
    {
    if( id == 0 )
        id = currentDesktop();
    QPoint coords = desktopGridCoords( id );
    assert( coords.x() >= 0 );
    for(;;)
        {
        coords.ry()++;
        if( coords.y() >= m_gridSize.height() )
            {
            if( wrap )
                coords.setY( 0 );
            else
                return id; // Already at the bottom-most desktop
            }
        int desktop = desktopAtCoords( coords );
        if( desktop > 0 )
            return desktop;
        }
    }

int DesktopLayout::desktopToLeft( int id, bool wrap ) const
    {
    if( id == 0 )
        id = currentDesktop();
    QPoint coords = desktopGridCoords( id );
    assert( coords.x() >= 0 );
    for(;;)
        {
        coords.rx()--;
        if( coords.x() < 0 )
            {
            if( wrap )
                coords.setX( m_gridSize.width() - 1 );
            else
                return id; // Already at the left-most desktop
            }
        int desktop = desktopAtCoords( coords );
        if( desktop > 0 )
            return desktop;
        }
    }

int DesktopLayout::addDesktop( QPoint coords )
    { // TODO
    return 0;
    }

void DesktopLayout::deleteDesktop( int id )
    { // TODO
    }

} // namespace
