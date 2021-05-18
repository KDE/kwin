/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clientbuffer_x11.h"
#include "clientbufferref.h"
#include "main.h"
#include "toplevel.h"
#include "utils.h"

namespace KWin
{

ClientBufferX11::ClientBufferX11(const Toplevel *window)
{
    XServerGrabber grabber;
    xcb_connection_t *connection = kwinApp()->x11Connection();
    xcb_window_t frame = window->frameId();
    xcb_pixmap_t pixmap = xcb_generate_id(connection);
    xcb_void_cookie_t cookie = xcb_composite_name_window_pixmap_checked(connection, frame, pixmap);
    Xcb::WindowAttributes windowAttributes(frame);
    Xcb::WindowGeometry windowGeometry(frame);
    if (xcb_generic_error_t *error = xcb_request_check(connection, cookie)) {
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x (error code %d)",
                frame, error->error_code);
        free(error);
        return;
    }
    // check that the received pixmap is valid and actually matches what we
    // know about the window (i.e. size)
    if (!windowAttributes || windowAttributes->map_state != XCB_MAP_STATE_VIEWABLE) {
        qCDebug(KWIN_CORE, "Failed to create window pixmap for window 0x%x (not viewable)", frame);
        xcb_free_pixmap(connection, pixmap);
        return;
    }

    m_pixmap = pixmap;
    m_size = windowGeometry.size();
    m_visual = window->visual();
    m_hasAlphaChannel = window->hasAlpha();
}

ClientBufferX11::~ClientBufferX11()
{
    if (m_pixmap != XCB_PIXMAP_NONE) {
        xcb_free_pixmap(kwinApp()->x11Connection(), m_pixmap);
    }
}

void ClientBufferX11::release()
{
    delete this;
}

QSize ClientBufferX11::size() const
{
    return m_size;
}

bool ClientBufferX11::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

xcb_pixmap_t ClientBufferX11::pixmap() const
{
    return m_pixmap;
}

xcb_visualid_t ClientBufferX11::visual() const
{
    return m_visual;
}

ClientBufferX11 *ClientBufferX11::from(const ClientBufferRef &ref)
{
    return qobject_cast<ClientBufferX11 *>(ref.internalBuffer());
}

} // namespace KWin
