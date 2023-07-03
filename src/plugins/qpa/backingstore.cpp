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
    return &m_buffer;
}

void BackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    if (m_buffer.size() == size) {
        return;
    }

    const QPlatformWindow *platformWindow = static_cast<QPlatformWindow *>(window()->handle());
    const qreal devicePixelRatio = platformWindow->devicePixelRatio();

    m_buffer = QImage(size * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    m_buffer.setDevicePixelRatio(devicePixelRatio);
}

void BackingStore::beginPaint(const QRegion &region)
{
    if (m_buffer.hasAlphaChannel()) {
        QPainter p(paintDevice());
        p.setCompositionMode(QPainter::CompositionMode_Source);
        const QColor blank = Qt::transparent;
        for (const QRect &rect : region) {
            p.fillRect(rect, blank);
        }
    }
}

void BackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Window *platformWindow = static_cast<Window *>(window->handle());
    InternalWindow *internalWindow = platformWindow->internalWindow();
    if (!internalWindow) {
        return;
    }

    QRegion bufferDamage;
    for (const QRect &rect : region) {
        bufferDamage |= QRect(std::floor(rect.x() * m_buffer.devicePixelRatio()),
                              std::floor(rect.y() * m_buffer.devicePixelRatio()),
                              std::ceil(rect.width() * m_buffer.devicePixelRatio()),
                              std::ceil(rect.height() * m_buffer.devicePixelRatio()));
    }

    internalWindow->present(m_buffer, bufferDamage);
}

}
}
