/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_GROUP_H
#define KWIN_GROUP_H

#include "utils.h"
#include <X11/X.h>

namespace KWinInternal
{

class Client;
class Workspace;

class Group
    {
    public:
        Group( Window leader, Workspace* workspace );
        Window leader() const;
        const Client* leaderClient() const;
        Client* leaderClient();
        const ClientList& members() const;
        QPixmap icon() const;
        QPixmap miniIcon() const;
        void addMember( Client* member );
        void removeMember( Client* member );
        void gotLeader( Client* leader );
        void lostLeader();
        Workspace* workspace();
    private:
        ClientList _members;
        Client* leader_client;
        Window leader_wid;
        Workspace* workspace_;
    };

inline Window Group::leader() const
    {
    return leader_wid;
    }

inline const Client* Group::leaderClient() const
    {
    return leader_client;
    }

inline Client* Group::leaderClient()
    {
    return leader_client;
    }

inline const ClientList& Group::members() const
    {
    return _members;
    }

inline Workspace* Group::workspace()
    {
    return workspace_;
    }

} // namespace

#endif
