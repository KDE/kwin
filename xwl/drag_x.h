/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_DRAG_X
#define KWIN_XWL_DRAG_X

#include "drag.h"

#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/dataoffer.h>

#include <KWaylandServer/datadevicemanager_interface.h>

#include <QPoint>
#include <QPointer>
#include <QVector>

namespace KWayland
{
namespace Client
{
class DataSource;
}
}

namespace KWin
{
class Toplevel;
class AbstractClient;

namespace Xwl
{
class X11Source;
enum class DragEventReply;
class WlVisit;

using Mimes = QVector<QPair<QString, xcb_atom_t> >;

class XToWlDrag : public Drag
{
    Q_OBJECT

public:
    explicit XToWlDrag(X11Source *source);
    ~XToWlDrag() override;

    DragEventReply moveFilter(Toplevel *target, const QPoint &pos) override;
    bool handleClientMessage(xcb_client_message_event_t *event) override;

    void setDragAndDropAction(DnDAction action);
    DnDAction selectedDragAndDropAction();

    bool end() override {
        return false;
    }
    X11Source *x11Source() const {
        return m_source;
    }

private:
    void setOffers(const Mimes &offers);
    void offerCallback(const QString &mime);
    void setDragTarget();

    bool checkForFinished();

    KWayland::Client::DataSource *m_dataSource;

    Mimes m_offers;
    Mimes m_offersPending;

    X11Source *m_source;
    QVector<QPair<xcb_timestamp_t, bool> > m_dataRequests;

    WlVisit *m_visit = nullptr;
    QVector<WlVisit *> m_oldVisits;

    bool m_performed = false;
    DnDAction m_lastSelectedDragAndDropAction = DnDAction::None;

    Q_DISABLE_COPY(XToWlDrag)
};

class WlVisit : public QObject
{
    Q_OBJECT

public:
    WlVisit(AbstractClient *target, XToWlDrag *drag);
    ~WlVisit() override;

    bool handleClientMessage(xcb_client_message_event_t *event);
    bool leave();

    AbstractClient *target() const {
        return m_target;
    }
    xcb_window_t window() const {
        return m_window;
    }
    bool entered() const {
        return m_entered;
    }
    bool dropHandled() const {
        return m_dropHandled;
    }
    bool finished() const {
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

    AbstractClient *m_target;
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

#endif
