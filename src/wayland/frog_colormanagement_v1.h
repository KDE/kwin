/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/colorspace.h"
#include "wayland/qwayland-server-frog-color-management-v1.h"

#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;

class FrogColorManagementV1 : public QObject, private QtWaylandServer::frog_color_management_factory_v1
{
    Q_OBJECT
public:
    explicit FrogColorManagementV1(Display *display, QObject *parent);
    ~FrogColorManagementV1() override;

private:
    void frog_color_management_factory_v1_get_color_managed_surface(Resource *resource, wl_resource *surface, uint32_t callback) override;
    void frog_color_management_factory_v1_destroy(Resource *resource) override;
};

class FrogColorManagementSurfaceV1 : public QObject, private QtWaylandServer::frog_color_managed_surface
{
    Q_OBJECT
public:
    explicit FrogColorManagementSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id);
    ~FrogColorManagementSurfaceV1() override;

    void setPreferredColorDescription(const ColorDescription &colorDescription);

private:
    void frog_color_managed_surface_set_known_transfer_function(Resource *resource, uint32_t transfer_function) override;
    void frog_color_managed_surface_set_known_container_color_volume(Resource *resource, uint32_t primaries) override;
    void frog_color_managed_surface_set_render_intent(Resource *resource, uint32_t render_intent) override;
    void frog_color_managed_surface_set_hdr_metadata(Resource *resource, uint32_t mastering_display_primary_red_x, uint32_t mastering_display_primary_red_y,
                                                     uint32_t mastering_display_primary_green_x, uint32_t mastering_display_primary_green_y,
                                                     uint32_t mastering_display_primary_blue_x, uint32_t mastering_display_primary_blue_y,
                                                     uint32_t mastering_white_point_x, uint32_t mastering_white_point_y,
                                                     uint32_t max_display_mastering_luminance, uint32_t min_display_mastering_luminance,
                                                     uint32_t max_cll, uint32_t max_fall) override;
    void frog_color_managed_surface_destroy(Resource *resource) override;
    void frog_color_managed_surface_destroy_resource(Resource *resource) override;
    void updateColorDescription();

    const QPointer<SurfaceInterface> m_surface;
    TransferFunction m_transferFunction{TransferFunction::sRGB};
    NamedColorimetry m_containerColorimetry = NamedColorimetry::BT709;
    std::optional<Colorimetry> m_masteringColorimetry;
    std::optional<double> m_minMasteringLuminance;
    std::optional<double> m_maxAverageLuminance;
    std::optional<double> m_maxPeakBrightness;
};

}
