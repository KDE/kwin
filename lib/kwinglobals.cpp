/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "kwinglobals.h"

#include <QCursor>

#include "kdebug.h"

#include <assert.h>


namespace KWin
{


// Optimized version of QCursor::pos() that tries to avoid X roundtrips
// by updating the value only when the X timestamp changes.
static QPoint last_cursor_pos;
static Time last_cursor_timestamp = CurrentTime;

QPoint cursorPos()
    {
    last_cursor_timestamp = CurrentTime;
    if( last_cursor_timestamp == CurrentTime
        || last_cursor_timestamp != QX11Info::appTime())
        {
        last_cursor_timestamp = QX11Info::appTime();
        last_cursor_pos = QCursor::pos();
        }
    return last_cursor_pos;
    }


} // namespace

