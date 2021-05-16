/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "window.h"
#include "clientbuffer_internal.h"
#include "eglhelpers.h"
#include "platform.h"
#include "screens.h"
#include "swapchain.h"

#include "internal_client.h"

#include <logging.h>

#include <QOpenGLFramebufferObject>
#include <qpa/qwindowsysteminterface.h>

namespace KWin
{
namespace QPA
{
static quint32 s_windowId = 0;

Window::Window(QWindow *window)
    : QPlatformWindow(window)
    , m_eglDisplay(kwinApp()->platform()->sceneEglDisplay())
    , m_windowId(++s_windowId)
    , m_scale(std::max(qreal(1), screens()->maxScale()))
{
    if (window->surfaceType() == QSurface::OpenGLSurface) {
        // The window will use OpenGL for drawing.
        if (!kwinApp()->platform()->supportsSurfacelessContext()) {
            createPbuffer();
        }
    }
}

Window::~Window()
{
    if (m_eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(m_eglDisplay, m_eglSurface);
    }
    unmap();
}

void Window::setVisible(bool visible)
{
    if (visible) {
        map();
    } else {
        unmap();
    }

    QPlatformWindow::setVisible(visible);
}

QSurfaceFormat Window::format() const
{
    return m_format;
}

void Window::setGeometry(const QRect &rect)
{
    const QRect &oldRect = geometry();
    QPlatformWindow::setGeometry(rect);
    if (rect.x() != oldRect.x()) {
        emit window()->xChanged(rect.x());
    }
    if (rect.y() != oldRect.y()) {
        emit window()->yChanged(rect.y());
    }
    if (rect.width() != oldRect.width()) {
        emit window()->widthChanged(rect.width());
    }
    if (rect.height() != oldRect.height()) {
        emit window()->heightChanged(rect.height());
    }

    const QSize nativeSize = rect.size() * m_scale;

    if (m_swapchain) {
        if (m_swapchain->size() != nativeSize) {
            m_swapchain.reset();
        }
    }
    QWindowSystemInterface::handleGeometryChange(window(), geometry());
}

WId Window::winId() const
{
    return m_windowId;
}

qreal Window::devicePixelRatio() const
{
    return m_scale;
}

ClientBufferInternal *Window::backbuffer() const
{
    return m_backBuffer;
}

ClientBufferInternal *Window::swap()
{
    if (!m_swapchain) {
        const QSize nativeSize = geometry().size() * m_scale;
        m_swapchain.reset(new Swapchain(nativeSize, m_scale));
    }

    m_backBuffer = m_swapchain->acquire();
    return m_backBuffer;
}

InternalClient *Window::client() const
{
    return m_handle;
}

void Window::createPbuffer()
{
    const QSurfaceFormat requestedFormat = window()->requestedFormat();
    const EGLConfig config = configFromFormat(m_eglDisplay,
                                              requestedFormat,
                                              EGL_PBUFFER_BIT);
    if (config == EGL_NO_CONFIG_KHR) {
        qCWarning(KWIN_QPA) << "Could not find any EGL config for:" << requestedFormat;
        return;
    }

    // The size doesn't matter as we render into a framebuffer object.
    const EGLint attribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };

    m_eglSurface = eglCreatePbufferSurface(m_eglDisplay, config, attribs);
    if (m_eglSurface != EGL_NO_SURFACE) {
        m_format = formatFromConfig(m_eglDisplay, config);
    } else {
        qCWarning(KWIN_QPA, "Failed to create a pbuffer for window: 0x%x", eglGetError());
    }
}

void Window::map()
{
    if (m_handle) {
        return;
    }

    m_handle = new InternalClient(window());
}

void Window::unmap()
{
    if (!m_handle) {
        return;
    }

    m_handle->destroyClient();
    m_handle = nullptr;
    m_swapchain.reset();
}

EGLSurface Window::eglSurface() const
{
    return m_eglSurface;
}

}
}
