/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "x11eventfilter.h"

#include <xcb/xcb.h>

#include <functional>

class QPoint;

namespace KWin
{
class Window;

class WindowSelector : public X11EventFilter
{
public:
    WindowSelector();
    ~WindowSelector() override;

    void start(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName);
    void start(std::function<void(const QPoint &)> callback);
    bool isActive() const
    {
        return m_active;
    }
    void processEvent(xcb_generic_event_t *event);

    bool event(xcb_generic_event_t *event) override;

private:
    xcb_cursor_t createCursor(const QByteArray &cursorName);
    void release();
    void selectWindowUnderPointer();
    void handleKeyPress(xcb_keycode_t keycode, uint16_t state);
    void handleButtonRelease(xcb_button_t button, xcb_window_t window);
    void selectWindowId(xcb_window_t window_to_kill);
    bool activate(const QByteArray &cursorName = QByteArray());
    void cancelCallback();
    bool m_active;
    std::function<void(KWin::Window *)> m_callback;
    std::function<void(const QPoint &)> m_pointSelectionFallback;
};

} // namespace
