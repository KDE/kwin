/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "rootinfo_filter.h"
#include "netinfo.h"

namespace KWin
{

RootInfoFilter::RootInfoFilter(RootInfo *parent)
    : X11EventFilter(QList<int>{XCB_CLIENT_MESSAGE})
    , m_rootInfo(parent)
{
}

bool RootInfoFilter::event(xcb_generic_event_t *event)
{
    NET::Properties dirtyProtocols;
    NET::Properties2 dirtyProtocols2;
    m_rootInfo->event(event, &dirtyProtocols, &dirtyProtocols2);
    return false;
}

}
