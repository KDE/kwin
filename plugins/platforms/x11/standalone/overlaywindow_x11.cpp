/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

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

#include "overlaywindow_x11.h"

#include "kwinglobals.h"
#include "screens.h"
#include "utils.h"
#include "xcbutils.h"

#include "assert.h"

#include <QVector>

#include <xcb/composite.h>
#include <xcb/shape.h>
#if XCB_COMPOSITE_MAJOR_VERSION > 0 || XCB_COMPOSITE_MINOR_VERSION >= 3
#define KWIN_HAVE_XCOMPOSITE_OVERLAY
#endif

namespace KWin {
OverlayWindowX11::OverlayWindowX11()
    : OverlayWindow()
    , m_visible(true)
    , m_shown(false)
    , m_window(XCB_WINDOW_NONE)
{
}

OverlayWindowX11::~OverlayWindowX11()
{
}

bool OverlayWindowX11::create()
{
    assert(m_window == XCB_WINDOW_NONE);
    if (!Xcb::Extensions::self()->isCompositeOverlayAvailable())
        return false;
    if (!Xcb::Extensions::self()->isShapeInputAvailable())  // needed in setupOverlay()
        return false;
#ifdef KWIN_HAVE_XCOMPOSITE_OVERLAY
    Xcb::OverlayWindow overlay(rootWindow());
    if (overlay.isNull()) {
        return false;
    }
    m_window = overlay->overlay_win;
    if (m_window == XCB_WINDOW_NONE)
        return false;
    resize(screens()->size());
    return true;
#else
    return false;
#endif
}

void OverlayWindowX11::setup(xcb_window_t window)
{
    assert(m_window != XCB_WINDOW_NONE);
    assert(Xcb::Extensions::self()->isShapeInputAvailable());
    setNoneBackgroundPixmap(m_window);
    m_shape = QRegion();
    const QSize &s = screens()->size();
    setShape(QRect(0, 0, s.width(), s.height()));
    if (window != XCB_WINDOW_NONE) {
        setNoneBackgroundPixmap(window);
        setupInputShape(window);
    }
    const uint32_t eventMask = XCB_EVENT_MASK_VISIBILITY_CHANGE;
    xcb_change_window_attributes(connection(), m_window, XCB_CW_EVENT_MASK, &eventMask);
}

void OverlayWindowX11::setupInputShape(xcb_window_t window)
{
    xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED, window, 0, 0, 0, NULL);
}

void OverlayWindowX11::setNoneBackgroundPixmap(xcb_window_t window)
{
    const uint32_t mask = XCB_BACK_PIXMAP_NONE;
    xcb_change_window_attributes(connection(), window, XCB_CW_BACK_PIXMAP, &mask);
}

void OverlayWindowX11::show()
{
    assert(m_window != XCB_WINDOW_NONE);
    if (m_shown)
        return;
    xcb_map_subwindows(connection(), m_window);
    xcb_map_window(connection(), m_window);
    m_shown = true;
}

void OverlayWindowX11::hide()
{
    assert(m_window != XCB_WINDOW_NONE);
    xcb_unmap_window(connection(), m_window);
    m_shown = false;
    const QSize &s = screens()->size();
    setShape(QRect(0, 0, s.width(), s.height()));
}

void OverlayWindowX11::setShape(const QRegion& reg)
{
    // Avoid setting the same shape again, it causes flicker (apparently it is not a no-op
    // and triggers something).
    if (reg == m_shape)
        return;
    QVector< QRect > rects = reg.rects();
    xcb_rectangle_t *xrects = new xcb_rectangle_t[rects.count()];
    for (int i = 0;
            i < rects.count();
            ++i) {
        xrects[ i ].x = rects[ i ].x();
        xrects[ i ].y = rects[ i ].y();
        xrects[ i ].width = rects[ i ].width();
        xrects[ i ].height = rects[ i ].height();
    }
    xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, XCB_CLIP_ORDERING_UNSORTED,
                         m_window, 0, 0, rects.count(), xrects);
    delete[] xrects;
    setupInputShape(m_window);
    m_shape = reg;
}

void OverlayWindowX11::resize(const QSize &size)
{
    assert(m_window != XCB_WINDOW_NONE);
    const uint32_t geometry[2] = {
        static_cast<uint32_t>(size.width()),
        static_cast<uint32_t>(size.height())
    };
    xcb_configure_window(connection(), m_window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, geometry);
    setShape(QRegion(0, 0, size.width(), size.height()));
}

bool OverlayWindowX11::isVisible() const
{
    return m_visible;
}

void OverlayWindowX11::setVisibility(bool visible)
{
    m_visible = visible;
}

void OverlayWindowX11::destroy()
{
    if (m_window == XCB_WINDOW_NONE)
        return;
    // reset the overlay shape
    const QSize &s = screens()->size();
    xcb_rectangle_t rec = { 0, 0, static_cast<uint16_t>(s.width()), static_cast<uint16_t>(s.height()) };
    xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, XCB_CLIP_ORDERING_UNSORTED, m_window, 0, 0, 1, &rec);
    xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED, m_window, 0, 0, 1, &rec);
#ifdef KWIN_HAVE_XCOMPOSITE_OVERLAY
    xcb_composite_release_overlay_window(connection(), m_window);
#endif
    m_window = XCB_WINDOW_NONE;
    m_shown = false;
}

xcb_window_t OverlayWindowX11::window() const
{
    return m_window;
}

} // namespace KWin

