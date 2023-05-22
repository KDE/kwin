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

class BackingStore : public QPlatformBackingStore
{
public:
    explicit BackingStore(QWindow *window);
    ~BackingStore() override;

    QPaintDevice *paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    void beginPaint(const QRegion &region) override;

private:
    QImage m_backBuffer;
    QImage m_frontBuffer;
};

}
}
