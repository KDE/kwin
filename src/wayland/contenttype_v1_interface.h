/*
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "surface_interface.h"

#include "qwayland-server-content-type-v1.h"

namespace KWaylandServer
{

class ContentTypeV1Interface;
class Display;

class ContentTypeManagerV1Interface : public QObject, private QtWaylandServer::wp_content_type_manager_v1
{
public:
    ContentTypeManagerV1Interface(Display *display, QObject *parent = nullptr);

private:
    void wp_content_type_manager_v1_destroy(QtWaylandServer::wp_content_type_manager_v1::Resource *resource) override;
    void wp_content_type_manager_v1_get_surface_content_type(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

class ContentTypeV1Interface : public QObject, private QtWaylandServer::wp_content_type_v1
{
    Q_OBJECT
public:
    ContentTypeV1Interface(SurfaceInterface *surface, wl_client *client, uint32_t id);

private:
    void wp_content_type_v1_set_content_type(QtWaylandServer::wp_content_type_v1::Resource *resource, uint32_t content_type) override;
    void wp_content_type_v1_destroy(QtWaylandServer::wp_content_type_v1::Resource *resource) override;
    void wp_content_type_v1_destroy_resource(QtWaylandServer::wp_content_type_v1::Resource *resource) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
