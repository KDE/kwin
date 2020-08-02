/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*********************************************************************/
#include "window.h"
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
    , m_windowId(++s_windowId)
    , m_scale(screens()->maxScale())
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

}
}
