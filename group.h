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
#include <netwm.h>

namespace KWin
{

class Client;
class Workspace;
class EffectWindowGroupImpl;

class Group
    {
    public:
        Group( Window leader, Workspace* workspace );
        ~Group();
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
        bool groupEvent( XEvent* e );
        void updateUserTime( Time time = CurrentTime );
        Time userTime() const;
        EffectWindowGroupImpl* effectGroup();
    private:
        void getIcons();
        void startupIdChanged();
        ClientList _members;
        Client* leader_client;
        Window leader_wid;
        Workspace* _workspace;
        NETWinInfo* leader_info;
        Time user_time;
        EffectWindowGroupImpl* effect_group;
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
    return _workspace;
    }

inline Time Group::userTime() const
    {
    return user_time;
    }

inline
EffectWindowGroupImpl* Group::effectGroup()
    {
    return effect_group;
    }

} // namespace

#endif
