/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "qwayland-server-ext-blur-v1.h"

#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;

class KWIN_EXPORT ExtBlurManagerV1 : public QObject, private QtWaylandServer::ext_blur_manager_v1
{
    Q_OBJECT
public:
    explicit ExtBlurManagerV1(Display *display, QObject *parent);

    void remove();

private:
    void ext_blur_manager_v1_destroy_global() override;
    void ext_blur_manager_v1_destroy(Resource *resource) override;
    void ext_blur_manager_v1_get_blur(Resource *resource, uint32_t id, wl_resource *surface) override;
};

class ExtBlurSurfaceV1 : private QtWaylandServer::ext_blur_surface_v1
{
public:
    explicit ExtBlurSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id);
    ~ExtBlurSurfaceV1() override;

private:
    void ext_blur_surface_v1_destroy_resource(Resource *resource) override;
    void ext_blur_surface_v1_destroy(Resource *resource) override;
    void ext_blur_surface_v1_set_region(Resource *resource, wl_resource *region) override;

    QPointer<SurfaceInterface> m_surface;
};

}
