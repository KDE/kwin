#pragma once

#include "core/colorspace.h"
#include "qwayland-server-chrome-color-management.h"

#include <QObject>
#include <QPointer>

namespace KWin
{
class Display;
class Output;
class SurfaceInterface;

class ChromeColorManagementManager : public QObject, private QtWaylandServer::zcr_color_manager_v1
{
    Q_OBJECT
public:
    explicit ChromeColorManagementManager(Display *display, QObject *parent);

private:
    void zcr_color_manager_v1_create_color_space_from_icc(Resource *resource, uint32_t id, int32_t icc) override;
    void zcr_color_manager_v1_create_color_space_from_names(Resource *resource, uint32_t id, uint32_t eotf, uint32_t chromaticity, uint32_t whitepoint) override;
    void zcr_color_manager_v1_create_color_space_from_params(Resource *resource, uint32_t id, uint32_t eotf, uint32_t primary_r_x, uint32_t primary_r_y, uint32_t primary_g_x, uint32_t primary_g_y, uint32_t primary_b_x, uint32_t primary_b_y, uint32_t white_point_x, uint32_t white_point_y) override;
    void zcr_color_manager_v1_get_color_management_output(Resource *resource, uint32_t id, struct ::wl_resource *output) override;
    void zcr_color_manager_v1_get_color_management_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void zcr_color_manager_v1_create_color_space_from_complete_names(Resource *resource, uint32_t id, uint32_t eotf, uint32_t chromaticity, uint32_t whitepoint, uint32_t matrix, uint32_t range) override;
    void zcr_color_manager_v1_create_color_space_from_complete_params(Resource *resource, uint32_t id, uint32_t eotf, uint32_t matrix, uint32_t range, uint32_t primary_r_x, uint32_t primary_r_y, uint32_t primary_g_x, uint32_t primary_g_y, uint32_t primary_b_x, uint32_t primary_b_y, uint32_t white_point_x, uint32_t white_point_y) override;
    void zcr_color_manager_v1_destroy(Resource *resource) override;
};

class ChromeColorManagementOutput : public QObject, private QtWaylandServer::zcr_color_management_output_v1
{
    Q_OBJECT
public:
    explicit ChromeColorManagementOutput(wl_client *client, uint32_t id, Output *output);

private:
    void zcr_color_management_output_v1_destroy_resource(Resource *resource) override;
    void zcr_color_management_output_v1_get_color_space(Resource *resource, uint32_t id) override;
    void zcr_color_management_output_v1_destroy(Resource *resource) override;

    QPointer<Output> m_output;
};

class ChromeColorManagementSurface : private QtWaylandServer::zcr_color_management_surface_v1
{
public:
    explicit ChromeColorManagementSurface(wl_client *client, uint32_t id, SurfaceInterface *surface);
    ~ChromeColorManagementSurface() override;

    void setPreferredColorDescription(const ColorDescription &descr);

private:
    void zcr_color_management_surface_v1_set_alpha_mode(Resource *resource, uint32_t alpha_mode) override;
    void zcr_color_management_surface_v1_set_extended_dynamic_range(Resource *resource, uint32_t value) override;
    void zcr_color_management_surface_v1_set_color_space(Resource *resource, struct ::wl_resource *color_space, uint32_t render_intent) override;
    void zcr_color_management_surface_v1_set_default_color_space(Resource *resource) override;
    void zcr_color_management_surface_v1_destroy(Resource *resource) override;

    const QPointer<SurfaceInterface> m_surface;
    ColorDescription m_preferredDescription = ColorDescription::sRGB;
};

class ChromeColorManagementColorSpace : private QtWaylandServer::zcr_color_space_v1
{
public:
    explicit ChromeColorManagementColorSpace(wl_client *client, uint32_t id, const ColorDescription &colorDescription);

    const ColorDescription colorDescription;

    wl_resource *handle() const;

    static ChromeColorManagementColorSpace *get(wl_resource *resource);

private:
    void zcr_color_space_v1_get_information(Resource *resource) override;
    void zcr_color_space_v1_destroy_resource(Resource *resource) override;
    void zcr_color_space_v1_destroy(Resource *resource) override;
};

class ChromeColorManagementColorSpaceCreator : QtWaylandServer::zcr_color_space_creator_v1
{
public:
    explicit ChromeColorManagementColorSpaceCreator(wl_client *client, uint32_t id, ChromeColorManagementColorSpace *colorspace);
};

}
