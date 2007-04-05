/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

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
        void unrefWindow();
        virtual int desktop() const;
    protected:
        virtual void debug( kdbgstream& stream ) const;
    private:
        Deleted( Workspace *ws ); // use create()
        void copyToDeleted( Toplevel* c );
        virtual ~Deleted(); // deleted only using unrefWindow()
        int delete_refcount;
        double window_opacity;
        int desk;
    };

inline void Deleted::refWindow()
    {
    ++delete_refcount;
    }

} // namespace

#endif
