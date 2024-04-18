/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "cursor.h"

#include <QTimer>
#include <memory>

namespace KWin
{
class XFixesCursorEventFilter;

class KWIN_EXPORT X11Cursor : public Cursor
{
    Q_OBJECT
public:
    X11Cursor(bool xInputSupport = false);
    ~X11Cursor() override;

    /**
     * @internal
     *
     * Called from X11 event handler.
     */
    void notifyCursorChanged();
    /**
     * @internal queries the cursor position
     */
    void notifyCursorPosChanged();

protected:
    void doSetPos() override;
    void doGetPos() override;

private Q_SLOTS:
    /**
     * Because of QTimer's and the impossibility to get events for all mouse
     * movements (at least I haven't figured out how) the position needs
     * to be also refetched after each return to the event loop.
     */
    void resetTimeStamp();
    void aboutToBlock();
    void pollMouse();

private:
    xcb_timestamp_t m_timeStamp;
    uint16_t m_buttonMask;
    QTimer m_resetTimeStampTimer;
    QTimer m_mousePollingTimer;
    bool m_hasXInput;

    std::unique_ptr<XFixesCursorEventFilter> m_xfixesFilter;

    friend class Cursor;
};

}
