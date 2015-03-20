/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_X11WINDOWED_BACKEND_H
#define KWIN_X11WINDOWED_BACKEND_H
#include "abstract_backend.h"

#include <kwin_export.h>

#include <QObject>
#include <QSize>

#include <xcb/xcb.h>

struct _XDisplay;
typedef struct _XDisplay Display;

namespace KWin
{

class KWIN_EXPORT X11WindowedBackend : public AbstractBackend
{
    Q_OBJECT
    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
public:
    virtual ~X11WindowedBackend();

    xcb_connection_t *connection() const {
        return m_connection;
    }
    int screenNumer() const {
        return m_screenNumber;
    }
    xcb_window_t window() const {
        return m_window;
    }
    Display *display() const {
        return m_display;
    }
    xcb_window_t rootWindow() const;

    bool isValid() const {
        return m_connection != nullptr && m_window != XCB_WINDOW_NONE;
    }

    QSize size() const {
        return m_size;
    }

    void installCursorFromServer() override;

    static X11WindowedBackend *self();
    static X11WindowedBackend *create(const QString &display, const QSize &size, QObject *parent);

Q_SIGNALS:
    void sizeChanged();

private:
    X11WindowedBackend(const QString &display, const QSize &size, QObject *parent);
    void createWindow();
    void startEventReading();
    void handleEvent(xcb_generic_event_t *event);
    void handleClientMessage(xcb_client_message_event_t *event);
    void handleButtonPress(xcb_button_press_event_t *event);
    void handleExpose(xcb_expose_event_t *event);
    void updateSize(xcb_configure_notify_event_t *event);

    xcb_connection_t *m_connection = nullptr;
    xcb_screen_t *m_screen = nullptr;
    int m_screenNumber = 0;
    xcb_window_t m_window = XCB_WINDOW_NONE;
    QSize m_size;
    xcb_atom_t m_protocols = XCB_ATOM_NONE;
    xcb_atom_t m_deleteWindowProtocol = XCB_ATOM_NONE;
    xcb_cursor_t m_cursor = XCB_CURSOR_NONE;
    Display *m_display = nullptr;
    static X11WindowedBackend *s_self;
};

inline X11WindowedBackend *X11WindowedBackend::self()
{
    return s_self;
}

}

#endif
