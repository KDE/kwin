/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once
#include "core/colorspace.h"

#include "wayland/qwayland-server-xx-color-management-v1.h"
#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;
class Output;

class XXColorManagerV1 : public QObject, private QtWaylandServer::xx_color_manager_v1
{
    Q_OBJECT
public:
    explicit XXColorManagerV1(Display *display, QObject *parent);

private:
    void xx_color_manager_v1_bind_resource(Resource *resource) override;
    void xx_color_manager_v1_destroy(Resource *resource) override;
    void xx_color_manager_v1_get_output(Resource *resource, uint32_t id, struct ::wl_resource *output) override;
    void xx_color_manager_v1_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void xx_color_manager_v1_new_icc_creator(Resource *resource, uint32_t obj) override;
    void xx_color_manager_v1_new_parametric_creator(Resource *resource, uint32_t obj) override;
};

class XXColorSurfaceV1 : private QtWaylandServer::xx_color_management_surface_v1
{
public:
    explicit XXColorSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~XXColorSurfaceV1() override;

    void setPreferredColorDescription(const ColorDescription &descr);

private:
    void xx_color_management_surface_v1_destroy_resource(Resource *resource) override;
    void xx_color_management_surface_v1_destroy(Resource *resource) override;
    void xx_color_management_surface_v1_set_image_description(Resource *resource, struct ::wl_resource *image_description, uint32_t render_intent) override;
    void xx_color_management_surface_v1_set_default_image_description(Resource *resource) override;
    void xx_color_management_surface_v1_get_preferred(Resource *resource, uint32_t image_description) override;

    QPointer<SurfaceInterface> m_surface;
    ColorDescription m_preferred;
};

class XXColorParametricCreatorV1 : private QtWaylandServer::xx_image_description_creator_params_v1
{
public:
    explicit XXColorParametricCreatorV1(wl_client *client, uint32_t id, uint32_t version);

private:
    void xx_image_description_creator_params_v1_destroy_resource(Resource *resource) override;
    void xx_image_description_creator_params_v1_destroy(Resource *resource) override;
    void xx_image_description_creator_params_v1_create(Resource *resource, uint32_t image_description) override;
    void xx_image_description_creator_params_v1_set_tf_named(Resource *resource, uint32_t tf) override;
    void xx_image_description_creator_params_v1_set_tf_power(Resource *resource, uint32_t eexp) override;
    void xx_image_description_creator_params_v1_set_primaries_named(Resource *resource, uint32_t primaries) override;
    void xx_image_description_creator_params_v1_set_primaries(Resource *resource, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y) override;
    void xx_image_description_creator_params_v1_set_mastering_display_primaries(Resource *resource, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y) override;
    void xx_image_description_creator_params_v1_set_mastering_luminance(Resource *resource, uint32_t min_lum, uint32_t max_lum) override;
    void xx_image_description_creator_params_v1_set_max_cll(Resource *resource, uint32_t max_cll) override;
    void xx_image_description_creator_params_v1_set_max_fall(Resource *resource, uint32_t max_fall) override;

    std::optional<Colorimetry> m_colorimetry;
    std::optional<NamedTransferFunction> m_transferFunction;
    std::optional<double> m_maxFrameAverageBrightness;
    std::optional<double> m_maxPeakBrightness;
};

class XXImageDescriptionV1 : private QtWaylandServer::xx_image_description_v1
{
public:
    explicit XXImageDescriptionV1(wl_client *client, uint32_t id, uint32_t version, const ColorDescription &color);

    const ColorDescription &description() const;

    static XXImageDescriptionV1 *get(wl_resource *resource);

private:
    void xx_image_description_v1_destroy_resource(Resource *resource) override;
    void xx_image_description_v1_destroy(Resource *resource) override;
    void xx_image_description_v1_get_information(Resource *resource, uint32_t information) override;

    const ColorDescription m_description;
};

class XXColorManagementOutputV1 : public QObject, private QtWaylandServer::xx_color_management_output_v1
{
    Q_OBJECT
public:
    explicit XXColorManagementOutputV1(wl_client *client, uint32_t id, uint32_t version, Output *output);

private:
    void colorDescriptionChanged();
    void xx_color_management_output_v1_destroy_resource(Resource *resource) override;
    void xx_color_management_output_v1_destroy(Resource *resource) override;
    void xx_color_management_output_v1_get_image_description(Resource *resource, uint32_t image_description) override;

    Output *const m_output;
    ColorDescription m_colorDescription;
};

}
