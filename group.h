/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_GROUP_H
#define KWIN_GROUP_H

#include "utils.h"
#include <netwm.h>

namespace KWin
{

class EffectWindowGroupImpl;
class X11Client;

class Group
{
public:
    Group(xcb_window_t leader);
    ~Group();
    xcb_window_t leader() const;
    const X11Client *leaderClient() const;
    X11Client *leaderClient();
    const QList<X11Client *> &members() const;
    QIcon icon() const;
    void addMember(X11Client *member);
    void removeMember(X11Client *member);
    void gotLeader(X11Client *leader);
    void lostLeader();
    void updateUserTime(xcb_timestamp_t time);
    xcb_timestamp_t userTime() const;
    void ref();
    void deref();
    EffectWindowGroupImpl* effectGroup();
private:
    void startupIdChanged();
    QList<X11Client *> _members;
    X11Client *leader_client;
    xcb_window_t leader_wid;
    NETWinInfo* leader_info;
    xcb_timestamp_t user_time;
    int refcount;
    EffectWindowGroupImpl* effect_group;
};

inline xcb_window_t Group::leader() const
{
    return leader_wid;
}

inline const X11Client *Group::leaderClient() const
{
    return leader_client;
}

inline X11Client *Group::leaderClient()
{
    return leader_client;
}

inline const QList<X11Client *> &Group::members() const
{
    return _members;
}

inline xcb_timestamp_t Group::userTime() const
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
