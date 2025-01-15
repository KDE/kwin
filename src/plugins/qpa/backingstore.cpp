/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "backingstore.h"
#include "core/graphicsbuffer.h"
#include "core/graphicsbufferview.h"
#include "internalwindow.h"
#include "logging.h"
#include "swapchain.h"
#include "window.h"

#include <QPainter>
#include <libdrm/drm_fourcc.h>

namespace KWin
{
namespace QPA
{

BackingStore::BackingStore(QWindow *window)
    : QPlatformBackingStore(window)
{
}

QPaintDevice *BackingStore::paintDevice()
{
    return m_bufferView->image();
}

void BackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    QPlatformWindow *platformWindow = static_cast<QPlatformWindow *>(window()->handle());
    platformWindow->invalidateSurface();
}

void BackingStore::beginPaint(const QRegion &region)
{
    Window *platformWindow = static_cast<Window *>(window()->handle());
    Swapchain *swapchain = platformWindow->swapchain(nullptr, {{DRM_FORMAT_ARGB8888, {DRM_FORMAT_MOD_LINEAR}}});
    if (!swapchain) {
        qCCritical(KWIN_QPA, "Failed to ceate a swapchain for the backing store!");
        return;
    }

    const auto oldBuffer = m_buffer;
    m_buffer = swapchain->acquire();
    if (!m_buffer) {
        qCCritical(KWIN_QPA, "Failed to acquire a graphics buffer for the backing store");
        return;
    }

    m_bufferView = std::make_unique<GraphicsBufferView>(m_buffer, GraphicsBuffer::Read | GraphicsBuffer::Write);
    if (m_bufferView->isNull()) {
        qCCritical(KWIN_QPA) << "Failed to map a graphics buffer for the backing store";
        return;
    }

    if (oldBuffer && oldBuffer != m_buffer && oldBuffer->size() == m_buffer->size()) {
        const GraphicsBufferView oldView(oldBuffer, GraphicsBuffer::Read);
        std::memcpy(m_bufferView->image()->bits(), oldView.image()->constBits(), oldView.image()->sizeInBytes());
    }

    QImage *image = m_bufferView->image();
    image->setDevicePixelRatio(platformWindow->devicePixelRatio());

    if (image->hasAlphaChannel()) {
        QPainter p(image);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        const QColor blank = Qt::transparent;
        for (const QRect &rect : region) {
            p.fillRect(rect, blank);
        }
    }
}

void BackingStore::endPaint()
{
    m_bufferView.reset();
}

void BackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Window *platformWindow = static_cast<Window *>(window->handle());
    InternalWindow *internalWindow = platformWindow->internalWindow();
    if (!internalWindow) {
        return;
    }

    const qreal scale = platformWindow->devicePixelRatio();
    const QRect bufferRect(QPoint(0, 0), m_buffer->size());
    QRegion bufferDamage;
    for (const QRect &rect : region) {
        bufferDamage += QRectF(rect.x() * scale, rect.y() * scale, rect.width() * scale, rect.height() * scale)
                            .toAlignedRect()
                            .intersected(bufferRect);
    }

    internalWindow->present(InternalWindowFrame{
        .buffer = m_buffer,
        .bufferDamage = bufferDamage,
    });
}

}
}
