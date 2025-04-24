/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "qwayland-server-ext-background-effect-v1.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

namespace KWin
{

class Display;
class SurfaceInterface;

class KWIN_EXPORT ExtBackgroundEffectManagerV1 : public QObject, private QtWaylandServer::ext_background_effect_manager_v1
{
    Q_OBJECT
public:
    explicit ExtBackgroundEffectManagerV1(Display *display, QObject *parent);

    void addBlurCapability();
    void removeBlurCapability();

private:
    void ext_background_effect_manager_v1_bind_resource(Resource *resource) override;
    void ext_background_effect_manager_v1_destroy(Resource *resource) override;
    void ext_background_effect_manager_v1_get_background_effect(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void sendCapabilities();

    QTimer m_removeTimer;
    bool m_supportsBlur = false;
};

class ExtBackgroundEffectSurfaceV1 : private QtWaylandServer::ext_background_effect_surface_v1
{
public:
    explicit ExtBackgroundEffectSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version);
    ~ExtBackgroundEffectSurfaceV1() override;

private:
    void ext_background_effect_surface_v1_destroy_resource(Resource *resource) override;
    void ext_background_effect_surface_v1_destroy(Resource *resource) override;
    void ext_background_effect_surface_v1_set_blur_region(Resource *resource, wl_resource *region) override;

    QPointer<SurfaceInterface> m_surface;
};

}
