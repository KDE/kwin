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

#include "deleted.h"

#include "workspace.h"
#include "client.h"
#include "effects.h"

namespace KWin
{

Deleted::Deleted( Workspace* ws )
    : Toplevel( ws )
    , delete_refcount( 1 )
    {
    }

Deleted::~Deleted()
    {
    assert( delete_refcount == 0 );
    workspace()->removeDeleted( this, Allowed );
    deleteEffectWindow();
    }

Deleted* Deleted::create( Toplevel* c )
    {
    Deleted* d = new Deleted( c->workspace());
    d->copyToDeleted( c );
    d->workspace()->addDeleted( d, Allowed );
    return d;
    }

void Deleted::copyToDeleted( Toplevel* c )
    {
    assert( dynamic_cast< Deleted* >( c ) == NULL );
    Toplevel::copyToDeleted( c );
    desk = c->desktop();
    contentsRect = QRect( c->clientPos(), c->clientSize());
    if( WinInfo* cinfo = dynamic_cast< WinInfo* >( info ))
        cinfo->disable();
    }

void Deleted::unrefWindow( bool delay )
    {
    if( --delete_refcount > 0 )
        return;
    // needs to be delayed when calling from effects, otherwise it'd be rather
    // complicated to handle the case of the window going away during a painting pass
    if( delay )
        deleteLater();
    else
        delete this;
    }

int Deleted::desktop() const
    {
    return desk;
    }

QPoint Deleted::clientPos() const
    {
    return contentsRect.topLeft();
    }

QSize Deleted::clientSize() const
    {
    return contentsRect.size();
    }

void Deleted::debug( kdbgstream& stream ) const
    {
    stream << "\'ID:" << window() << "\' (deleted)";
    }

} // namespace

#include "deleted.moc"
