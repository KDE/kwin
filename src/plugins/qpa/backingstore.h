/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <epoxy/egl.h>

#include <qpa/qplatformbackingstore.h>

namespace KWin
{
namespace QPA
{

class BackingStoreSlot;

class BackingStore : public QPlatformBackingStore
{
public:
    explicit BackingStore(QWindow *window);

    QPaintDevice *paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    void beginPaint(const QRegion &region) override;

private:
    std::shared_ptr<BackingStoreSlot> allocate(const QSize &size);
    std::shared_ptr<BackingStoreSlot> acquire();

    std::vector<std::shared_ptr<BackingStoreSlot>> m_slots;
    std::shared_ptr<BackingStoreSlot> m_backBuffer;
    QSize m_requestedSize;
    qreal m_requestedDevicePixelRatio = 1;
};

}
}
