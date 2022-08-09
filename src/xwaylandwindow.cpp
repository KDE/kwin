/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xwaylandwindow.h"
#include "wayland/surface_interface.h"

using namespace KWaylandServer;

namespace KWin
{

XwaylandWindow::XwaylandWindow()
{
    // The wayland surface is associated with the Xwayland window asynchronously.
    connect(this, &Window::surfaceChanged, this, &XwaylandWindow::associate);
}

void XwaylandWindow::associate()
{
    if (surface()->isMapped()) {
        initialize();
    } else {
        // Queued connection because we want to mark the window ready for painting after
        // the associated surface item has processed the new surface state.
        connect(surface(), &SurfaceInterface::mapped, this, &XwaylandWindow::initialize, Qt::QueuedConnection);
    }
}

void XwaylandWindow::initialize()
{
    if (!readyForPainting()) { // avoid "setReadyForPainting()" function calling overhead
        if (syncRequest().counter == XCB_NONE) { // cannot detect complete redraw, consider done now
            setReadyForPainting();
        }
    }
}

bool XwaylandWindow::wantsSyncCounter() const
{
    // When the frame window is resized, the attached buffer will be destroyed by
    // Xwayland, causing unexpected invalid previous and current window pixmaps.
    // With the addition of multiple window buffers in Xwayland 1.21, X11 clients
    // are no longer able to destroy the buffer after it's been committed and not
    // released by the compositor yet.
    static const quint32 xwaylandVersion = xcb_get_setup(kwinApp()->x11Connection())->release_number;
    return xwaylandVersion >= 12100000;
}

} // namespace KWin
