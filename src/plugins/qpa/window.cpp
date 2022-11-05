/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "window.h"
#include "core/outputbackend.h"
#include "eglhelpers.h"

#include "internalwindow.h"

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
    , m_eglDisplay(kwinApp()->outputBackend()->sceneEglDisplay())
    , m_windowId(++s_windowId)
    , m_scale(kwinApp()->devicePixelRatio())
{
}

Window::~Window()
{
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

void Window::requestActivateWindow()
{
    QWindowSystemInterface::handleWindowActivated(window());
}

void Window::setGeometry(const QRect &rect)
{
    const QRect oldGeometry = geometry();
    QPlatformWindow::setGeometry(rect);

    if (window()->isVisible() && rect.isValid()) {
        const QSize nativeSize = rect.size() * m_scale;
        if (m_contentFBO) {
            if (m_contentFBO->size() != nativeSize) {
                m_resized = true;
            }
        }
        QWindowSystemInterface::handleGeometryChange(window(), geometry());
    }

    if (isExposed() && oldGeometry.size() != rect.size()) {
        QWindowSystemInterface::handleExposeEvent(window(), QRect(QPoint(), rect.size()));
    }
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

const std::shared_ptr<QOpenGLFramebufferObject> &Window::contentFBO() const
{
    return m_contentFBO;
}

std::shared_ptr<QOpenGLFramebufferObject> Window::swapFBO()
{
    std::shared_ptr<QOpenGLFramebufferObject> fbo;
    m_contentFBO.swap(fbo);
    return fbo;
}

InternalWindow *Window::internalWindow() const
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

void Window::map()
{
    if (m_handle) {
        return;
    }

    m_handle = new InternalWindow(window());
}

void Window::unmap()
{
    if (!m_handle) {
        return;
    }

    m_handle->destroyWindow();
    m_handle = nullptr;

    m_contentFBO = nullptr;
}

EGLSurface Window::eglSurface() const
{
    return EGL_NO_SURFACE; // EGL_KHR_surfaceless_context is required.
}

}
}
