/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Fredrik Höglund <fredrik@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "x11eventfilter.h"
#include <workspace.h>

namespace KWin
{

X11EventFilter::X11EventFilter(const QVector<int> &eventTypes)
    : m_eventTypes(eventTypes)
    , m_extension(0)
{
    Workspace::self()->registerEventFilter(this);
}


X11EventFilter::X11EventFilter(int eventType, int opcode, int genericEventType)
    : X11EventFilter(eventType, opcode, QVector<int>{genericEventType})
{
}

X11EventFilter::X11EventFilter(int eventType, int opcode, const QVector< int > &genericEventTypes)
    : m_eventTypes(QVector<int>{eventType}), m_extension(opcode), m_genericEventTypes(genericEventTypes)
{
    Workspace::self()->registerEventFilter(this);
}

X11EventFilter::~X11EventFilter()
{
    if (auto w = Workspace::self()) {
        w->unregisterEventFilter(this);
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
