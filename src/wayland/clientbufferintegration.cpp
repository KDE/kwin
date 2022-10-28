/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "clientbufferintegration.h"
#include "display.h"
#include "display_p.h"

namespace KWaylandServer
{
ClientBufferIntegration::ClientBufferIntegration(Display *display)
    : QObject(display)
    , m_display(display)
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->bufferIntegrations.append(this);
}

ClientBufferIntegration::~ClientBufferIntegration()
{
    if (m_display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(m_display);
        displayPrivate->bufferIntegrations.removeOne(this);
    }
}

Display *ClientBufferIntegration::display() const
{
    return m_display;
}

ClientBuffer *ClientBufferIntegration::createBuffer(wl_resource *resource)
{
    return nullptr;
}

} // namespace KWaylandServer
