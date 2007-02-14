/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "deleted.h"

#include "workspace.h"
#include "client.h"

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
    delete effectWindow();
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
    if( WinInfo* cinfo = dynamic_cast< WinInfo* >( info ))
        cinfo->disable();
    }

void Deleted::unrefWindow()
    {
    if( --delete_refcount > 0 )
        return;
    deleteLater();
    }

int Deleted::desktop() const
    {
    return desk;
    }

void Deleted::debug( kdbgstream& stream ) const
    {
    stream << "\'ID:" << window() << "\' (deleted)";
    }

} // namespace

#include "deleted.moc"
