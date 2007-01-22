/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_UNMANAGED_H
#define KWIN_UNMANAGED_H

#include <netwm.h>

#include "toplevel.h"

namespace KWinInternal
{

class Unmanaged
    : public Toplevel
    {
    Q_OBJECT
    public:
        Unmanaged( Workspace *ws );
        bool windowEvent( XEvent* e );
        void release();
        bool track( Window w );
        virtual NET::WindowType windowType( bool direct = false, int supported_types = SUPPORTED_WINDOW_TYPES_MASK ) const;
        static void deleteUnmanaged( Unmanaged* c, allowed_t );
        virtual double opacity() const;
    protected:
        virtual void debug( kdbgstream& stream ) const;
    private:
        virtual ~Unmanaged(); // use release()
    // handlers for X11 events
        void mapNotifyEvent( XMapEvent* e );
        void unmapNotifyEvent( XUnmapEvent*e );
        void configureNotifyEvent( XConfigureEvent* e );
        NETWinInfo* info;
    };

} // namespace

#endif
