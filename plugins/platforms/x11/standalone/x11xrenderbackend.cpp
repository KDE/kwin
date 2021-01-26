/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11xrenderbackend.h"
#include "main.h"
#include "platform.h"
#include "overlaywindow.h"
#include "renderloop_p.h"
#include "scene.h"
#include "screens.h"
#include "softwarevsyncmonitor.h"
#include "utils.h"
#include "x11_platform.h"

#include "kwinxrenderutils.h"

namespace KWin
{

X11XRenderBackend::X11XRenderBackend(X11StandalonePlatform *backend)
    : XRenderBackend()
    , m_backend(backend)
    , m_overlayWindow(backend->createOverlayWindow())
    , m_front(XCB_RENDER_PICTURE_NONE)
    , m_format(0)
{
    // Fallback to software vblank events for now. Maybe use the Present extension or
    // something to get notified when the overlay window is actually presented?
    m_vsyncMonitor = SoftwareVsyncMonitor::create(this);
    connect(backend->renderLoop(), &RenderLoop::refreshRateChanged, this, [this, backend]() {
        m_vsyncMonitor->setRefreshRate(backend->renderLoop()->refreshRate());
    });
    m_vsyncMonitor->setRefreshRate(backend->renderLoop()->refreshRate());

    connect(m_vsyncMonitor, &VsyncMonitor::vblankOccurred, this, &X11XRenderBackend::vblank);

    init(true);
}

X11XRenderBackend::~X11XRenderBackend()
{
    // No completion events will be received for in-flight frames, this may lock the
    // render loop. We need to ensure that the render loop is back to its initial state
    // if the render backend is about to be destroyed.
    RenderLoopPrivate::get(kwinApp()->platform()->renderLoop())->invalidate();

    if (m_front) {
        xcb_render_free_picture(connection(), m_front);
    }
    m_overlayWindow->destroy();
}

OverlayWindow *X11XRenderBackend::overlayWindow()
{
    return m_overlayWindow.data();
}

void X11XRenderBackend::showOverlay()
{
    if (m_overlayWindow->window()) { // show the window only after the first pass, since
        m_overlayWindow->show();   // that pass may take long
    }
}

void X11XRenderBackend::init(bool createOverlay)
{
    if (m_front != XCB_RENDER_PICTURE_NONE)
        xcb_render_free_picture(connection(), m_front);
    bool haveOverlay = createOverlay ? m_overlayWindow->create() : (m_overlayWindow->window() != XCB_WINDOW_NONE);
    if (haveOverlay) {
        m_overlayWindow->setup(XCB_WINDOW_NONE);
        ScopedCPointer<xcb_get_window_attributes_reply_t> attribs(xcb_get_window_attributes_reply(connection(),
            xcb_get_window_attributes_unchecked(connection(), m_overlayWindow->window()), nullptr));
        if (!attribs) {
            setFailed("Failed getting window attributes for overlay window");
            return;
        }
        m_format = XRenderUtils::findPictFormat(attribs->visual);
        if (m_format == 0) {
            setFailed("Failed to find XRender format for overlay window");
            return;
        }
        m_front = xcb_generate_id(connection());
        xcb_render_create_picture(connection(), m_front, m_overlayWindow->window(), m_format, 0, nullptr);
    } else {
        // create XRender picture for the root window
        m_format = XRenderUtils::findPictFormat(kwinApp()->x11DefaultScreen()->root_visual);
        if (m_format == 0) {
            setFailed("Failed to find XRender format for root window");
            return; // error
        }
        m_front = xcb_generate_id(connection());
        const uint32_t values[] = {XCB_SUBWINDOW_MODE_INCLUDE_INFERIORS};
        xcb_render_create_picture(connection(), m_front, rootWindow(), m_format, XCB_RENDER_CP_SUBWINDOW_MODE, values);
    }
    createBuffer();
}

void X11XRenderBackend::createBuffer()
{
    xcb_pixmap_t pixmap = xcb_generate_id(connection());
    const auto displaySize = screens()->displaySize();
    xcb_create_pixmap(connection(), Xcb::defaultDepth(), pixmap, rootWindow(), displaySize.width(), displaySize.height());
    xcb_render_picture_t b = xcb_generate_id(connection());
    xcb_render_create_picture(connection(), b, pixmap, m_format, 0, nullptr);
    xcb_free_pixmap(connection(), pixmap);   // The picture owns the pixmap now
    setBuffer(b);
}

void X11XRenderBackend::present(int mask, const QRegion &damage)
{
    m_vsyncMonitor->arm();

    const auto displaySize = screens()->displaySize();
    if (mask & Scene::PAINT_SCREEN_REGION) {
        // Use the damage region as the clip region for the root window
        XFixesRegion frontRegion(damage);
        xcb_xfixes_set_picture_clip_region(connection(), m_front, frontRegion, 0, 0);
        // copy composed buffer to the root window
        xcb_xfixes_set_picture_clip_region(connection(), buffer(), XCB_XFIXES_REGION_NONE, 0, 0);
        xcb_render_composite(connection(), XCB_RENDER_PICT_OP_SRC, buffer(), XCB_RENDER_PICTURE_NONE,
                             m_front, 0, 0, 0, 0, 0, 0, displaySize.width(), displaySize.height());
        xcb_xfixes_set_picture_clip_region(connection(), m_front, XCB_XFIXES_REGION_NONE, 0, 0);
    } else {
        // copy composed buffer to the root window
        xcb_render_composite(connection(), XCB_RENDER_PICT_OP_SRC, buffer(), XCB_RENDER_PICTURE_NONE,
                             m_front, 0, 0, 0, 0, 0, 0, displaySize.width(), displaySize.height());
    }

    xcb_flush(connection());
}

void X11XRenderBackend::vblank(std::chrono::nanoseconds timestamp)
{
    RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_backend->renderLoop());
    renderLoopPrivate->notifyFrameCompleted(timestamp);
}

void X11XRenderBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    init(false);
}

} // namespace KWin
