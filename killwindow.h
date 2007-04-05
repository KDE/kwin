/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_KILLWINDOW_H
#define KWIN_KILLWINDOW_H

#include "workspace.h"

namespace KWin
{

class KillWindow 
    {
    public:

        KillWindow( Workspace* ws );
        ~KillWindow();

        void start();

    private:
        Workspace* workspace;
    };

} // namespace

#endif
