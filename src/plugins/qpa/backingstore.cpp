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
#include "core/shmgraphicsbufferallocator.h"
#include "internalwindow.h"
#include "window.h"

#include <QPainter>

#include <libdrm/drm_fourcc.h>

namespace KWin
{
namespace QPA
{

class BackingStoreSlot
{
public:
    explicit BackingStoreSlot(GraphicsBuffer *graphicsBuffer);
    ~BackingStoreSlot();

    GraphicsBuffer *graphicsBuffer;
    std::unique_ptr<GraphicsBufferView> graphicsBufferView;
};

BackingStoreSlot::BackingStoreSlot(GraphicsBuffer *graphicsBuffer)
    : graphicsBuffer(graphicsBuffer)
    , graphicsBufferView(std::make_unique<GraphicsBufferView>(graphicsBuffer, GraphicsBuffer::Read | GraphicsBuffer::Write))
{
}

BackingStoreSlot::~BackingStoreSlot()
{
    graphicsBufferView.reset();
    graphicsBuffer->drop();
}

BackingStore::BackingStore(QWindow *window)
    : QPlatformBackingStore(window)
{
}

QPaintDevice *BackingStore::paintDevice()
{
    return m_backBuffer->graphicsBufferView->image();
}

std::shared_ptr<BackingStoreSlot> BackingStore::allocate(const QSize &size)
{
    ShmGraphicsBufferAllocator allocator;

    GraphicsBuffer *buffer = allocator.allocate(GraphicsBufferOptions{
        .size = size,
        .format = DRM_FORMAT_ARGB8888,
        .software = true,
    });
    if (!buffer) {
        return nullptr;
    }

    auto slot = std::make_shared<BackingStoreSlot>(buffer);
    m_slots.push_back(slot);

    return slot;
}

std::shared_ptr<BackingStoreSlot> BackingStore::acquire()
{
    const QSize bufferSize = m_requestedSize * m_requestedDevicePixelRatio;
    if (!m_slots.empty()) {
        const auto front = m_slots.front();
        if (front->graphicsBuffer->size() == bufferSize) {
            for (const auto &slot : m_slots) {
                if (!slot->graphicsBuffer->isReferenced()) {
                    return slot;
                }
            }
            return allocate(bufferSize);
        }
    }

    m_slots.clear();
    return allocate(bufferSize);
}

void BackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    m_requestedSize = size;

    const QPlatformWindow *platformWindow = static_cast<QPlatformWindow *>(window()->handle());
    m_requestedDevicePixelRatio = platformWindow->devicePixelRatio();
}

void BackingStore::beginPaint(const QRegion &region)
{
    const auto oldBackBuffer = m_backBuffer;
    m_backBuffer = acquire();

    if (oldBackBuffer && oldBackBuffer != m_backBuffer && oldBackBuffer->graphicsBuffer->size() == m_backBuffer->graphicsBuffer->size()) {
        const GraphicsBufferView *oldView = oldBackBuffer->graphicsBufferView.get();
        GraphicsBufferView *view = m_backBuffer->graphicsBufferView.get();
        std::memcpy(view->image()->bits(), oldView->image()->constBits(), oldView->image()->sizeInBytes());
    }

    QImage *image = m_backBuffer->graphicsBufferView->image();
    image->setDevicePixelRatio(m_requestedDevicePixelRatio);

    if (image->hasAlphaChannel()) {
        QPainter p(image);
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
        bufferDamage |= QRect(std::floor(rect.x() * m_requestedDevicePixelRatio),
                              std::floor(rect.y() * m_requestedDevicePixelRatio),
                              std::ceil(rect.width() * m_requestedDevicePixelRatio),
                              std::ceil(rect.height() * m_requestedDevicePixelRatio));
    }

    internalWindow->present(m_backBuffer->graphicsBuffer, bufferDamage);
}

}
}
