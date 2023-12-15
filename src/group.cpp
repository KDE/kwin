/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "group.h"
#include "effect/effecthandler.h"
#include "workspace.h"
#include "x11window.h"

#include <KStartupInfo>
#include <KX11Extras>
#include <QDebug>

namespace KWin
{

//********************************************
// Group
//********************************************

Group::Group(xcb_window_t leader_P)
    : leader_client(nullptr)
    , leader_wid(leader_P)
    , leader_info(nullptr)
    , user_time(-1U)
    , refcount(0)
{
    if (leader_P != XCB_WINDOW_NONE) {
        leader_client = workspace()->findClient(Predicate::WindowMatch, leader_P);
        leader_info = std::make_unique<NETWinInfo>(kwinApp()->x11Connection(), leader_P, kwinApp()->x11RootWindow(),
                                                   NET::Properties(), NET::WM2StartupId);
    }
    effect_group = std::make_unique<EffectWindowGroup>(this);
    workspace()->addGroup(this);
}

Group::~Group() = default;

QIcon Group::icon() const
{
    if (leader_client != nullptr) {
        return leader_client->icon();
    } else if (leader_wid != XCB_WINDOW_NONE) {
        QIcon ic;
        NETWinInfo info(kwinApp()->x11Connection(), leader_wid, kwinApp()->x11RootWindow(), NET::WMIcon, NET::WM2IconPixmap);
        auto readIcon = [&ic, &info, this](int size, bool scale = true) {
            const QPixmap pix = KX11Extras::icon(leader_wid, size, size, scale, KX11Extras::NETWM | KX11Extras::WMHints, &info);
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

void Group::addMember(X11Window *member_P)
{
    _members.append(member_P);
    //    qDebug() << "GROUPADD:" << this << ":" << member_P;
    //    qDebug() << kBacktrace();
}

void Group::removeMember(X11Window *member_P)
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

void Group::gotLeader(X11Window *leader_P)
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

void Group::startupIdChanged()
{
    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification(leader_wid, asn_id, asn_data);
    if (!asn_valid) {
        return;
    }
    if (asn_id.timestamp() != 0 && user_time != -1U
        && NET::timestampCompare(asn_id.timestamp(), user_time) > 0) {
        user_time = asn_id.timestamp();
    }
}

void Group::updateUserTime(xcb_timestamp_t time)
{
    // copy of X11Window::updateUserTime
    if (time == XCB_CURRENT_TIME) {
        kwinApp()->updateXTime();
        time = xTime();
    }
    if (time != -1U
        && (user_time == XCB_CURRENT_TIME
            || NET::timestampCompare(time, user_time) > 0)) { // time > user_time
        user_time = time;
    }
}

} // namespace
