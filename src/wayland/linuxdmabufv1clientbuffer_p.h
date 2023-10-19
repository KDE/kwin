/*
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    Based on the libweston implementation,
    SPDX-FileCopyrightText: 2014, 2015 Collabora, Ltd.

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "display.h"
#include "linuxdmabufv1clientbuffer.h"
#include "utils/ramfile.h"

#include "qwayland-server-linux-dmabuf-unstable-v1.h"

#include <QDebug>
#include <QList>
#include <QPointer>

#include <drm_fourcc.h>

namespace KWin
{

class LinuxDmaBufV1FormatTable;

class LinuxDmaBufV1ClientBufferIntegrationPrivate : public QtWaylandServer::zwp_linux_dmabuf_v1
{
public:
    LinuxDmaBufV1ClientBufferIntegrationPrivate(LinuxDmaBufV1ClientBufferIntegration *q, Display *display);

    LinuxDmaBufV1ClientBufferIntegration *q;
    std::unique_ptr<LinuxDmaBufV1Feedback> defaultFeedback;
    std::unique_ptr<LinuxDmaBufV1FormatTable> table;
    dev_t mainDevice;
    QPointer<RenderBackend> renderBackend;
    QHash<uint32_t, QList<uint64_t>> supportedModifiers;

protected:
    void zwp_linux_dmabuf_v1_bind_resource(Resource *resource) override;
    void zwp_linux_dmabuf_v1_destroy(Resource *resource) override;
    void zwp_linux_dmabuf_v1_create_params(Resource *resource, uint32_t params_id) override;
    void zwp_linux_dmabuf_v1_get_default_feedback(Resource *resource, uint32_t id) override;
    void zwp_linux_dmabuf_v1_get_surface_feedback(Resource *resource, uint32_t id, wl_resource *surface) override;
};

class LinuxDmaBufParamsV1 : public QtWaylandServer::zwp_linux_buffer_params_v1
{
public:
    LinuxDmaBufParamsV1(LinuxDmaBufV1ClientBufferIntegration *integration, ::wl_resource *resource);

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
    DmaBufAttributes m_attrs;
    bool m_isUsed = false;
};

class LinuxDmaBufV1ClientBuffer : public GraphicsBuffer
{
    Q_OBJECT

public:
    LinuxDmaBufV1ClientBuffer(DmaBufAttributes &&attrs);

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const DmaBufAttributes *dmabufAttributes() const override;

    static LinuxDmaBufV1ClientBuffer *get(wl_resource *resource);

private:
    void initialize(wl_resource *resource);

    static void buffer_destroy_resource(wl_resource *resource);
    static void buffer_destroy(wl_client *client, wl_resource *resource);
    static const struct wl_buffer_interface implementation;

    wl_resource *m_resource = nullptr;
    DmaBufAttributes m_attrs;
    bool m_hasAlphaChannel = false;

    friend class LinuxDmaBufParamsV1;
};

class LinuxDmaBufV1FormatTable
{
public:
    LinuxDmaBufV1FormatTable(const QHash<uint32_t, QList<uint64_t>> &supportedModifiers);

    RamFile file;
    QMap<std::pair<uint32_t, uint64_t>, uint16_t> indices;
};

class LinuxDmaBufV1FeedbackPrivate : public QtWaylandServer::zwp_linux_dmabuf_feedback_v1
{
public:
    LinuxDmaBufV1FeedbackPrivate(LinuxDmaBufV1ClientBufferIntegrationPrivate *bufferintegration);

    static LinuxDmaBufV1FeedbackPrivate *get(LinuxDmaBufV1Feedback *q);
    void send(Resource *resource);

    QList<LinuxDmaBufV1Feedback::Tranche> m_tranches;
    LinuxDmaBufV1ClientBufferIntegrationPrivate *m_bufferintegration;

protected:
    void zwp_linux_dmabuf_feedback_v1_bind_resource(Resource *resource) override;
    void zwp_linux_dmabuf_feedback_v1_destroy(Resource *resource) override;
};
}
