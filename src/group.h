/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "utils/common.h"
#include <netwm.h>

namespace KWin
{

class EffectWindowGroupImpl;
class X11Window;

class Group
{
public:
    Group(xcb_window_t leader);
    ~Group();
    xcb_window_t leader() const;
    const X11Window *leaderClient() const;
    X11Window *leaderClient();
    const QList<X11Window *> &members() const;
    QIcon icon() const;
    void addMember(X11Window *member);
    void removeMember(X11Window *member);
    void gotLeader(X11Window *leader);
    void lostLeader();
    void updateUserTime(xcb_timestamp_t time);
    xcb_timestamp_t userTime() const;
    void ref();
    void deref();
    EffectWindowGroupImpl *effectGroup();

private:
    void startupIdChanged();
    QList<X11Window *> _members;
    X11Window *leader_client;
    xcb_window_t leader_wid;
    std::unique_ptr<NETWinInfo> leader_info;
    xcb_timestamp_t user_time;
    int refcount;
    std::unique_ptr<EffectWindowGroupImpl> effect_group;
};

inline xcb_window_t Group::leader() const
{
    return leader_wid;
}

inline const X11Window *Group::leaderClient() const
{
    return leader_client;
}

inline X11Window *Group::leaderClient()
{
    return leader_client;
}

inline const QList<X11Window *> &Group::members() const
{
    return _members;
}

inline xcb_timestamp_t Group::userTime() const
{
    return user_time;
}

inline EffectWindowGroupImpl *Group::effectGroup()
{
    return effect_group.get();
}

} // namespace
