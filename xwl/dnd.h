/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_DND
#define KWIN_XWL_DND

#include "selection.h"

#include <QPoint>

namespace KWayland
{
namespace Client
{
class Surface;
}
}
namespace KWaylandServer
{
class SurfaceInterface;
}

namespace KWin
{
class Toplevel;

namespace Xwl
{
class Drag;
enum class DragEventReply;

/**
 * Represents the drag and drop mechanism, on X side this is the XDND protocol.
 * For more information on XDND see: https://johnlindal.wixsite.com/xdnd
 */
class Dnd : public Selection
{
    Q_OBJECT

public:
    explicit Dnd(xcb_atom_t atom, QObject *parent);

    static uint32_t version();

    void doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event) override;
    void x11OffersChanged(const QStringList &added, const QStringList &removed) override;
    bool handleClientMessage(xcb_client_message_event_t *event) override;

    DragEventReply dragMoveFilter(Toplevel *target, const QPoint &pos);

    KWaylandServer::SurfaceInterface *surfaceIface() const {
        return m_surfaceIface;
    }
    KWayland::Client::Surface *surface() const {
        return m_surface;
    }

private:
    // start and end Wl native client drags (Wl -> Xwl)
    void startDrag();
    void endDrag();
    void clearOldDrag(Drag *drag);

    // active drag or null when no drag active
    Drag *m_currentDrag = nullptr;
    QVector<Drag *> m_oldDrags;

    KWayland::Client::Surface *m_surface;
    KWaylandServer::SurfaceInterface *m_surfaceIface = nullptr;

    Q_DISABLE_COPY(Dnd)
};

} // namespace Xwl
} // namespace KWin

#endif
