/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_DRAG
#define KWIN_XWL_DRAG

#include <KWayland/Client/datadevicemanager.h>

#include <QPoint>

#include <xcb/xcb.h>

namespace KWin
{
class Toplevel;

namespace Xwl
{
enum class DragEventReply;

using DnDAction = KWayland::Client::DataDeviceManager::DnDAction;

/**
 * An ongoing drag operation.
 */
class Drag : public QObject
{
    Q_OBJECT

public:
    explicit Drag(QObject *parent = nullptr);
    ~Drag() override;

    static void sendClientMessage(xcb_window_t target, xcb_atom_t type, xcb_client_message_data_t *data);
    static DnDAction atomToClientAction(xcb_atom_t atom);
    static xcb_atom_t clientActionToAtom(DnDAction action);

    virtual bool handleClientMessage(xcb_client_message_event_t *event) = 0;
    virtual DragEventReply moveFilter(Toplevel *target, const QPoint &pos) = 0;

    virtual bool end() = 0;

Q_SIGNALS:
    void finish(Drag *self);

private:
    Q_DISABLE_COPY(Drag)
};

} // namespace Xwl
} // namespace KWin

#endif
