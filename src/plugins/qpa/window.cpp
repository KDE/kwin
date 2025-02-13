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

#include "compositor.h"
#include "core/drmdevice.h"
#include "core/renderbackend.h"
#include "core/shmgraphicsbufferallocator.h"
#include "internalwindow.h"
#include "swapchain.h"
#include "window.h"

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

Swapchain *Window::swapchain(const std::shared_ptr<EglContext> &context, const QHash<uint32_t, QList<uint64_t>> &formats)
{
    const QSize nativeSize = geometry().size() * devicePixelRatio();
    const bool software = window()->surfaceType() == QSurface::RasterSurface; // RasterGLSurface is unsupported by us
    if (!m_swapchain || m_swapchain->size() != nativeSize
        || !formats.contains(m_swapchain->format())
        || m_swapchain->modifiers() != formats[m_swapchain->format()]
        || (!software && m_eglContext.lock() != context)) {

        GraphicsBufferAllocator *allocator;
        if (software) {
            static ShmGraphicsBufferAllocator shmAllocator;
            allocator = &shmAllocator;
        } else {
            allocator = Compositor::self()->backend()->drmDevice()->allocator();
        }

        for (auto it = formats.begin(); it != formats.end(); it++) {
            if (auto info = FormatInfo::get(it.key()); info && info->bitsPerColor == 8 && info->alphaBits == 8) {
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
                m_eglContext = context;
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
}

QSurfaceFormat Window::format() const
{
    return m_format;
}

void Window::requestActivateWindow()
{
    QWindowSystemInterface::handleFocusWindowChanged(window());
}

void Window::raise()
{
    // Left blank intentionally to suppress warnings in QPlatformWindow::raise().
}

void Window::lower()
{
    // Left blank intentionally to suppress warnings in QPlatformWindow::lower().
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

bool Window::isExposed() const
{
    return m_exposed;
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

    m_exposed = true;
    QWindowSystemInterface::handleExposeEvent(window(), QRect(QPoint(), geometry().size()));
}

void Window::unmap()
{
    if (!m_handle) {
        return;
    }

    m_handle->destroyWindow();
    m_handle = nullptr;

    invalidateSurface();

    m_exposed = false;
    QWindowSystemInterface::handleExposeEvent(window(), QRect());
}

}
}
