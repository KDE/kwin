/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "drag.h"

#include "wayland/datadevicemanager.h"

#include <QList>
#include <QPoint>
#include <QPointer>

namespace KWin
{
class DataDeviceInterface;
class DataSourceInterface;
class SurfaceInterface;
class Window;
class X11Window;

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
    DragEventReply moveFilter(Window *target) override;
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

    Xvisit(X11Window *target, AbstractDataSource *dataSource, Dnd *dnd, QObject *parent);

    bool handleClientMessage(xcb_client_message_event_t *event);
    bool handleStatus(xcb_client_message_event_t *event);
    bool handleFinished(xcb_client_message_event_t *event);

    void sendPosition(const QPointF &globalPos);
    void leave();

    bool isFinished() const
    {
        return m_state.finished;
    }
    X11Window *target() const
    {
        return m_target;
    }
    void drop();

Q_SIGNALS:
    void finished();

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
    X11Window *m_target;
    QPointer<AbstractDataSource> m_dataSource;
    uint32_t m_version = 0;

    QMetaObject::Connection m_motionConnection;

    struct
    {
        bool pending = false;
        bool cached = false;
        QPoint cache;
    } m_pos;

    // supported by the Wl source
    DataDeviceManagerInterface::DnDActions m_supportedActions = DataDeviceManagerInterface::DnDAction::None;
    // preferred by the X client
    DataDeviceManagerInterface::DnDAction m_preferredAction = DataDeviceManagerInterface::DnDAction::None;
    // decided upon by the compositor
    DataDeviceManagerInterface::DnDAction m_proposedAction = DataDeviceManagerInterface::DnDAction::None;

    struct
    {
        bool entered = false;
        bool dropRequested = false;
        bool dropCompleted = false;
        bool finished = false;
    } m_state;

    bool m_accepts = false;

    Q_DISABLE_COPY(Xvisit)
};

} // namespace Xwl
} // namespace KWin
