/*
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    Based on the libweston implementation,
    SPDX-FileCopyrightText: 2014, 2015 Collabora, Ltd.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once
#include "clientbuffer_p.h"
#include "display.h"
#include "display_p.h"
#include "drm_fourcc.h"
#include "linuxdmabufv1clientbuffer.h"

#include "qwayland-server-linux-dmabuf-unstable-v1.h"
#include "qwayland-server-wayland.h"

#include <QDebug>
#include <QVector>

#include <unistd.h>

namespace KWaylandServer
{

class LinuxDmaBufV1ClientBufferIntegrationPrivate : public QtWaylandServer::zwp_linux_dmabuf_v1
{
public:
    LinuxDmaBufV1ClientBufferIntegrationPrivate(LinuxDmaBufV1ClientBufferIntegration *q, Display *display);

    LinuxDmaBufV1ClientBufferIntegration *q;
    LinuxDmaBufV1ClientBufferIntegration::RendererInterface *rendererInterface = nullptr;
    QHash<uint32_t, QSet<uint64_t>> supportedModifiers;

protected:
    void zwp_linux_dmabuf_v1_bind_resource(Resource *resource) override;
    void zwp_linux_dmabuf_v1_destroy(Resource *resource) override;
    void zwp_linux_dmabuf_v1_create_params(Resource *resource, uint32_t params_id) override;
};

class LinuxDmaBufV1ClientBufferPrivate : public ClientBufferPrivate, public QtWaylandServer::wl_buffer
{
public:
    QSize size;
    quint32 format;
    quint32 flags;
    QVector<LinuxDmaBufV1Plane> planes;
    bool hasAlphaChannel = false;

protected:
    void buffer_destroy(Resource *resource) override;
};

class LinuxDmaBufParamsV1 : public QtWaylandServer::zwp_linux_buffer_params_v1
{
public:
    LinuxDmaBufParamsV1(LinuxDmaBufV1ClientBufferIntegration *integration, ::wl_resource *resource);
    ~LinuxDmaBufParamsV1() override;

protected:
    void zwp_linux_buffer_params_v1_destroy_resource(Resource *resource) override;
    void zwp_linux_buffer_params_v1_destroy(Resource *resource) override;
    void zwp_linux_buffer_params_v1_add(Resource *resource,
                                        int32_t fd,
                                        uint32_t plane_idx,
                                        uint32_t offset,
                                        uint32_t stride,
                                        uint32_t modifier_hi,
                                        uint32_t modifier_lo) override;
    void zwp_linux_buffer_params_v1_create(Resource *resource, int32_t width, int32_t height, uint32_t format, uint32_t flags) override;
    void
    zwp_linux_buffer_params_v1_create_immed(Resource *resource, uint32_t buffer_id, int32_t width, int32_t height, uint32_t format, uint32_t flags) override;

private:
    bool test(Resource *resource, uint32_t width, uint32_t height);

    LinuxDmaBufV1ClientBufferIntegration *m_integration;
    QVector<LinuxDmaBufV1Plane> m_planes;
    int m_planeCount = 0;
    bool m_isUsed = false;
};

}
