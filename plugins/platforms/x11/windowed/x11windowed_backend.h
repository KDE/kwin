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
#include "platform.h"

#include <kwin_export.h>

#include <QObject>
#include <QSize>

#include <xcb/xcb.h>

struct _XDisplay;
typedef struct _XDisplay Display;
typedef struct _XCBKeySymbols xcb_key_symbols_t;
class NETWinInfo;

namespace KWin
{

class KWIN_EXPORT X11WindowedBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "x11.json")
    Q_PROPERTY(QSize size READ screenSize NOTIFY sizeChanged)
public:
    X11WindowedBackend(QObject *parent = nullptr);
    virtual ~X11WindowedBackend();
    void init() override;
    QVector<QRect> screenGeometries() const override;
    QVector<qreal> screenScales() const override;

    xcb_connection_t *connection() const {
        return m_connection;
    }
    int screenNumer() const {
        return m_screenNumber;
    }
    xcb_window_t window() const {
        return windowForScreen(0);
    }
    xcb_window_t windowForScreen(int screen) const;
    Display *display() const {
        return m_display;
    }
    xcb_window_t rootWindow() const;

    xcb_visualid_t visual() const {
        return m_screen->root_visual;
    }

    Screens *createScreens(QObject *parent = nullptr) override;
    OpenGLBackend *createOpenGLBackend() override;
    QPainterBackend* createQPainterBackend() override;
    VulkanBackend* createVulkanBackend() override;
    void warpPointer(const QPointF &globalPos) override;

    QVector<CompositingType> supportedCompositors() const override;

Q_SIGNALS:
    void sizeChanged();

private:
    void createWindow();
    void startEventReading();
    void grabKeyboard(xcb_timestamp_t time);
    void updateWindowTitle();
    void handleEvent(xcb_generic_event_t *event);
    void handleClientMessage(xcb_client_message_event_t *event);
    void handleButtonPress(xcb_button_press_event_t *event);
    void handleExpose(xcb_expose_event_t *event);
    void updateSize(xcb_configure_notify_event_t *event);
    void createCursor(const QImage &img, const QPoint &hotspot);

    xcb_connection_t *m_connection = nullptr;
    xcb_screen_t *m_screen = nullptr;
    xcb_key_symbols_t *m_keySymbols = nullptr;
    int m_screenNumber = 0;
    struct Output {
        xcb_window_t window = XCB_WINDOW_NONE;
        QSize size;
        qreal scale = 1;
        QPoint xPosition;
        QPoint internalPosition;
        NETWinInfo *winInfo = nullptr;
    };
    QVector<Output> m_windows;
    xcb_atom_t m_protocols = XCB_ATOM_NONE;
    xcb_atom_t m_deleteWindowProtocol = XCB_ATOM_NONE;
    xcb_cursor_t m_cursor = XCB_CURSOR_NONE;
    Display *m_display = nullptr;
    bool m_keyboardGrabbed = false;
};

}

#endif
