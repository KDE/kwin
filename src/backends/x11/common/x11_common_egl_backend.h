/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "abstract_egl_backend.h"

#include <xcb/xcb.h>

struct _XDisplay;
typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;

namespace KWin
{

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class KWIN_EXPORT EglOnXBackend : public AbstractEglBackend
{
    Q_OBJECT

public:
    explicit EglOnXBackend(xcb_connection_t *connection, Display *display, xcb_window_t rootWindow);

    void init() override;

protected:
    virtual bool createSurfaces() = 0;
    EGLSurface createSurface(xcb_window_t window);
    void setHavePlatformBase(bool have)
    {
        m_havePlatformBase = have;
    }
    bool havePlatformBase() const
    {
        return m_havePlatformBase;
    }
    bool havePostSubBuffer() const
    {
        return surfaceHasSubPost;
    }
    bool makeContextCurrent(const EGLSurface &surface);

private:
    bool initBufferConfigs();
    bool initRenderingContext();
    int surfaceHasSubPost;
    xcb_connection_t *m_connection;
    Display *m_x11Display;
    xcb_window_t m_rootWindow;
    bool m_havePlatformBase = false;
};

} // namespace
