/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_UNMANAGED_H
#define KWIN_UNMANAGED_H

#include "toplevel.h"

namespace KWinInternal
{

class Unmanaged
    : public Toplevel
    {
    Q_OBJECT
    public:
        Unmanaged( Workspace *ws );
        virtual ~Unmanaged();
        bool windowEvent( XEvent* e );
        void release();
        bool track( Window w );
        static void deleteUnmanaged( Unmanaged* c, allowed_t );
    protected:
        virtual void debug( kdbgstream& stream ) const;
    private:
    // handlers for X11 events
        void mapNotifyEvent( XMapEvent* e );
        void unmapNotifyEvent( XUnmapEvent*e );
        void configureNotifyEvent( XConfigureEvent* e );
        void damageNotifyEvent( XDamageNotifyEvent* e );
    };

} // namespace

#endif
