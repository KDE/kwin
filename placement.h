/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 1997 to 2002 Cristian Tibirna <tibirna@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_PLACEMENT_H
#define KWIN_PLACEMENT_H

#include <qpoint.h>
#include <qvaluelist.h>

namespace KWinInternal
{

class Workspace;
class Client;

class Placement
    {
    public:

        Placement(Workspace* w);

        void place(Client* c, QRect& area );

        void placeAtRandom            (Client* c, const QRect& area );
        void placeCascaded            (Client* c, const QRect& area, bool re_init = false);
        void placeSmart               (Client* c, const QRect& area );
        void placeCentered    (Client* c, const QRect& area );
        void placeZeroCornered(Client* c, const QRect& area );
        void placeDialog      (Client* c, QRect& area );
        void placeUtility     (Client* c, QRect& area );

    private:

        void placeInternal(Client* c, const QRect& area );
        void placeUnderMouse(Client* c, QRect& area );
        void placeOnMainWindow(Client* c, QRect& area );
        QRect checkArea( const Client*c, const QRect& area );

        Placement();

    //CT needed for cascading+
        struct DesktopCascadingInfo 
            {
            QPoint pos;
            int col;
            int row;
            };

        QValueList<DesktopCascadingInfo> cci;

        Workspace* m_WorkspacePtr;
    };

} // namespace

#endif
