/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "wayland/qwayland-server-alpha-modifier-v1.h"

#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;

class AlphaModifierManagerV1 : public QObject, private QtWaylandServer::wp_alpha_modifier_v1
{
public:
    explicit AlphaModifierManagerV1(Display *display, QObject *parent);

private:
    void wp_alpha_modifier_v1_destroy(Resource *resource) override;
    void wp_alpha_modifier_v1_get_surface(Resource *resource, uint32_t id, ::wl_resource *surface) override;
};

class AlphaModifierSurfaceV1 : private QtWaylandServer::wp_alpha_modifier_surface_v1
{
public:
    explicit AlphaModifierSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~AlphaModifierSurfaceV1() override;

private:
    void wp_alpha_modifier_surface_v1_destroy_resource(Resource *resource) override;
    void wp_alpha_modifier_surface_v1_destroy(Resource *resource) override;
    void wp_alpha_modifier_surface_v1_set_multiplier(Resource *resource, uint32_t factor) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
