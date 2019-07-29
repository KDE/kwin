/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_X11CURSOR_H
#define KWIN_X11CURSOR_H
#include "cursor.h"

#include <memory>

namespace KWin
{
class XFixesCursorEventFilter;

class KWIN_EXPORT X11Cursor : public Cursor
{
    Q_OBJECT
public:
    X11Cursor(QObject *parent, bool xInputSupport = false);
    ~X11Cursor() override;

    void schedulePoll() {
        m_needsPoll = true;
    }

    /**
     * @internal
     *
     * Called from X11 event handler.
     */
    void notifyCursorChanged();

protected:
    xcb_cursor_t getX11Cursor(CursorShape shape) override;
    xcb_cursor_t getX11Cursor(const QByteArray &name) override;
    void doSetPos() override;
    void doGetPos() override;
    void doStartMousePolling() override;
    void doStopMousePolling() override;
    void doStartCursorTracking() override;
    void doStopCursorTracking() override;

private Q_SLOTS:
    /**
     * Because of QTimer's and the impossibility to get events for all mouse
     * movements (at least I haven't figured out how) the position needs
     * to be also refetched after each return to the event loop.
     */
    void resetTimeStamp();
    void mousePolled();
    void aboutToBlock();
private:
    xcb_cursor_t createCursor(const QByteArray &name);
    QHash<QByteArray, xcb_cursor_t > m_cursors;
    xcb_timestamp_t m_timeStamp;
    uint16_t m_buttonMask;
    QTimer *m_resetTimeStampTimer;
    QTimer *m_mousePollingTimer;
    bool m_hasXInput;
    bool m_needsPoll;

    std::unique_ptr<XFixesCursorEventFilter> m_xfixesFilter;

    friend class Cursor;
};


}

#endif
