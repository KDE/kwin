/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cursor.h"

#include <xcb/xcb_cursor.h>

namespace KWin
{

class KWIN_EXPORT X11WindowManager : public QObject
{
    Q_OBJECT

public:
    ~X11WindowManager() override;

    xcb_cursor_t xcbCursor(CursorShape shape);
    xcb_cursor_t xcbCursor(const QByteArray &shapeName);

public Q_SLOTS:
    void start();
    void stop();

private:
    void resetXcbCursorCache();

    QHash<QByteArray, xcb_cursor_t> m_cursors;
    KWIN_SINGLETON(X11WindowManager)
};

inline X11WindowManager *xwm()
{
    return X11WindowManager::self();
}

} // namespace KWin
