/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#include "itemlayoutconfig.h"

namespace KWin
{
namespace TabBox
{

ItemLayoutConfigRowElement::ItemLayoutConfigRowElement()
    {
    // initialize attributes with reasonable default values
    m_type = ElementEmpty;
    m_iconSize = QSize( 16, 16 );
    m_alignment = Qt::AlignCenter;
    m_stretch = false;
    m_smallTextSize = false;
    m_bold = false;
    m_italic = false;
    m_italicMinimized = true;
    m_rowSpan = false;
    m_width = 0.0;
    }

ItemLayoutConfigRowElement::~ItemLayoutConfigRowElement()
    {
    }

ItemLayoutConfigRow::ItemLayoutConfigRow()
    {
    }

ItemLayoutConfigRow::~ItemLayoutConfigRow()
    {
    }

void ItemLayoutConfigRow::addElement( ItemLayoutConfigRowElement element )
    {
    m_elements.append( element );
    }

ItemLayoutConfigRowElement ItemLayoutConfigRow::element( int index ) const
    {
    return m_elements[ index ];
    }

int ItemLayoutConfigRow::count() const
    {
    return m_elements.count();
    }

ItemLayoutConfig::ItemLayoutConfig()
    {
    }

ItemLayoutConfig::~ItemLayoutConfig()
    {
    }

void ItemLayoutConfig::addRow( ItemLayoutConfigRow row )
    {
    m_rows.append( row );
    }

int ItemLayoutConfig::count() const
    {
    return m_rows.count();
    }

ItemLayoutConfigRow ItemLayoutConfig::row( int index ) const
    {
    return m_rows[ index ];
    }

} //namespace TabBox
} //namespace KWin

