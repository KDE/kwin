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

#ifndef KWIN_DESKTOPLAYOUT_H
#define KWIN_DESKTOPLAYOUT_H

#include <assert.h>
#include <QPoint>
#include <QSize>

#include "utils.h"

namespace KWin
{

class DesktopLayout
    {
    public:
        DesktopLayout();
        ~DesktopLayout();

        /**
         * @returns Total number of desktops currently in existance.
         */
        int numberOfDesktops() const;
        /**
         * Set the number of available desktops to @a count. It is not recommended to use this
         * function as it overrides any previous grid layout.
         */
        void setNumberOfDesktops( int count );

        /**
         * @returns The width of desktop layout in grid units.
         */
        int gridWidth() const;
        /**
         * @returns The height of desktop layout in grid units.
         */
        int gridHeight() const;
        /**
         * @returns The width of desktop layout in pixels. Equivalent to gridWidth() *
         * ::displayWidth().
         */
        int width() const;
        /**
         * @returns The height of desktop layout in pixels. Equivalent to gridHeight() *
         * ::displayHeight().
         */
        int height() const;

        /**
         * @returns The ID of the current desktop.
         */
        int currentDesktop() const;
        /**
         * Set the current desktop to @a current.
         */
        void setCurrentDesktop( int current );

        /**
         * Generate a desktop layout from EWMH _NET_DESKTOP_LAYOUT property parameters.
         */
        void setNETDesktopLayout( Qt::Orientation orientation, int width, int height, int startingCorner );

        /**
         * @returns The ID of the desktop at the point @a coords or 0 if no desktop exists at that
         * point. @a coords is to be in grid units.
         */
        int desktopAtCoords( QPoint coords ) const;
        /**
         * @returns The coords of desktop @a id in grid units.
         */
        QPoint desktopGridCoords( int id ) const;
        /**
         * @returns The coords of the top-left corner of desktop @a id in pixels.
         */
        QPoint desktopCoords( int id ) const;

        /**
         * @returns The ID of the desktop above desktop @a id. Wraps around to the bottom of
         * the layout if @a wrap is set. If @a id is not set use the current one.
         */
        int desktopAbove( int id = 0, bool wrap = true ) const;
        /**
         * @returns The ID of the desktop to the right of desktop @a id. Wraps around to the
         * left of the layout if @a wrap is set. If @a id is not set use the current one.
         */
        int desktopToRight( int id = 0, bool wrap = true ) const;
        /**
         * @returns The ID of the desktop below desktop @a id. Wraps around to the top of the
         * layout if @a wrap is set. If @a id is not set use the current one.
         */
        int desktopBelow( int id = 0, bool wrap = true ) const;
        /**
         * @returns The ID of the desktop to the left of desktop @a id. Wraps around to the
         * right of the layout if @a wrap is set. If @a id is not set use the current one.
         */
        int desktopToLeft( int id = 0, bool wrap = true ) const;

        /**
         * @returns Whether or not the layout is allowed to be modified by the user.
         */
        bool isDynamic() const;
        /**
         * Sets whether or not this layout can be modified by the user.
         */
        void setDynamic( bool dynamic );
        /**
         * Create new desktop at the point @a coords
         * @returns The ID of the created desktop
         */
        int addDesktop( QPoint coords );
        /**
         * Deletes the desktop with the ID @a id. All desktops with an ID greater than the one that
         * was deleted will have their IDs' decremented.
         */
        void deleteDesktop( int id );

    private:
        int m_count;
        QSize m_gridSize;
        int* m_grid;
        int m_current;
        bool m_dynamic;
    };

inline int DesktopLayout::numberOfDesktops() const
    {
    return m_count;
    }

inline int DesktopLayout::gridWidth() const
    {
    return m_gridSize.width();
    }

inline int DesktopLayout::gridHeight() const
    {
    return m_gridSize.height();
    }

inline int DesktopLayout::width() const
    {
    return m_gridSize.width() * displayWidth();
    }

inline int DesktopLayout::height() const
    {
    return m_gridSize.height() * displayHeight();
    }

inline int DesktopLayout::currentDesktop() const
    {
    return m_current;
    }

inline void DesktopLayout::setCurrentDesktop( int current )
    {
    assert( current >= 1 );
    assert( current <= m_count );
    m_current = current;
    }

inline int DesktopLayout::desktopAtCoords( QPoint coords ) const
    {
    return m_grid[coords.y() * m_gridSize.height() + coords.x()];
    }

inline bool DesktopLayout::isDynamic() const
    {
    return m_dynamic;
    }

inline void DesktopLayout::setDynamic( bool dynamic )
    {
    m_dynamic = dynamic;
    }

} // namespace

#endif
