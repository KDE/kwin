/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbuffer.h"
#include "clientbufferintegration.h"

namespace KWaylandServer
{
class ShmClientBufferPrivate;
class ShmClientBufferIntegrationPrivate;
class ShmPool;

/**
 * The ShmClientBuffer class represents a shared memory client buffer.
 *
 * The buffer's data can be accessed using the data() function. Note that it is not allowed
 * to access data of several shared memory buffers simultaneously.
 */
class KWIN_EXPORT ShmClientBuffer : public ClientBuffer
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ShmClientBuffer)

public:
    explicit ShmClientBuffer(ShmPool *pool, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format, wl_resource *resource);
    ~ShmClientBuffer() override;

    QImage data() const;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    Origin origin() const override;
};

/**
 * The ShmClientBufferIntegration class provides support for shared memory client buffers.
 */
class ShmClientBufferIntegration : public ClientBufferIntegration
{
    Q_OBJECT

public:
    explicit ShmClientBufferIntegration(Display *display);
    ~ShmClientBufferIntegration() override;

    QVector<uint32_t> formats() const;

private:
    friend class ShmClientBufferIntegrationPrivate;
    std::unique_ptr<ShmClientBufferIntegrationPrivate> d;
};

} // namespace KWaylandServer
