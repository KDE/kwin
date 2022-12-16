/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "selection.h"

#include "wayland/datadevicemanager_interface.h"

#include <QPoint>

namespace KWaylandServer
{
class SurfaceInterface;
}

namespace KWin
{
class Window;

namespace Xwl
{
class Drag;
enum class DragEventReply;

class XwlDropHandler;

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
    XwlDropHandler *dropHandler() const;

    void doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event) override;
    void x11OffersChanged(const QStringList &added, const QStringList &removed) override;
    bool handleClientMessage(xcb_client_message_event_t *event) override;

    DragEventReply dragMoveFilter(Window *target, const QPoint &pos);

    using DnDAction = KWaylandServer::DataDeviceManagerInterface::DnDAction;
    using DnDActions = KWaylandServer::DataDeviceManagerInterface::DnDActions;
    static DnDAction atomToClientAction(xcb_atom_t atom);
    static xcb_atom_t clientActionToAtom(DnDAction action);

private:
    // start and end Wl native client drags (Wl -> Xwl)
    void startDrag();
    void endDrag();
    void clearOldDrag(Drag *drag);

    // active drag or null when no drag active
    Drag *m_currentDrag = nullptr;
    QVector<Drag *> m_oldDrags;

    XwlDropHandler *m_dropHandler;

    Q_DISABLE_COPY(Dnd)
};

} // namespace Xwl
} // namespace KWin
