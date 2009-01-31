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

#include "highlightwindow.h"

#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT( highlightwindow, HighlightWindowEffect )

HighlightWindowEffect::HighlightWindowEffect()
    : m_highlightedWindow( NULL )
    , m_monitorWindow( NULL )
    {
    m_atom = XInternAtom( display(), "_KDE_WINDOW_HIGHLIGHT", False );
    effects->registerPropertyType( m_atom, true );

    // Announce support by creating a dummy version on the root window
    unsigned char dummy = 0;
    XChangeProperty( display(), rootWindow(), m_atom, m_atom, 8, PropModeReplace, &dummy, 1 );
    }

HighlightWindowEffect::~HighlightWindowEffect()
    {
    XDeleteProperty( display(), rootWindow(), m_atom );
    effects->registerPropertyType( m_atom, false );
    }

void HighlightWindowEffect::prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time )
    {
    // If we are highlighting a window set the translucent flag to all others
    if( m_highlightedWindow && m_highlightedWindow != w && m_monitorWindow != w && w->isNormalWindow() )
        data.mask |= PAINT_WINDOW_TRANSLUCENT;
    effects->prePaintWindow( w, data, time );
    }

void HighlightWindowEffect::paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data )
    {
    if( m_highlightedWindow && m_highlightedWindow != w && m_monitorWindow != w && w->isNormalWindow() )
        data.opacity *= 0.15; // TODO: Fade out
    effects->paintWindow( w, mask, region, data );
    }

void HighlightWindowEffect::windowAdded( EffectWindow* w )
    {
    propertyNotify( w, m_atom ); // Check initial value
    }

void HighlightWindowEffect::windowDeleted( EffectWindow* w )
    {
    if( m_monitorWindow == w )
        { // The monitoring window was destroyed
        m_monitorWindow = NULL;
        m_highlightedWindow = NULL;
        }
    }

void HighlightWindowEffect::propertyNotify( EffectWindow* w, long a )
    {
    if( a != m_atom )
        return; // Not our atom

    QByteArray byteData = w->readProperty( m_atom, m_atom, 32 );
    if( byteData.length() < 1 )
        return; // Invalid length
    long* data = reinterpret_cast<long*>( byteData.data() );

    if( !data[0] )
        { // Purposely clearing highlight
        m_monitorWindow = NULL;
        m_highlightedWindow = NULL;
        return;
        }
    m_monitorWindow = w;
    m_highlightedWindow = effects->findWindow( data[0] );
    if( !m_highlightedWindow )
        kDebug(1212) << "Invalid window targetted for highlight. Requested:" << data[0];
    }

} // namespace
