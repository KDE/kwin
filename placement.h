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

        void place(Client* c);

        void placeAtRandom            (Client* c);
        void placeCascaded            (Client* c, bool re_init = false);
        void placeSmart               (Client* c);
        void placeCentered    (Client* c);
        void placeZeroCornered(Client* c);
        void placeDialog      (Client* c);
        void placeUtility     (Client* c);

    private:

        void placeInternal(Client* c);
        void placeUnderMouse(Client* c);
        void placeOnMainWindow(Client* c);

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
