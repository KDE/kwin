/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "core/graphicsbuffer.h"

#include <QImage>
#include <QObject>
#include <QSize>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{
class ClientBufferPrivate;

/**
 * The ClientBuffer class represents a client buffer.
 *
 * While the ClientBuffer is referenced, it won't be destroyed. Note that the client can
 * still destroy the wl_buffer object while the ClientBuffer is referenced by the compositor.
 * You can use the isDestroyed() function to check whether the wl_buffer object has been
 * destroyed.
 */
class KWIN_EXPORT ClientBuffer : public KWin::GraphicsBuffer
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ClientBuffer)

public:
    ~ClientBuffer() override;

    /**
     * Returns the wl_resource for this ClientBuffer. If the buffer is destroyed, @c null
     * will be returned.
     */
    wl_resource *resource() const;

protected:
    ClientBuffer(ClientBufferPrivate &dd);
    ClientBuffer(wl_resource *resource, ClientBufferPrivate &dd);

    void initialize(wl_resource *resource);
    std::unique_ptr<ClientBufferPrivate> d_ptr;
};

} // namespace KWaylandServer
