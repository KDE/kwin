/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

//#define QT_CLEAN_NAMESPACE

#include "group.h"
#include "workspace.h"
#include "x11client.h"
#include "effects.h"

#include <KWindowSystem>
#include <QDebug>

namespace KWin
{

//********************************************
// Group
//********************************************

Group::Group(xcb_window_t leader_P)
    :   leader_client(nullptr),
        leader_wid(leader_P),
        leader_info(nullptr),
        user_time(-1U),
        refcount(0)
{
    if (leader_P != XCB_WINDOW_NONE) {
        leader_client = workspace()->findClient(Predicate::WindowMatch, leader_P);
        leader_info = new NETWinInfo(connection(), leader_P, rootWindow(),
                                     NET::Properties(), NET::WM2StartupId);
    }
    effect_group = new EffectWindowGroupImpl(this);
    workspace()->addGroup(this);
}

Group::~Group()
{
    delete leader_info;
    delete effect_group;
}

QIcon Group::icon() const
{
    if (leader_client != nullptr)
        return leader_client->icon();
    else if (leader_wid != XCB_WINDOW_NONE) {
        QIcon ic;
        NETWinInfo info(connection(), leader_wid, rootWindow(), NET::WMIcon, NET::WM2IconPixmap);
        auto readIcon = [&ic, &info, this](int size, bool scale = true) {
            const QPixmap pix = KWindowSystem::icon(leader_wid, size, size, scale, KWindowSystem::NETWM | KWindowSystem::WMHints, &info);
            if (!pix.isNull()) {
                ic.addPixmap(pix);
            }
        };
        readIcon(16);
        readIcon(32);
        readIcon(48, false);
        readIcon(64, false);
        readIcon(128, false);
        return ic;
    }
    return QIcon();
}

void Group::addMember(X11Client *member_P)
{
    _members.append(member_P);
//    qDebug() << "GROUPADD:" << this << ":" << member_P;
//    qDebug() << kBacktrace();
}

void Group::removeMember(X11Client *member_P)
{
//    qDebug() << "GROUPREMOVE:" << this << ":" << member_P;
//    qDebug() << kBacktrace();
    Q_ASSERT(_members.contains(member_P));
    _members.removeAll(member_P);
// there are cases when automatic deleting of groups must be delayed,
// e.g. when removing a member and doing some operation on the possibly
// other members of the group (which would be however deleted already
// if there were no other members)
    if (refcount == 0 && _members.isEmpty()) {
        workspace()->removeGroup(this);
        delete this;
    }
}

void Group::ref()
{
    ++refcount;
}

void Group::deref()
{
    if (--refcount == 0 && _members.isEmpty()) {
        workspace()->removeGroup(this);
        delete this;
    }
}

void Group::gotLeader(X11Client *leader_P)
{
    Q_ASSERT(leader_P->window() == leader_wid);
    leader_client = leader_P;
}

void Group::lostLeader()
{
    Q_ASSERT(!_members.contains(leader_client));
    leader_client = nullptr;
    if (_members.isEmpty()) {
        workspace()->removeGroup(this);
        delete this;
    }
}

} // namespace
