/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// this needs to be on top, epoxy has an error if you include it after GL/gl.h,
// which Qt does include
#include "utils/drm_format_helper.h"

#include "window.h"
#include "composite.h"
#include "core/renderbackend.h"
#include "core/shmgraphicsbufferallocator.h"
#include "internalwindow.h"
#include "swapchain.h"

#include <logging.h>

#include <libdrm/drm_fourcc.h>
#include <qpa/qwindowsysteminterface.h>

namespace KWin
{
namespace QPA
{
static quint32 s_windowId = 0;

Window::Window(QWindow *window)
    : QPlatformWindow(window)
    , m_windowId(++s_windowId)
    , m_scale(kwinApp()->devicePixelRatio())
{
    Q_ASSERT(!window->property("_KWIN_WINDOW_IS_OFFSCREEN").toBool());
}

Window::~Window()
{
    unmap();
}

Swapchain *Window::swapchain(const QHash<uint32_t, QVector<uint64_t>> &formats)
{
    const QSize nativeSize = geometry().size() * devicePixelRatio();
    if (!m_swapchain || m_swapchain->size() != nativeSize
        || !formats.contains(m_swapchain->format())
        || m_swapchain->modifiers() != formats[m_swapchain->format()]) {
        const bool software = window()->surfaceType() == QSurface::RasterSurface; // RasterGLSurface is unsupported by us

        GraphicsBufferAllocator *allocator;
        if (software) {
            static ShmGraphicsBufferAllocator shmAllocator;
            allocator = &shmAllocator;
        } else {
            allocator = Compositor::self()->backend()->graphicsBufferAllocator();
        }

        for (auto it = formats.begin(); it != formats.end(); it++) {
            if (auto info = formatInfo(it.key()); info && info->bitsPerColor == 8 && info->alphaBits == 8) {
                const auto options = GraphicsBufferOptions{
                    .size = nativeSize,
                    .format = it.key(),
                    .modifiers = it.value(),
                    .software = software,
                };
                auto buffer = allocator->allocate(options);
                if (!buffer) {
                    continue;
                }
                m_swapchain = std::make_unique<Swapchain>(allocator, options, buffer);
                break;
            }
        }
    }
    return m_swapchain.get();
}

void Window::invalidateSurface()
{
    m_swapchain.reset();
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

InternalWindow *Window::internalWindow() const
{
    return m_handle;
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

    invalidateSurface();
}

}
}
