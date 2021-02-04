/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xwaylandclient.h"

#include <KWaylandServer/surface_interface.h>

using namespace KWaylandServer;

namespace KWin
{

XwaylandClient::XwaylandClient()
{
    // The wayland surface is associated with the Xwayland window asynchronously.
    connect(this, &Toplevel::surfaceChanged, this, &XwaylandClient::associate);
}

void XwaylandClient::associate()
{
    if (surface()->isMapped()) {
        initialize();
    } else {
        // Queued connection because we want to mark the window ready for painting after
        // the associated surface item has processed the new surface state.
        connect(surface(), &SurfaceInterface::mapped, this, &XwaylandClient::initialize, Qt::QueuedConnection);
    }
}

void XwaylandClient::initialize()
{
    if (!readyForPainting()) { // avoid "setReadyForPainting()" function calling overhead
        if (syncRequest().counter == XCB_NONE) {  // cannot detect complete redraw, consider done now
            setReadyForPainting();
            setupWindowManagementInterface();
        }
    }
}

bool XwaylandClient::wantsSyncCounter() const
{
    // When the frame window is resized, the attached buffer will be destroyed by
    // Xwayland, causing unexpected invalid previous and current window pixmaps.
    // With the addition of multiple window buffers in Xwayland 1.21, X11 clients
    // are no longer able to destroy the buffer after it's been committed and not
    // released by the compositor yet.
    static const quint32 xwaylandVersion = xcb_get_setup(connection())->release_number;
    return xwaylandVersion >= 12100000;
}

} // namespace KWin
