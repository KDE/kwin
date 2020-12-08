/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "dnd.h"

#include "databridge.h"
#include "drag_wl.h"
#include "drag_x.h"
#include "selection_source.h"

#include "abstract_client.h"
#include "atoms.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xwayland.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/surface.h>

#include <KWaylandServer/compositor_interface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/datasource_interface.h>

#include <QMouseEvent>

#include <xcb/xcb.h>

namespace KWin
{
namespace Xwl
{

// version of DnD support in X
const static uint32_t s_version = 5;
uint32_t Dnd::version()
{
    return s_version;
}

Dnd::Dnd(xcb_atom_t atom, QObject *parent)
    : Selection(atom, parent)
{
    xcb_connection_t *xcbConn = kwinApp()->x11Connection();

    const uint32_t dndValues[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                                   XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_create_window(xcbConn,
                      XCB_COPY_FROM_PARENT,
                      window(),
                      kwinApp()->x11RootWindow(),
                      0, 0,
                      8192, 8192,           // TODO: get current screen size and connect to changes
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      XCB_CW_EVENT_MASK,
                      dndValues);
    registerXfixes();

    xcb_change_property(xcbConn,
                        XCB_PROP_MODE_REPLACE,
                        window(),
                        atoms->xdnd_aware,
                        XCB_ATOM_ATOM,
                        32, 1, &s_version);
    xcb_flush(xcbConn);

    connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::dragStarted, this, &Dnd::startDrag);
    connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::dragEnded, this, &Dnd::endDrag);

    const auto *comp = waylandServer()->compositor();
    m_surface = waylandServer()->internalCompositor()->createSurface(this);
    m_surface->setInputRegion(nullptr);
    m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    auto *dc = new QMetaObject::Connection();
    *dc = connect(comp, &KWaylandServer::CompositorInterface::surfaceCreated, this,
                 [this, dc](KWaylandServer::SurfaceInterface *si) {
                    // TODO: how to make sure that it is the iface of m_surface?
                    if (m_surfaceIface || si->client() != waylandServer()->internalConnection()) {
                        return;
                    }
                    QObject::disconnect(*dc);
                    delete dc;
                    m_surfaceIface = si;
                    connect(workspace(), &Workspace::clientActivated, this,
                        [this](AbstractClient *ac) {
                            if (!ac || !ac->inherits("KWin::X11Client")) {
                                return;
                            }
                            auto *surface = ac->surface();
                            if (surface) {
                                surface->setDataProxy(m_surfaceIface);
                            } else {
                                auto *dc = new QMetaObject::Connection();
                                *dc = connect(ac, &AbstractClient::surfaceChanged, this, [this, ac, dc] {
                                        if (auto *surface = ac->surface()) {
                                            surface->setDataProxy(m_surfaceIface);
                                            QObject::disconnect(*dc);
                                            delete dc;
                                        }
                                      }
                                );
                            }
                    });
                }
    );
    waylandServer()->dispatch();
}

void Dnd::doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    if (qobject_cast<XToWlDrag *>(m_currentDrag)) {
        // X drag is in progress, rogue X client took over the selection.
        return;
    }
    if (m_currentDrag) {
        // Wl drag is in progress - don't overwrite by rogue X client,
        // get it back instead!
        ownSelection(true);
        return;
    }
    createX11Source(nullptr);
    const auto *seat = waylandServer()->seat();
    auto *originSurface = seat->focusedPointerSurface();
    if (!originSurface) {
        return;
    }
    if (originSurface->client() != waylandServer()->xWaylandConnection()) {
        // focused surface client is not Xwayland - do not allow drag to start
        // TODO: can we make this stronger (window id comparison)?
        return;
    }
    if (!seat->isPointerButtonPressed(Qt::LeftButton)) {
        // we only allow drags to be started on (left) pointer button being
        // pressed for now
        return;
    }
    createX11Source(event);
    X11Source *source = x11Source();
    if (!source) {
        return;
    }
    DataBridge::self()->dataDeviceIface()->updateProxy(originSurface);
    m_currentDrag = new XToWlDrag(source);
}

void Dnd::x11OffersChanged(const QStringList &added, const QStringList &removed)
{
    Q_UNUSED(added);
    Q_UNUSED(removed);
    // TODO: handled internally
}

bool Dnd::handleClientMessage(xcb_client_message_event_t *event)
{
    for (Drag *drag : m_oldDrags) {
        if (drag->handleClientMessage(event)) {
            return true;
        }
    }
    if (m_currentDrag && m_currentDrag->handleClientMessage(event)) {
        return true;
    }
    return false;
}

DragEventReply Dnd::dragMoveFilter(Toplevel *target, const QPoint &pos)
{
    // This filter only is used when a drag is in process.
    Q_ASSERT(m_currentDrag);
    return m_currentDrag->moveFilter(target, pos);
}

void Dnd::startDrag()
{
    auto *ddi = waylandServer()->seat()->dragSource();
    if (ddi == DataBridge::self()->dataDeviceIface()) {
        // X to Wl drag, started by us, is in progress.
        Q_ASSERT(m_currentDrag);
        return;
    }

    // There can only ever be one Wl native drag at the same time.
    Q_ASSERT(!m_currentDrag);

    // New Wl to X drag, init drag and Wl source.
    m_currentDrag = new WlToXDrag();
    auto source = new WlSource(this);
    source->setDataSourceIface(ddi->dragSource());
    setWlSource(source);
    ownSelection(true);
}

void Dnd::endDrag()
{
    Q_ASSERT(m_currentDrag);

    if (m_currentDrag->end()) {
        delete m_currentDrag;
    } else {
        connect(m_currentDrag, &Drag::finish, this, &Dnd::clearOldDrag);
        m_oldDrags << m_currentDrag;
    }
    m_currentDrag = nullptr;
}

void Dnd::clearOldDrag(Drag *drag)
{
    m_oldDrags.removeOne(drag);
    delete drag;
}

} // namespace Xwl
} // namespace KWin
