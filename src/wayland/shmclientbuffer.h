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

/**
 * The ShmClientBuffer class represents a wl_shm_buffer client buffer.
 *
 * The buffer's data can be accessed using the data() function. Note that it is not allowed
 * to access data of several shared memory buffers simultaneously.
 */
class KWIN_EXPORT ShmClientBuffer : public ClientBuffer
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ShmClientBuffer)

public:
    explicit ShmClientBuffer(wl_resource *resource);

    QImage data() const;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    Origin origin() const override;
};

/**
 * The ShmClientBufferIntegration class provides support for wl_shm_buffer buffers.
 */
class ShmClientBufferIntegration : public ClientBufferIntegration
{
    Q_OBJECT

public:
    explicit ShmClientBufferIntegration(Display *display);

    ClientBuffer *createBuffer(::wl_resource *resource) override;
};

} // namespace KWaylandServer
