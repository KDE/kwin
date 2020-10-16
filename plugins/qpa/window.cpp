/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "window.h"
#include "eglhelpers.h"
#include "platform.h"
#include "screens.h"

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
    , m_scale(screens()->maxScale())
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

    if (m_contentFBO) {
        if (m_contentFBO->size() != nativeSize) {
            m_resized = true;
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

void Window::bindContentFBO()
{
    if (m_resized || !m_contentFBO) {
        createFBO();
    }
    m_contentFBO->bind();
}

const QSharedPointer<QOpenGLFramebufferObject> &Window::contentFBO() const
{
    return m_contentFBO;
}

QSharedPointer<QOpenGLFramebufferObject> Window::swapFBO()
{
    QSharedPointer<QOpenGLFramebufferObject> fbo = m_contentFBO;
    m_contentFBO.clear();
    return fbo;
}

InternalClient *Window::client() const
{
    return m_handle;
}

void Window::createFBO()
{
    const QRect &r = geometry();
    if (m_contentFBO && r.size().isEmpty()) {
        return;
    }
    const QSize nativeSize = r.size() * m_scale;
    m_contentFBO.reset(new QOpenGLFramebufferObject(nativeSize.width(), nativeSize.height(), QOpenGLFramebufferObject::CombinedDepthStencil));
    if (!m_contentFBO->isValid()) {
        qCWarning(KWIN_QPA) << "Content FBO is not valid";
    }
    m_resized = false;
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

    m_contentFBO = nullptr;
}

EGLSurface Window::eglSurface() const
{
    return m_eglSurface;
}

}
}
