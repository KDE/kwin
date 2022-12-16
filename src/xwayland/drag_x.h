/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "datasource.h"
#include "drag.h"

#include "wayland/datadevicemanager_interface.h"

#include <QPoint>
#include <QPointer>
#include <QVector>

namespace KWin
{
class Window;

namespace Xwl
{
class X11Source;
enum class DragEventReply;
class WlVisit;
class Dnd;

using Mimes = QVector<QPair<QString, xcb_atom_t>>;

class XToWlDrag : public Drag
{
    Q_OBJECT

public:
    explicit XToWlDrag(X11Source *source, Dnd *dnd);
    ~XToWlDrag() override;

    DragEventReply moveFilter(Window *target, const QPoint &pos) override;
    bool handleClientMessage(xcb_client_message_event_t *event) override;

    void setDragAndDropAction(KWaylandServer::DataDeviceManagerInterface::DnDAction action);
    KWaylandServer::DataDeviceManagerInterface::DnDAction selectedDragAndDropAction();

    X11Source *x11Source() const
    {
        return m_source;
    }

private:
    void setOffers(const Mimes &offers);
    void setDragTarget();

    bool checkForFinished();

    Dnd *const m_dnd;
    Mimes m_offers;

    XwlDataSource m_selectionSource;

    X11Source *m_source;
    QVector<QPair<xcb_timestamp_t, bool>> m_dataRequests;

    WlVisit *m_visit = nullptr;
    QVector<WlVisit *> m_oldVisits;

    bool m_performed = false;
    KWaylandServer::DataDeviceManagerInterface::DnDAction m_lastSelectedDragAndDropAction = KWaylandServer::DataDeviceManagerInterface::DnDAction::None;

    Q_DISABLE_COPY(XToWlDrag)
};

class WlVisit : public QObject
{
    Q_OBJECT

public:
    WlVisit(Window *target, XToWlDrag *drag, Dnd *dnd);
    ~WlVisit() override;

    bool handleClientMessage(xcb_client_message_event_t *event);
    bool leave();

    Window *target() const
    {
        return m_target;
    }
    xcb_window_t window() const
    {
        return m_window;
    }
    bool entered() const
    {
        return m_entered;
    }
    bool dropHandled() const
    {
        return m_dropHandled;
    }
    bool finished() const
    {
        return m_finished;
    }
    void sendFinished();

Q_SIGNALS:
    void offersReceived(const Mimes &offers);
    void finish(WlVisit *self);

private:
    bool handleEnter(xcb_client_message_event_t *event);
    bool handlePosition(xcb_client_message_event_t *event);
    bool handleDrop(xcb_client_message_event_t *event);
    bool handleLeave(xcb_client_message_event_t *event);

    void sendStatus();

    void getMimesFromWinProperty(Mimes &offers);

    bool targetAcceptsAction() const;

    void doFinish();
    void unmapProxyWindow();

    Dnd *const m_dnd;
    Window *m_target;
    xcb_window_t m_window;

    xcb_window_t m_srcWindow = XCB_WINDOW_NONE;
    XToWlDrag *m_drag;

    uint32_t m_version = 0;

    xcb_atom_t m_actionAtom;
    KWaylandServer::DataDeviceManagerInterface::DnDAction m_action = KWaylandServer::DataDeviceManagerInterface::DnDAction::None;

    bool m_mapped = false;
    bool m_entered = false;
    bool m_dropHandled = false;
    bool m_finished = false;

    Q_DISABLE_COPY(WlVisit)
};

} // namespace Xwl
} // namespace KWin
