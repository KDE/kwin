/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QPointer>

#include <epoxy/egl.h>

#include <qpa/qplatformbackingstore.h>

namespace KWin
{

class GraphicsBuffer;
class GraphicsBufferView;

namespace QPA
{

class BackingStore : public QPlatformBackingStore
{
public:
    explicit BackingStore(QWindow *window);

    QPaintDevice *paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    void beginPaint(const QRegion &region) override;
    void endPaint() override;

private:
    QPointer<GraphicsBuffer> m_buffer;
    std::unique_ptr<GraphicsBufferView> m_bufferView;
};

}
}
