/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/graphicsbuffer.h"
#include "qwayland-server-single-pixel-buffer-v1.h"

#include <QObject>

namespace KWin
{

class Display;

class SinglePixelBufferManagerV1 : public QObject, private QtWaylandServer::wp_single_pixel_buffer_manager_v1
{
public:
    explicit SinglePixelBufferManagerV1(Display *display, QObject *parent);

private:
    void wp_single_pixel_buffer_manager_v1_destroy(Resource *resource) override;
    void wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(Resource *resource, uint32_t id, uint32_t r, uint32_t g, uint32_t b, uint32_t a) override;
};

class SinglePixelClientBuffer : public GraphicsBuffer
{
    Q_OBJECT
public:
    explicit SinglePixelClientBuffer(uint32_t r, uint32_t g, uint32_t b, uint32_t a, wl_client *client, uint32_t id);

    Map map(MapFlags flags) override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const SinglePixelAttributes *singlePixelAttributes() const override;

    static SinglePixelClientBuffer *get(wl_resource *resource);

private:
    static void buffer_destroy_resource(wl_resource *resource);
    static void buffer_destroy(wl_client *client, wl_resource *resource);
    static const struct wl_buffer_interface implementation;

    const SinglePixelAttributes m_attributes;
    uint32_t m_argb8888;
    wl_resource *m_resource;
};

}
