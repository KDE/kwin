/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screens_xrandr.h"
#include "x11_platform.h"

#ifndef KWIN_UNIT_TEST
#include "composite.h"
#include "options.h"
#include "workspace.h"
#endif
#include "xcbutils.h"


namespace KWin
{

XRandRScreens::XRandRScreens(X11StandalonePlatform *backend, QObject *parent)
    : OutputScreens(backend, parent)
    , X11EventFilter(Xcb::Extensions::self()->randrNotifyEvent())
    , m_backend(backend)
{
}

XRandRScreens::~XRandRScreens() = default;

void XRandRScreens::init()
{
    KWin::Screens::init();
    // we need to call ScreenResources at least once to be able to use current
    m_backend->initOutputs();
    setCount(m_backend->outputs().count());
    emit changed();

#ifndef KWIN_UNIT_TEST
    connect(this, &XRandRScreens::changed, this, [] {
        if (!workspace()->compositing()) {
            return;
        }
        if (Compositor::self()->xrrRefreshRate() == Options::currentRefreshRate()) {
            return;
        }
        // desktopResized() should take care of when the size or
        // shape of the desktop has changed, but we also want to
        // catch refresh rate changes
        Compositor::self()->reinitialize();
    });
#endif
}

void XRandRScreens::updateCount()
{
    m_backend->updateOutputs();
    setCount(m_backend->outputs().count());
}

bool XRandRScreens::event(xcb_generic_event_t *event)
{
    Q_ASSERT((event->response_type & ~0x80) == Xcb::Extensions::self()->randrNotifyEvent());
    // let's try to gather a few XRandR events, unlikely that there is just one
    startChangedTimer();

    // update default screen
    auto *xrrEvent = reinterpret_cast<xcb_randr_screen_change_notify_event_t*>(event);
    xcb_screen_t *screen = defaultScreen();
    if (xrrEvent->rotation & (XCB_RANDR_ROTATION_ROTATE_90 | XCB_RANDR_ROTATION_ROTATE_270)) {
        screen->width_in_pixels = xrrEvent->height;
        screen->height_in_pixels = xrrEvent->width;
        screen->width_in_millimeters = xrrEvent->mheight;
        screen->height_in_millimeters = xrrEvent->mwidth;
    } else {
        screen->width_in_pixels = xrrEvent->width;
        screen->height_in_pixels = xrrEvent->height;
        screen->width_in_millimeters = xrrEvent->mwidth;
        screen->height_in_millimeters = xrrEvent->mheight;
    }

    return false;
}

QSize XRandRScreens::displaySize() const
{
    xcb_screen_t *screen = defaultScreen();
    if (!screen) {
        return Screens::size();
    }
    return QSize(screen->width_in_pixels, screen->height_in_pixels);
}

} // namespace
