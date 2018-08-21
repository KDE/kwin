/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2018 Roman Gilg <subdiff@gmail.com>

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
#ifndef KWIN_XWL_DATABRIDGE
#define KWIN_XWL_DATABRIDGE

#include <QObject>

#include <xcb/xcb.h>

class xcb_xfixes_selection_notify_event_t;

namespace KWayland {
namespace Client {
class DataDevice;
}
namespace Server {
class DataDeviceInterface;
class SurfaceInterface;
}
}

namespace KWin
{

namespace Xwl
{
class Xwayland;
class Clipboard;

/*
 * Interface class for all data sharing in the context of X selections
 * and Wayland's internal mechanism.
 *
 * Exists only once per Xwayland session.
 */
class DataBridge : public QObject
{
    Q_OBJECT
public:
    static DataBridge* self();

    explicit DataBridge(QObject *parent = nullptr);
    ~DataBridge();

    bool filterEvent(xcb_generic_event_t *event);

    KWayland::Client::DataDevice *dataDevice() const {
        return m_dd;
    }
    KWayland::Server::DataDeviceInterface *dataDeviceIface() const {
        return m_ddi;
    }

private:
    void init();

    bool handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event);

    Clipboard *m_clipboard = nullptr;

    /* Internal data device interface */
    KWayland::Client::DataDevice *m_dd = nullptr;
    KWayland::Server::DataDeviceInterface *m_ddi = nullptr;
};

}
}

#endif
