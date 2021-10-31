/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_ON_X_BACKEND_H
#define KWIN_EGL_ON_X_BACKEND_H
#include "abstract_egl_backend.h"

#include <xcb/xcb.h>

#include <X11/Xlib-xcb.h>
#include <fixx11h.h>

namespace KWin
{

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class KWIN_EXPORT EglOnXBackend : public AbstractEglBackend
{
    Q_OBJECT

public:
    EglOnXBackend(Display *display);
    explicit EglOnXBackend(xcb_connection_t *connection, Display *display, xcb_window_t rootWindow, int screenNumber, xcb_window_t renderingWindow);
    ~EglOnXBackend() override;
    OverlayWindow* overlayWindow() const override;
    void init() override;

protected:
    virtual bool createSurfaces();
    EGLSurface createSurface(xcb_window_t window);
    void setHavePlatformBase(bool have) {
        m_havePlatformBase = have;
    }
    bool havePlatformBase() const {
        return m_havePlatformBase;
    }
    bool havePostSubBuffer() const {
        return surfaceHasSubPost;
    }
    bool makeContextCurrent(const EGLSurface &surface);

private:
    bool initBufferConfigs();
    bool initRenderingContext();
    /**
     * @brief The OverlayWindow used by this Backend.
     */
    OverlayWindow *m_overlayWindow;
    int surfaceHasSubPost;
    xcb_connection_t *m_connection;
    Display *m_x11Display;
    xcb_window_t m_rootWindow;
    int m_x11ScreenNumber;
    xcb_window_t m_renderingWindow = XCB_WINDOW_NONE;
    bool m_havePlatformBase = false;
};

} // namespace

#endif //  KWIN_EGL_ON_X_BACKEND_H
