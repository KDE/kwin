/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#include "xfixes_cursor_event_filter.h"
#include "x11cursor.h"
#include "xcbutils.h"

namespace KWin
{

XFixesCursorEventFilter::XFixesCursorEventFilter(X11Cursor *cursor)
    : X11EventFilter(QVector<int>{Xcb::Extensions::self()->fixesCursorNotifyEvent()})
    , m_cursor(cursor)
{
}

bool XFixesCursorEventFilter::event(xcb_generic_event_t *event)
{
    Q_UNUSED(event);
    m_cursor->notifyCursorChanged();
    return false;
}

}
