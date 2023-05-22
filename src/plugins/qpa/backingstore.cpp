/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "backingstore.h"
#include "window.h"

#include "internalwindow.h"

#include <QPainter>

namespace KWin
{
namespace QPA
{

BackingStore::BackingStore(QWindow *window)
    : QPlatformBackingStore(window)
{
}

BackingStore::~BackingStore() = default;

QPaintDevice *BackingStore::paintDevice()
{
    return &m_backBuffer;
}

void BackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    if (m_backBuffer.size() == size) {
        return;
    }

    const QPlatformWindow *platformWindow = static_cast<QPlatformWindow *>(window()->handle());
    const qreal devicePixelRatio = platformWindow->devicePixelRatio();

    m_backBuffer = QImage(size * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    m_backBuffer.setDevicePixelRatio(devicePixelRatio);

    m_frontBuffer = QImage(size * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    m_frontBuffer.setDevicePixelRatio(devicePixelRatio);
}

void BackingStore::beginPaint(const QRegion &region)
{
    if (m_backBuffer.hasAlphaChannel()) {
        QPainter p(paintDevice());
        p.setCompositionMode(QPainter::CompositionMode_Source);
        const QColor blank = Qt::transparent;
        for (const QRect &rect : region) {
            p.fillRect(rect, blank);
        }
    }
}

static QRect scaledRect(const QRect &rect, qreal devicePixelRatio)
{
    return QRect(rect.topLeft() * devicePixelRatio, rect.size() * devicePixelRatio);
}

static void blitImage(const QImage &source, QImage &target, const QRegion &region)
{
    QPainter painter(&target);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    for (const QRect &rect : region) {
        painter.drawImage(rect, source, scaledRect(rect, source.devicePixelRatio()));
    }
}

void BackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Window *platformWindow = static_cast<Window *>(window->handle());
    InternalWindow *internalWindow = platformWindow->internalWindow();
    if (!internalWindow) {
        return;
    }

    blitImage(m_backBuffer, m_frontBuffer, region);

    internalWindow->present(m_frontBuffer, region);
}

}
}
