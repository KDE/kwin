/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
