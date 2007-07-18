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
