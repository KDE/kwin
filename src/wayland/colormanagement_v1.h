/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once
#include "core/colorspace.h"

#include "wayland/qwayland-server-color-management-v1.h"
#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;
class Output;

class ColorManagerV1 : public QObject, private QtWaylandServer::wp_color_manager_v1
{
    Q_OBJECT
public:
    explicit ColorManagerV1(Display *display, QObject *parent);

private:
    void wp_color_manager_v1_bind_resource(Resource *resource) override;
    void wp_color_manager_v1_destroy(Resource *resource) override;
    void wp_color_manager_v1_get_output(Resource *resource, uint32_t id, struct ::wl_resource *output) override;
    void wp_color_manager_v1_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void wp_color_manager_v1_get_surface_feedback(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void wp_color_manager_v1_create_icc_creator(Resource *resource, uint32_t obj) override;
    void wp_color_manager_v1_create_parametric_creator(Resource *resource, uint32_t obj) override;
    void wp_color_manager_v1_create_windows_scrgb(Resource *resource, uint32_t image_description) override;
};

class ColorFeedbackSurfaceV1 : private QtWaylandServer::wp_color_management_surface_feedback_v1
{
public:
    explicit ColorFeedbackSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~ColorFeedbackSurfaceV1() override;

    void setPreferredColorDescription(const ColorDescription &descr);

private:
    void wp_color_management_surface_feedback_v1_destroy_resource(Resource *resource) override;
    void wp_color_management_surface_feedback_v1_destroy(Resource *resource) override;
    void wp_color_management_surface_feedback_v1_get_preferred(Resource *resource, uint32_t image_description) override;
    void wp_color_management_surface_feedback_v1_get_preferred_parametric(Resource *resource, uint32_t image_description) override;

    QPointer<SurfaceInterface> m_surface;
    ColorDescription m_preferred;
};

class ColorSurfaceV1 : private QtWaylandServer::wp_color_management_surface_v1
{
public:
    explicit ColorSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~ColorSurfaceV1() override;

private:
    void wp_color_management_surface_v1_destroy_resource(Resource *resource) override;
    void wp_color_management_surface_v1_destroy(Resource *resource) override;
    void wp_color_management_surface_v1_set_image_description(Resource *resource, struct ::wl_resource *image_description, uint32_t render_intent) override;
    void wp_color_management_surface_v1_unset_image_description(Resource *resource) override;

    QPointer<SurfaceInterface> m_surface;
};

class ColorParametricCreatorV1 : private QtWaylandServer::wp_image_description_creator_params_v1
{
public:
    explicit ColorParametricCreatorV1(wl_client *client, uint32_t id, uint32_t version);

private:
    void wp_image_description_creator_params_v1_destroy_resource(Resource *resource) override;
    void wp_image_description_creator_params_v1_create(Resource *resource, uint32_t image_description) override;
    void wp_image_description_creator_params_v1_set_tf_named(Resource *resource, uint32_t tf) override;
    void wp_image_description_creator_params_v1_set_tf_power(Resource *resource, uint32_t eexp) override;
    void wp_image_description_creator_params_v1_set_primaries_named(Resource *resource, uint32_t primaries) override;
    void wp_image_description_creator_params_v1_set_primaries(Resource *resource, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) override;
    void wp_image_description_creator_params_v1_set_mastering_display_primaries(Resource *resource, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) override;
    void wp_image_description_creator_params_v1_set_mastering_luminance(Resource *resource, uint32_t min_lum, uint32_t max_lum) override;
    void wp_image_description_creator_params_v1_set_max_cll(Resource *resource, uint32_t max_cll) override;
    void wp_image_description_creator_params_v1_set_max_fall(Resource *resource, uint32_t max_fall) override;
    void wp_image_description_creator_params_v1_set_luminances(Resource *resource, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) override;

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

class ImageDescriptionV1 : private QtWaylandServer::wp_image_description_v1
{
public:
    explicit ImageDescriptionV1(wl_client *client, uint32_t id, uint32_t version, const std::optional<ColorDescription> &color);

    const std::optional<ColorDescription> &description() const;

    static ImageDescriptionV1 *get(wl_resource *resource);
    static uint64_t s_idCounter;

private:
    void wp_image_description_v1_destroy_resource(Resource *resource) override;
    void wp_image_description_v1_destroy(Resource *resource) override;
    void wp_image_description_v1_get_information(Resource *resource, uint32_t information) override;

    const std::optional<ColorDescription> m_description;
};

class ColorManagementOutputV1 : public QObject, private QtWaylandServer::wp_color_management_output_v1
{
    Q_OBJECT
public:
    explicit ColorManagementOutputV1(wl_client *client, uint32_t id, uint32_t version, Output *output);

private:
    void colorDescriptionChanged();
    void wp_color_management_output_v1_destroy_resource(Resource *resource) override;
    void wp_color_management_output_v1_destroy(Resource *resource) override;
    void wp_color_management_output_v1_get_image_description(Resource *resource, uint32_t image_description) override;

    Output *const m_output;
    ColorDescription m_colorDescription;
};

}
