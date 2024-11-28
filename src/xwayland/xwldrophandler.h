/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wayland/datadevice.h"

#include <xcb/xcb.h>

namespace KWin
{
namespace Xwl
{

class Xvisit;
class Dnd;

class XwlDropHandler : public AbstractDropHandler
{
    Q_OBJECT
public:
    XwlDropHandler(Dnd *dnd);

    void updateDragTarget(SurfaceInterface *surface, quint32 serial) override;
    bool handleClientMessage(xcb_client_message_event_t *event);

private:
    void drop() override;

    Dnd *const m_dnd;
    QList<Xvisit *> m_visits;
    Xvisit *m_currentVisit = nullptr;
};
}
}
