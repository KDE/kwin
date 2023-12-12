/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_overlaywindow.h"

#include "compositor.h"
#include "core/renderloop.h"
#include "scene/workspacescene.h"
#include "utils/xcbutils.h"
#include "x11_standalone_backend.h"

#include <QList>

#include <xcb/composite.h>
#include <xcb/shape.h>
#if XCB_COMPOSITE_MAJOR_VERSION > 0 || XCB_COMPOSITE_MINOR_VERSION >= 3
#define KWIN_HAVE_XCOMPOSITE_OVERLAY
#endif

namespace KWin
{
OverlayWindowX11::OverlayWindowX11(X11StandaloneBackend *backend)
    : OverlayWindow()
    , X11EventFilter(QList<int>{XCB_EXPOSE, XCB_VISIBILITY_NOTIFY})
    , m_visible(true)
    , m_shown(false)
    , m_backend(backend)
    , m_window(XCB_WINDOW_NONE)
{
}

OverlayWindowX11::~OverlayWindowX11()
{
}

bool OverlayWindowX11::create()
{
    Q_ASSERT(m_window == XCB_WINDOW_NONE);
    if (!Xcb::Extensions::self()->isCompositeOverlayAvailable()) {
        return false;
    }
    if (!Xcb::Extensions::self()->isShapeInputAvailable()) { // needed in setupOverlay()
        return false;
    }
#ifdef KWIN_HAVE_XCOMPOSITE_OVERLAY
    Xcb::OverlayWindow overlay(rootWindow());
    if (overlay.isNull()) {
        return false;
    }
    m_window = overlay->overlay_win;
    if (m_window == XCB_WINDOW_NONE) {
        return false;
    }
    return true;
#else
    return false;
#endif
}

void OverlayWindowX11::setup(xcb_window_t window)
{
    Q_ASSERT(m_window != XCB_WINDOW_NONE);
    Q_ASSERT(Xcb::Extensions::self()->isShapeInputAvailable());
    setNoneBackgroundPixmap(m_window);
    if (m_size.isValid()) {
        setShape(QRect(0, 0, m_size.width(), m_size.height()));
    }
    if (window != XCB_WINDOW_NONE) {
        setNoneBackgroundPixmap(window);
        setupInputShape(window);
    }
    const uint32_t eventMask = XCB_EVENT_MASK_VISIBILITY_CHANGE;
    xcb_change_window_attributes(connection(), m_window, XCB_CW_EVENT_MASK, &eventMask);
}

void OverlayWindowX11::setupInputShape(xcb_window_t window)
{
    xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED, window, 0, 0, 0, nullptr);
}

void OverlayWindowX11::setNoneBackgroundPixmap(xcb_window_t window)
{
    const uint32_t mask = XCB_BACK_PIXMAP_NONE;
    xcb_change_window_attributes(connection(), window, XCB_CW_BACK_PIXMAP, &mask);
}

void OverlayWindowX11::show()
{
    Q_ASSERT(m_window != XCB_WINDOW_NONE);
    if (m_shown) {
        return;
    }
    xcb_map_subwindows(connection(), m_window);
    xcb_map_window(connection(), m_window);
    m_shown = true;
}

void OverlayWindowX11::hide()
{
    Q_ASSERT(m_window != XCB_WINDOW_NONE);
    xcb_unmap_window(connection(), m_window);
    m_shown = false;
    const QSize &s = m_size;
    setShape(QRect(0, 0, s.width(), s.height()));
}

void OverlayWindowX11::setShape(const QRegion &reg)
{
    // Avoid setting the same shape again, it causes flicker (apparently it is not a no-op
    // and triggers something).
    if (reg == m_shape) {
        return;
    }
    const QList<xcb_rectangle_t> xrects = Xcb::regionToRects(reg);
    xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, XCB_CLIP_ORDERING_UNSORTED,
                         m_window, 0, 0, xrects.count(), xrects.data());
    setupInputShape(m_window);
    m_shape = reg;
}

void OverlayWindowX11::resize(const QSize &size)
{
    m_size = size;
    if (m_window == XCB_WINDOW_NONE) {
        return;
    }
    const uint32_t geometry[2] = {
        static_cast<uint32_t>(size.width()),
        static_cast<uint32_t>(size.height())};
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
    if (m_window == XCB_WINDOW_NONE) {
        return;
    }
    // reset the overlay shape
    xcb_rectangle_t rec = {0, 0, static_cast<uint16_t>(m_size.width()), static_cast<uint16_t>(m_size.height())};
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

bool OverlayWindowX11::event(xcb_generic_event_t *event)
{
    const uint8_t eventType = event->response_type & ~0x80;
    if (eventType == XCB_EXPOSE) {
        const auto *expose = reinterpret_cast<xcb_expose_event_t *>(event);
        if (expose->window == rootWindow() // root window needs repainting
            || (m_window != XCB_WINDOW_NONE && expose->window == m_window)) { // overlay needs repainting
            Compositor::self()->scene()->addRepaint(expose->x, expose->y, expose->width, expose->height);
        }
    } else if (eventType == XCB_VISIBILITY_NOTIFY) {
        const auto *visibility = reinterpret_cast<xcb_visibility_notify_event_t *>(event);
        if (m_window != XCB_WINDOW_NONE && visibility->window == m_window) {
            bool was_visible = isVisible();
            setVisibility((visibility->state != XCB_VISIBILITY_FULLY_OBSCURED));
            auto compositor = Compositor::self();
            if (!was_visible && m_visible) {
                // hack for #154825
                compositor->scene()->addRepaintFull();
                QTimer::singleShot(2000, compositor, [compositor]() {
                    if (compositor->compositing()) {
                        compositor->scene()->addRepaintFull();
                    }
                });
            }
            m_backend->renderLoop()->scheduleRepaint();
        }
    }
    return false;
}

} // namespace KWin
