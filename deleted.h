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

#ifndef KWIN_DELETED_H
#define KWIN_DELETED_H

#include "toplevel.h"

namespace KWin
{

class Deleted
    : public Toplevel
    {
    Q_OBJECT
    public:
        static Deleted* create( Toplevel* c );
        // used by effects to keep the window around for e.g. fadeout effects when it's destroyed
        void refWindow();
        void unrefWindow( bool delay = false );
        void discard( allowed_t );
        virtual int desktop() const;
        virtual QPoint clientPos() const;
        virtual QSize clientSize() const;
    protected:
        virtual void debug( kdbgstream& stream ) const;
        virtual bool shouldUnredirect() const;
    private:
        Deleted( Workspace *ws ); // use create()
        void copyToDeleted( Toplevel* c );
        virtual ~Deleted(); // deleted only using unrefWindow()
        int delete_refcount;
        double window_opacity;
        int desk;
        QRect contentsRect; // for clientPos()/clientSize()
    };

inline void Deleted::refWindow()
    {
    ++delete_refcount;
    }

} // namespace

#endif
