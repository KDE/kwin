/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "datasource.h"
#include "drag.h"
#include "wayland/abstract_data_source.h"

#include <QList>
#include <QPoint>
#include <QPointer>

namespace KWin
{
class Window;

namespace Xwl
{
class X11Source;
class WlVisit;
class Dnd;

class XToWlDrag : public Drag
{
    Q_OBJECT

public:
    explicit XToWlDrag(X11Source *source, Dnd *dnd);
    ~XToWlDrag() override;

    bool moveFilter(Window *target, const QPointF &position) override;
    bool handleClientMessage(xcb_client_message_event_t *event) override;

    void setDragAndDropAction(DnDAction action);
    DnDAction selectedDragAndDropAction();

    Dnd *selection() const
    {
        return m_dnd;
    }

private:
    void setMimeTypes(const QStringList &offers);
    void setDragTarget();
    void tryFinish();

    Dnd *const m_dnd;
    X11Source *m_source;

    WlVisit *m_visit = nullptr;
    QList<WlVisit *> m_oldVisits;

    DnDAction m_lastSelectedDragAndDropAction = DnDAction::None;

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
    bool isEntered() const
    {
        return m_entered;
    }
    bool isDropHandled() const
    {
        return m_dropHandled;
    }
    bool isFinished() const
    {
        return m_finished;
    }
    void sendFinished();

Q_SIGNALS:
    void entered(const QStringList &mimeTypes);
    void finish(WlVisit *self);

private:
    bool handleEnter(xcb_client_message_event_t *event);
    bool handlePosition(xcb_client_message_event_t *event);
    bool handleDrop(xcb_client_message_event_t *event);
    bool handleLeave(xcb_client_message_event_t *event);

    void sendStatus();

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
    DnDAction m_action = DnDAction::None;

    bool m_mapped = false;
    bool m_entered = false;
    bool m_dropHandled = false;
    bool m_finished = false;

    Q_DISABLE_COPY(WlVisit)
};

} // namespace Xwl
} // namespace KWin
