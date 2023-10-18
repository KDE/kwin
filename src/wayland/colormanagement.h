#pragma once

#include "libkwineffects/colorspace.h"
#include "qwayland-server-color-management-v1.h"
#include "surface.h"

#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;

class ColorManagerV1Interface : public QObject, public QtWaylandServer::wp_color_manager_v1
{
    Q_OBJECT
public:
    ColorManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~ColorManagerV1Interface() override;

private:
    void wp_color_manager_v1_get_color_management_surface(Resource *res, uint32_t id, wl_resource *surface) override;
    void wp_color_manager_v1_new_parametric_creator(Resource *resource, uint32_t obj) override;
    void wp_color_manager_v1_destroy(Resource *resource) override;
    void wp_color_manager_v1_bind_resource(Resource *resource) override;
};

class ColorManagerParametricCreator : public QObject, public QtWaylandServer::wp_image_description_creator_params_v1
{
    Q_OBJECT
public:
    ColorManagerParametricCreator(wl_client *client, uint32_t id);

private:
    void wp_image_description_creator_params_v1_destroy(Resource *resource) override;
    void wp_image_description_creator_params_v1_destroy_resource(Resource *resource) override;
    void wp_image_description_creator_params_v1_create(Resource *resource, uint32_t image_description) override;
    void wp_image_description_creator_params_v1_set_tf_cicp(Resource *resource, uint32_t tf_code) override;
    void wp_image_description_creator_params_v1_set_tf_power(Resource *resource, uint32_t eexp) override;
    void wp_image_description_creator_params_v1_set_primaries_cicp(Resource *resource, uint32_t primaries_code) override;
    void wp_image_description_creator_params_v1_set_primaries(Resource *resource, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y) override;
    void wp_image_description_creator_params_v1_set_mastering_display_primaries(Resource *resource, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y) override;
    void wp_image_description_creator_params_v1_set_mastering_luminance(Resource *resource, uint32_t min_lum, uint32_t max_lum) override;
    void wp_image_description_creator_params_v1_set_max_cll(Resource *resource, uint32_t maxCLL) override;
    void wp_image_description_creator_params_v1_set_max_fall(Resource *resource, uint32_t maxFALL) override;

    Colorimetry m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT709);
    NamedTransferFunction m_transferFunction;
    double m_sdrBrightness = 100;
    double m_minHdrBrightness = 0;
    double m_maxFrameAverageBrightness = 0;
    double m_maxHdrHighlightBrightness = 0;
};

class ColorManagementDescription : public QObject, public QtWaylandServer::wp_image_description_v1
{
    Q_OBJECT
public:
    ColorManagementDescription(const ColorDescription &desc, wl_client *client, uint32_t id);

    const ColorDescription desc;

    static ColorManagementDescription *get(wl_resource *resource);

private:
    void wp_image_description_v1_get_information(Resource *resource, uint32_t new_id) override;
    void wp_image_description_v1_destroy(Resource *resource) override;
    void wp_image_description_v1_destroy_resource(Resource *resource) override;
};

class ColorManagementSurface : public QObject, public QtWaylandServer::wp_color_management_surface_v1
{
    Q_OBJECT
public:
    ColorManagementSurface(SurfaceInterface *surface, wl_client *client, uint32_t id);
    ~ColorManagementSurface() override;

private:
    void wp_color_management_surface_v1_set_image_description(Resource *resource, wl_resource *image_description, uint32_t render_intent) override;
    void wp_color_management_surface_v1_set_default_image_description(Resource *resource) override;
    void wp_color_management_surface_v1_destroy(Resource *resource) override;
    void wp_color_management_surface_v1_destroy_resource(Resource *resource) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
