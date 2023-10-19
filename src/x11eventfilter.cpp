/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11eventfilter.h"
#include "main.h"

namespace KWin
{

X11EventFilter::X11EventFilter(const QList<int> &eventTypes)
    : m_eventTypes(eventTypes)
    , m_extension(0)
{
    kwinApp()->registerEventFilter(this);
}

X11EventFilter::X11EventFilter(int eventType, int opcode, int genericEventType)
    : X11EventFilter(eventType, opcode, QList<int>{genericEventType})
{
}

X11EventFilter::X11EventFilter(int eventType, int opcode, const QList<int> &genericEventTypes)
    : m_eventTypes(QList<int>{eventType})
    , m_extension(opcode)
    , m_genericEventTypes(genericEventTypes)
{
    kwinApp()->registerEventFilter(this);
}

X11EventFilter::~X11EventFilter()
{
    if (kwinApp()) {
        kwinApp()->unregisterEventFilter(this);
    }
}

bool X11EventFilter::isGenericEvent() const
{
    if (m_eventTypes.count() != 1) {
        return false;
    }
    return m_eventTypes.first() == XCB_GE_GENERIC;
}

}
