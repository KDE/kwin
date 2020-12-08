/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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
        if (Compositor::self()->refreshRate() == Options::currentRefreshRate()) {
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
    xcb_screen_t *screen = kwinApp()->x11DefaultScreen();
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
    xcb_screen_t *screen = kwinApp()->x11DefaultScreen();
    if (!screen) {
        return Screens::size();
    }
    return QSize(screen->width_in_pixels, screen->height_in_pixels);
}

} // namespace
