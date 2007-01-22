/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "deleted.h"

#include "workspace.h"

namespace KWinInternal
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
    window_opacity = c->opacity();
    }

void Deleted::unrefWindow()
    {
    if( --delete_refcount > 0 )
        return;
    deleteLater();
    }

NET::WindowType Deleted::windowType( bool direct, int supported_types ) const
    {
    return NET::Normal; // TODO
    }

double Deleted::opacity() const
    {
    return window_opacity;
    }

void Deleted::debug( kdbgstream& stream ) const
    {
    stream << "\'ID:" << handle() << "\' (deleted)";
    }

} // namespace

#include "deleted.moc"
