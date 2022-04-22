/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "clientbuffer.h"
#include "clientbufferintegration.h"

namespace KWaylandServer
{
class DrmClientBufferPrivate;

/**
 * The DrmClientBufferIntegration class provides support for wl_drm client buffers.
 */
class KWIN_EXPORT DrmClientBufferIntegration : public ClientBufferIntegration
{
    Q_OBJECT

public:
    explicit DrmClientBufferIntegration(Display *display);

    ClientBuffer *createBuffer(::wl_resource *resource) override;
};

/**
 * The DrmClientBuffer class represents a wl_drm client buffer.
 *
 * Nowadays, the wl_drm protocol is de-facto deprecated with the introduction of the
 * linux-dmabuf-v1 protocol. Note that Vulkan WSI in Mesa still prefers wl_drm, but
 * that's about to change, https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/4942/
 */
class KWIN_EXPORT DrmClientBuffer : public ClientBuffer
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(DrmClientBuffer)

public:
    explicit DrmClientBuffer(wl_resource *resource, DrmClientBufferIntegration *integration);

    int textureFormat() const;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    Origin origin() const override;
};

} // namespace KWaylandServer
