/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "window_property_notify_x11_filter.h"
#include "client.h"
#include "effects.h"
#include "unmanaged.h"
#include "workspace.h"

namespace KWin
{

WindowPropertyNotifyX11Filter::WindowPropertyNotifyX11Filter(EffectsHandlerImpl *effects)
    : X11EventFilter(QVector<int>{XCB_PROPERTY_NOTIFY})
    , m_effects(effects)
{
}

bool WindowPropertyNotifyX11Filter::event(xcb_generic_event_t *event)
{
    const auto *pe = reinterpret_cast<xcb_property_notify_event_t*>(event);
    if (!m_effects->isPropertyTypeRegistered(pe->atom)) {
        return false;
    }
    if (pe->window == kwinApp()->x11RootWindow()) {
        emit m_effects->propertyNotify(nullptr, pe->atom);
    } else if (const auto c = workspace()->findClient(Predicate::WindowMatch, pe->window)) {
        emit m_effects->propertyNotify(c->effectWindow(), pe->atom);
    } else if (const auto c = workspace()->findUnmanaged(pe->window)) {
        emit m_effects->propertyNotify(c->effectWindow(), pe->atom);
    }
    return false;
}

}
