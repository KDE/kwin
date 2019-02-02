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

#ifndef X11EVENTFILTER_H
#define X11EVENTFILTER_H

#include <xcb/xcb.h>

#include <QVector>

#include <kwin_export.h>

namespace KWin
{

class KWIN_EXPORT X11EventFilter
{
public:
    /**
     * Creates an event filter for the given event type.
     **/
    X11EventFilter(int eventType, int opcode = 0, int genericEventType = 0);
    X11EventFilter(int eventType, int opcode, const QVector<int> &genericEventTypes);
    X11EventFilter(const QVector<int> &eventTypes);

    /**
     * Destroys the event filter.
     **/
    virtual ~X11EventFilter();

    /**
     * Returns the type of events to filter.
     **/
    QVector<int> eventTypes() const { return m_eventTypes; }

    /**
     * Returns the major opcode of the extension.
     *
     * Only used when the event type is XCB_GE_GENERIC.
     **/
    int extension() const { return m_extension; }

    /**
     * Returns the types of generic events to filter.
     *
     * Only used when the event type is XCB_GE_GENERIC.
     **/
    QVector<int> genericEventTypes() const { return m_genericEventTypes; }

    /**
     * This method is called for every event of the filtered type.
     *
     * Return true to accept the event and stop further processing, and false otherwise.
     **/
    virtual bool event(xcb_generic_event_t *event) = 0;

    /**
     * Whether the event filter is for XCB_GE_GENERIC events.
     **/
    bool isGenericEvent() const;

private:
    QVector<int> m_eventTypes;
    int m_extension;
    QVector<int> m_genericEventTypes;
};

} // namespace KWin

#endif
