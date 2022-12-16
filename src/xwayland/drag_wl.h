/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drag.h"

#include "wayland/datadevicemanager_interface.h"

#include <QPoint>
#include <QPointer>
#include <QVector>

namespace KWaylandServer
{
class DataDeviceInterface;
class DataSourceInterface;
class SurfaceInterface;
}

namespace KWin
{
class Window;

namespace Xwl
{
class X11Source;
enum class DragEventReply;
class Xvisit;
class Dnd;

class WlToXDrag : public Drag
{
    Q_OBJECT
    using Drag::Drag;

public:
    explicit WlToXDrag(Dnd *dnd);
    DragEventReply moveFilter(Window *target, const QPoint &pos) override;
    bool handleClientMessage(xcb_client_message_event_t *event) override;

private:
    Dnd *const m_dnd;
    Q_DISABLE_COPY(WlToXDrag)
};

// visit to an X window
class Xvisit : public QObject
{
    Q_OBJECT

public:
    // TODO: handle ask action

    Xvisit(Window *target, KWaylandServer::AbstractDataSource *dataSource, Dnd *dnd, QObject *parent);

    bool handleClientMessage(xcb_client_message_event_t *event);
    bool handleStatus(xcb_client_message_event_t *event);
    bool handleFinished(xcb_client_message_event_t *event);

    void sendPosition(const QPointF &globalPos);
    void leave();

    bool finished() const
    {
        return m_state.finished;
    }
    Window *target() const
    {
        return m_target;
    }
    void drop();
Q_SIGNALS:
    void finish(Xvisit *self);

private:
    void sendEnter();
    void sendDrop(uint32_t time);
    void sendLeave();

    void receiveOffer();
    void enter();

    void retrieveSupportedActions();
    void determineProposedAction();
    void requestDragAndDropAction();
    void setProposedAction();

    void doFinish();
    void stopConnections();

    Dnd *const m_dnd;
    Window *m_target;
    QPointer<KWaylandServer::AbstractDataSource> m_dataSource;
    uint32_t m_version = 0;

    QMetaObject::Connection m_motionConnection;

    struct
    {
        bool pending = false;
        bool cached = false;
        QPoint cache;
    } m_pos;

    // supported by the Wl source
    KWaylandServer::DataDeviceManagerInterface::DnDActions m_supportedActions = KWaylandServer::DataDeviceManagerInterface::DnDAction::None;
    // preferred by the X client
    KWaylandServer::DataDeviceManagerInterface::DnDAction m_preferredAction = KWaylandServer::DataDeviceManagerInterface::DnDAction::None;
    // decided upon by the compositor
    KWaylandServer::DataDeviceManagerInterface::DnDAction m_proposedAction = KWaylandServer::DataDeviceManagerInterface::DnDAction::None;

    struct
    {
        bool entered = false;
        bool dropped = false;
        bool finished = false;
    } m_state;

    bool m_accepts = false;

    Q_DISABLE_COPY(Xvisit)
};

} // namespace Xwl
} // namespace KWin
