/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11windowmanager.h"
#include "cursor.h"
#include "main.h"

namespace KWin
{

KWIN_SINGLETON_FACTORY(X11WindowManager)

X11WindowManager::X11WindowManager(QObject *parent)
    : QObject(parent)
{
}

X11WindowManager::~X11WindowManager()
{
    s_self = nullptr;
}

void X11WindowManager::start()
{
    Cursor *mouseCursor = Cursors::self()->mouse();
    if (mouseCursor) {
        connect(mouseCursor, &Cursor::themeChanged, this, &X11WindowManager::resetXcbCursorCache);
    }
}

void X11WindowManager::stop()
{
    Cursor *mouseCursor = Cursors::self()->mouse();
    if (mouseCursor) {
        disconnect(mouseCursor, &Cursor::themeChanged, this, &X11WindowManager::resetXcbCursorCache);
    }

    m_cursors.clear();
}

xcb_cursor_t X11WindowManager::xcbCursor(CursorShape shape)
{
    return xcbCursor(shape.name());
}

xcb_cursor_t X11WindowManager::xcbCursor(const QByteArray &name)
{
    auto it = m_cursors.constFind(name);
    if (it != m_cursors.constEnd()) {
        return it.value();
    }

    if (name.isEmpty()) {
        return XCB_CURSOR_NONE;
    }

    xcb_cursor_context_t *ctx;
    if (xcb_cursor_context_new(kwinApp()->x11Connection(), kwinApp()->x11DefaultScreen(), &ctx) < 0) {
        return XCB_CURSOR_NONE;
    }

    xcb_cursor_t cursor = xcb_cursor_load_cursor(ctx, name.constData());
    if (cursor == XCB_CURSOR_NONE) {
        const auto &names = Cursor::cursorAlternativeNames(name);
        for (const QByteArray &cursorName : names) {
            cursor = xcb_cursor_load_cursor(ctx, cursorName.constData());
            if (cursor != XCB_CURSOR_NONE) {
                break;
            }
        }
    }
    if (cursor != XCB_CURSOR_NONE) {
        m_cursors.insert(name, cursor);
    }

    xcb_cursor_context_free(ctx);
    return cursor;
}

void X11WindowManager::resetXcbCursorCache()
{
    m_cursors.clear();
}

} // namespace KWin
