/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once
#include "core/colorspace.h"

#include "wayland/qwayland-server-xx-color-management-v4.h"
#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;
class Output;

class XXColorManagerV4 : public QObject, private QtWaylandServer::xx_color_manager_v4
{
    Q_OBJECT
public:
    explicit XXColorManagerV4(Display *display, QObject *parent);

private:
    void xx_color_manager_v4_bind_resource(Resource *resource) override;
    void xx_color_manager_v4_destroy(Resource *resource) override;
    void xx_color_manager_v4_get_output(Resource *resource, uint32_t id, struct ::wl_resource *output) override;
    void xx_color_manager_v4_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void xx_color_manager_v4_get_feedback_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void xx_color_manager_v4_new_icc_creator(Resource *resource, uint32_t obj) override;
    void xx_color_manager_v4_new_parametric_creator(Resource *resource, uint32_t obj) override;
};

class XXColorFeedbackSurfaceV4 : private QtWaylandServer::xx_color_management_feedback_surface_v4
{
public:
    explicit XXColorFeedbackSurfaceV4(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~XXColorFeedbackSurfaceV4() override;

    void setPreferredColorDescription(const ColorDescription &descr);

private:
    void xx_color_management_feedback_surface_v4_destroy_resource(Resource *resource) override;
    void xx_color_management_feedback_surface_v4_destroy(Resource *resource) override;
    void xx_color_management_feedback_surface_v4_get_preferred(Resource *resource, uint32_t image_description) override;

    QPointer<SurfaceInterface> m_surface;
    ColorDescription m_preferred;
};

class XXColorSurfaceV4 : private QtWaylandServer::xx_color_management_surface_v4
{
public:
    explicit XXColorSurfaceV4(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~XXColorSurfaceV4() override;

private:
    void xx_color_management_surface_v4_destroy_resource(Resource *resource) override;
    void xx_color_management_surface_v4_destroy(Resource *resource) override;
    void xx_color_management_surface_v4_set_image_description(Resource *resource, struct ::wl_resource *image_description, uint32_t render_intent) override;
    void xx_color_management_surface_v4_unset_image_description(Resource *resource) override;

    QPointer<SurfaceInterface> m_surface;
};

class XXColorParametricCreatorV4 : private QtWaylandServer::xx_image_description_creator_params_v4
{
public:
    explicit XXColorParametricCreatorV4(wl_client *client, uint32_t id, uint32_t version);

private:
    void xx_image_description_creator_params_v4_destroy_resource(Resource *resource) override;
    void xx_image_description_creator_params_v4_create(Resource *resource, uint32_t image_description) override;
    void xx_image_description_creator_params_v4_set_tf_named(Resource *resource, uint32_t tf) override;
    void xx_image_description_creator_params_v4_set_tf_power(Resource *resource, uint32_t eexp) override;
    void xx_image_description_creator_params_v4_set_primaries_named(Resource *resource, uint32_t primaries) override;
    void xx_image_description_creator_params_v4_set_primaries(Resource *resource, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) override;
    void xx_image_description_creator_params_v4_set_mastering_display_primaries(Resource *resource, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) override;
    void xx_image_description_creator_params_v4_set_mastering_luminance(Resource *resource, uint32_t min_lum, uint32_t max_lum) override;
    void xx_image_description_creator_params_v4_set_max_cll(Resource *resource, uint32_t max_cll) override;
    void xx_image_description_creator_params_v4_set_max_fall(Resource *resource, uint32_t max_fall) override;
    void xx_image_description_creator_params_v4_set_luminances(Resource *resource, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) override;

    std::optional<Colorimetry> m_colorimetry;
    std::optional<TransferFunction::Type> m_transferFunctionType;
    struct Luminances
    {
        double min;
        double max;
        double reference;
    };
    std::optional<Luminances> m_transferFunctionLuminances;

    // mastering display information
    std::optional<Colorimetry> m_masteringColorimetry;
    std::optional<double> m_minMasteringLuminance;
    std::optional<double> m_maxMasteringLuminance;
    std::optional<double> m_maxCll;
    std::optional<double> m_maxFall;
};

class XXImageDescriptionV4 : private QtWaylandServer::xx_image_description_v4
{
public:
    explicit XXImageDescriptionV4(wl_client *client, uint32_t id, uint32_t version, const ColorDescription &color);

    const ColorDescription &description() const;

    static XXImageDescriptionV4 *get(wl_resource *resource);

private:
    void xx_image_description_v4_destroy_resource(Resource *resource) override;
    void xx_image_description_v4_destroy(Resource *resource) override;
    void xx_image_description_v4_get_information(Resource *resource, uint32_t information) override;

    const ColorDescription m_description;
};

class XXColorManagementOutputV4 : public QObject, private QtWaylandServer::xx_color_management_output_v4
{
    Q_OBJECT
public:
    explicit XXColorManagementOutputV4(wl_client *client, uint32_t id, uint32_t version, Output *output);

private:
    void colorDescriptionChanged();
    void xx_color_management_output_v4_destroy_resource(Resource *resource) override;
    void xx_color_management_output_v4_destroy(Resource *resource) override;
    void xx_color_management_output_v4_get_image_description(Resource *resource, uint32_t image_description) override;

    Output *const m_output;
    ColorDescription m_colorDescription;
};

}
