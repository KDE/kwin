#include "colormanagement.h"
#include "display.h"
#include "surface.h"
#include "surface_p.h"
#include "utils/resource.h"

#include <QDebug>

namespace KWin
{

ColorManagerV1Interface::ColorManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_color_manager_v1(*display, 1)
{
}

ColorManagerV1Interface::~ColorManagerV1Interface() = default;

void ColorManagerV1Interface::wp_color_manager_v1_get_color_management_surface(Resource *res, uint32_t id, wl_resource *surface)
{
    new ColorManagementSurface(SurfaceInterface::get(surface), res->client(), id);
}

void ColorManagerV1Interface::wp_color_manager_v1_new_parametric_creator(Resource *resource, uint32_t obj)
{
    new ColorManagerParametricCreator(resource->client(), obj);
}

void ColorManagerV1Interface::wp_color_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorManagerV1Interface::wp_color_manager_v1_bind_resource(Resource *resource)
{
    send_supported_intent(resource->handle, render_intent_perceptual);
    send_supported_feature(resource->handle, feature_parametric);
    send_supported_feature(resource->handle, feature_set_primaries);
    send_supported_feature(resource->handle, feature_set_mastering_display_primaries);
    send_supported_feature(resource->handle, feature_extended_target_volume);
    send_supported_tf_cicp(resource->handle, 1);
    send_supported_tf_cicp(resource->handle, 4);
    send_supported_tf_cicp(resource->handle, 13);
    send_supported_tf_cicp(resource->handle, 14);
    send_supported_tf_cicp(resource->handle, 15);
    send_supported_tf_cicp(resource->handle, 16);
    send_supported_primaries_cicp(resource->handle, 1);
    send_supported_primaries_cicp(resource->handle, 9);
}

ColorManagerParametricCreator::ColorManagerParametricCreator(wl_client *client, uint32_t id)
    : QtWaylandServer::wp_image_description_creator_params_v1(client, id, 1)
{
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_create(Resource *resource, uint32_t image_description)
{
    new ColorManagementDescription(ColorDescription(m_colorimetry, m_transferFunction, m_sdrBrightness, m_minHdrBrightness, m_maxFrameAverageBrightness, m_maxHdrHighlightBrightness), resource->client(), image_description);
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_set_tf_cicp(Resource *resource, uint32_t tf_code)
{
    if (tf_code == 1 || tf_code == 4 || tf_code == 13) {
        m_transferFunction = NamedTransferFunction::sRGB;
    } else if (tf_code == 16 || tf_code == 14 || tf_code == 15) {
        m_transferFunction = NamedTransferFunction::PerceptualQuantizer;
    } else {
        // TODO scRGB
        qWarning("unknown TF %u", tf_code);
    }
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_set_tf_power(Resource *resource, uint32_t eexp)
{
    qWarning("ENOTSUP power %u", eexp);
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_set_primaries_cicp(Resource *resource, uint32_t primaries_code)
{
    if (primaries_code == 1) {
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT709);
    } else if (primaries_code == 9) {
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT2020);
    } else {
        qWarning("unknown Colorimetry %u", primaries_code);
    }
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_set_primaries(Resource *resource, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y)
{
    m_colorimetry = Colorimetry{
        .red = QVector2D(r_x, r_y) / 10000,
        .green = QVector2D(g_x, g_y) / 10000,
        .blue = QVector2D(b_x, b_y) / 10000,
        .white = QVector2D(w_x, w_y) / 10000,
    };
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_set_mastering_display_primaries(Resource *resource, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y)
{
    qWarning("ignoring mastering primaries");
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_set_mastering_luminance(Resource *resource, uint32_t min_lum, uint32_t max_lum)
{
    qWarning("ignoring mastering luminance");
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_set_max_cll(Resource *resource, uint32_t maxCLL)
{
    if (maxCLL == 0) {
        m_maxHdrHighlightBrightness = 1000;
    } else {
        m_maxHdrHighlightBrightness = maxCLL;
    }
}

void ColorManagerParametricCreator::wp_image_description_creator_params_v1_set_max_fall(Resource *resource, uint32_t maxFALL)
{
    if (maxFALL == 0) {
        m_maxFrameAverageBrightness = 1000;
    } else {
        m_maxFrameAverageBrightness = maxFALL;
    }
}

ColorManagementDescription::ColorManagementDescription(const ColorDescription &desc, wl_client *client, uint32_t id)
    : QtWaylandServer::wp_image_description_v1(client, id, 1)
    , desc(desc)
{
    send_ready(0);
}

ColorManagementDescription *ColorManagementDescription::get(wl_resource *resource)
{
    return resource_cast<ColorManagementDescription *>(resource);
}

void ColorManagementDescription::wp_image_description_v1_get_information(Resource *resource, uint32_t new_id)
{
    auto obj = std::make_unique<QtWaylandServer::wp_image_description_info_v1>(resource->client(), new_id, 1);
    obj->send_primaries(obj->resource()->handle, std::round(desc.colorimetry().red.x() * 10000), std::round(desc.colorimetry().red.y() * 10000),
                        std::round(desc.colorimetry().green.x() * 10000), std::round(desc.colorimetry().green.y() * 10000),
                        std::round(desc.colorimetry().blue.x() * 10000), std::round(desc.colorimetry().blue.y() * 10000),
                        std::round(desc.colorimetry().white.x() * 10000), std::round(desc.colorimetry().white.y() * 10000));
    if (desc.colorimetry().name == NamedColorimetry::BT709) {
        obj->send_primaries_cicp(obj->resource()->handle, 1);
    } else if (desc.colorimetry().name == NamedColorimetry::BT2020) {
        obj->send_primaries_cicp(obj->resource()->handle, 9);
    }
    if (desc.transferFunction() == NamedTransferFunction::sRGB) {
        obj->send_tf_cicp(obj->resource()->handle, 1);
    } else if (desc.transferFunction() == NamedTransferFunction::PerceptualQuantizer) {
        obj->send_tf_cicp(obj->resource()->handle, 14);
    } else {
        // TODO scRGB and linear?
        obj->send_tf_cicp(obj->resource()->handle, 14);
    }
    // TODO what to do with this?
    obj->send_target_luminance(obj->resource()->handle, 0, 0);
    obj->send_target_max_cll(obj->resource()->handle, std::round(desc.maxHdrHighlightBrightness()));
    obj->send_target_max_fall(obj->resource()->handle, std::round(desc.maxFrameAverageBrightness()));
    obj->send_done(obj->resource()->handle);
}

void ColorManagementDescription::wp_image_description_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorManagementDescription::wp_image_description_v1_destroy_resource(Resource *resource)
{
    delete this;
}

ColorManagementSurface::ColorManagementSurface(SurfaceInterface *surface, wl_client *client, uint32_t id)
    : QtWaylandServer::wp_color_management_surface_v1(client, id, 1)
    , m_surface(surface)
{
    SurfaceInterfacePrivate::get(m_surface)->colorManagement = this;
}

ColorManagementSurface::~ColorManagementSurface()
{
    if (m_surface) {
        SurfaceInterfacePrivate *priv = SurfaceInterfacePrivate::get(m_surface);
        priv->colorManagement = nullptr;
        priv->pending->colorDescription = ColorDescription::sRGB;
        priv->pending->colorDescriptionIsSet = true;
    }
}

void ColorManagementSurface::wp_color_management_surface_v1_set_image_description(Resource *resource, wl_resource *image_description, uint32_t render_intent)
{
    ColorManagementDescription *descr = ColorManagementDescription::get(image_description);
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(m_surface);
    surfacePrivate->pending->colorDescription = descr->desc;
    surfacePrivate->pending->colorDescriptionIsSet = true;
}

void ColorManagementSurface::wp_color_management_surface_v1_set_default_image_description(Resource *resource)
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(m_surface);
    surfacePrivate->pending->colorDescription = ColorDescription::sRGB;
    surfacePrivate->pending->colorDescriptionIsSet = true;
}

void ColorManagementSurface::wp_color_management_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorManagementSurface::wp_color_management_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}
}
