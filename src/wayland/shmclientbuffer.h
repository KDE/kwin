/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "core/graphicsbuffer.h"

struct wl_resource;

namespace KWaylandServer
{

class Display;
class ShmClientBufferPrivate;

/**
 * The ShmClientBuffer class represents a wl_shm_buffer client buffer.
 *
 * The buffer's data can be accessed using the data() function. Note that it is not allowed
 * to access data of several shared memory buffers simultaneously.
 */
class KWIN_EXPORT ShmClientBuffer : public KWin::GraphicsBuffer
{
    Q_OBJECT

public:
    explicit ShmClientBuffer(wl_resource *resource);
    ~ShmClientBuffer() override;

    QImage data() const;

    QSize size() const override;
    bool hasAlphaChannel() const override;

    static ShmClientBuffer *get(wl_resource *resource);

private:
    std::unique_ptr<ShmClientBufferPrivate> d;
};

/**
 * The ShmClientBufferIntegration class provides support for wl_shm_buffer buffers.
 */
class ShmClientBufferIntegration : public QObject
{
    Q_OBJECT

public:
    explicit ShmClientBufferIntegration(Display *display);
};

} // namespace KWaylandServer
